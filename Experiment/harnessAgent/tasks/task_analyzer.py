from typing import List, Dict, Optional, Union
from tasks.task_manager import TaskManager, Task
from agents.run import Runner
from agents.lifecycle import RunHooks
from utils.logger import Logger
import json
import re
import logging

# 配置日志记录
logging.basicConfig(
    level=logging.DEBUG,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s',
    handlers=[
        logging.FileHandler('task_analyzer.log')
    ]
)
logger = logging.getLogger('TaskAnalyzer')

class TaskAnalyzer:
    def __init__(self, task_manager: TaskManager, agent_manager=None, max_depth: int = 3):
        """
        Initialize the TaskAnalyzer.
        
        Args:
            task_manager: The TaskManager instance
            agent_manager: The agent manager instance (optional)
            max_depth: Maximum depth for task decomposition (default: 3)
        """
        self.task_manager = task_manager
        self.agent_manager = agent_manager
        self.max_depth = max_depth
        logger.info(f"Initialized TaskAnalyzer with max_depth={max_depth}")

    def is_complex_task(self, user_input: str) -> bool:
        """Check if the input is marked as a complex task."""
        logger.debug(f"Checking if task is complex: {user_input}")
        # 检查是否包含明确的复杂任务标记
        if user_input.startswith("-c ") or user_input.startswith("--complex "):
            logger.debug("Task marked as complex with explicit flag")
            return True
            
        # 检查是否包含暗示需要进一步分解的关键词
        decomposition_keywords = [
            "需要进一步分解",
            "可以细分为",
            "应该分为",
            "需要分为",
            "可以分解为",
            "应该分解为",
            "需要分解为",
            "可以细化为",
            "应该细化为",
            "需要细化为"
        ]
        
        is_complex = any(keyword in user_input for keyword in decomposition_keywords)
        if is_complex:
            logger.debug("Task marked as complex based on keywords")
        return is_complex

    def extract_task_content(self, user_input: str) -> tuple[str, int]:
        """Extract the actual task content and max depth from the input."""
        logger.debug(f"Extracting task content from: {user_input}")
        if user_input.startswith("-c "):
            parts = user_input[3:].strip().split(" ", 1)
            if len(parts) == 2 and parts[0].isdigit():
                logger.debug(f"Extracted depth {parts[0]} and content from -c flag")
                return parts[1], int(parts[0])
            logger.debug("Extracted content from -c flag with default depth")
            return user_input[3:].strip(), self.max_depth
        elif user_input.startswith("--complex "):
            parts = user_input[10:].strip().split(" ", 1)
            if len(parts) == 2 and parts[0].isdigit():
                logger.debug(f"Extracted depth {parts[0]} and content from --complex flag")
                return parts[1], int(parts[0])
            logger.debug("Extracted content from --complex flag with default depth")
            return user_input[10:].strip(), self.max_depth
        logger.debug("No special flags found, using default depth")
        return user_input.strip(), self.max_depth

    def _format_next_steps(self, next_steps: Union[str, List[str]]) -> str:
        """Format next_steps into a string format."""
        if isinstance(next_steps, list):
            return "\n".join(str(step) for step in next_steps)
        return str(next_steps)

    async def analyze_and_decompose(self, user_input: str, current_depth: int = 0, parent_id: Optional[str] = None) -> Optional[Task]:
        """Analyze the input and decompose it into subtasks if it's a complex task."""
        logger.info(f"Analyzing task at depth {current_depth}: {user_input}")
        if not self.is_complex_task(user_input):
            logger.debug("Task is not complex, skipping decomposition")
            return None

        # Extract task content and max depth
        task_content, max_depth = self.extract_task_content(user_input)
        logger.debug(f"Extracted content: {task_content}, max_depth: {max_depth}")
        
        # Check if we've reached the maximum depth
        if current_depth >= max_depth:
            logger.info(f"Maximum task decomposition depth ({max_depth}) reached")
            return None
        
        # Create the main task with different logic based on depth
        if current_depth == 0:
            logger.debug("Creating root level task")
            main_task = self.task_manager.create_task(
                title=task_content,
                description="root task"
            )
        else:
            logger.debug(f"Creating sub-level task at depth {current_depth}")
            main_task = self.task_manager.create_task(
                title=task_content,
                description=f"Subtask at depth {current_depth}",
                parent_id=parent_id
            )
        
        # Save the max depth to the task
        main_task.max_depth = max_depth
        logger.debug(f"Set max_depth={max_depth} for task {main_task.id}")

        # Use LLM to break down the task
        logger.info("Breaking down task using LLM")
        subtasks = await self._break_down_task_with_llm(task_content)
        
        # Create subtasks and store them
        created_subtasks = []
        for subtask in subtasks:
            logger.debug(f"Creating subtask: {subtask['title']}")
            subtask_obj = self.task_manager.create_task(
                title=subtask["title"],
                description=subtask["description"],
                parent_id=main_task.id
            )
            created_subtasks.append(subtask_obj)
        
        logger.info(f"Created {len(created_subtasks)} subtasks for task: {main_task.title}")
        return main_task

    def _extract_json_from_response(self, response: str) -> Optional[List[Dict]]:
        """Extract JSON from LLM response, handling various response formats."""
        logger.debug("Attempting to extract JSON from LLM response")
        try:
            # First try direct JSON parsing
            result = json.loads(response)
            logger.debug("Successfully parsed JSON directly")
            return result
        except json.JSONDecodeError:
            logger.debug("Direct JSON parsing failed, trying to find JSON in response")
            # Try to find JSON in the response
            json_match = re.search(r'\[[\s\S]*\]', response)
            if json_match:
                try:
                    result = json.loads(json_match.group())
                    logger.debug("Successfully extracted and parsed JSON array")
                    return result
                except json.JSONDecodeError:
                    logger.debug("Failed to parse extracted JSON array")
            
            # Try to extract and parse each object separately
            objects = re.findall(r'\{[\s\S]*?\}', response)
            if objects:
                try:
                    result = [json.loads(obj) for obj in objects]
                    logger.debug(f"Successfully parsed {len(result)} separate JSON objects")
                    return result
                except json.JSONDecodeError:
                    logger.debug("Failed to parse separate JSON objects")
            
            logger.warning("Could not extract valid JSON from response")
            return None

    async def _break_down_task_with_llm(self, task_content: str) -> List[Dict[str, str]]:
        """
        Use LLM to intelligently break down a complex task into smaller subtasks.
        """
        logger.info(f"Breaking down task with LLM: {task_content}")
        if not self.agent_manager:
            logger.info("No agent manager provided, using simple task decomposition")
            return self._break_down_task_simple(task_content)

        # Create a prompt for the LLM
        prompt = f"""Please analyze the following task and break it down into smaller, manageable subtasks.
For each subtask, provide a clear title and description.

Task: {task_content}

Please format your response as a JSON array of objects, where each object has:
- title: A short, descriptive title for the subtask
- description: A detailed description of what needs to be done
- next_steps: The steps that need to be taken after completing this subtask (if any)

Example format:
[
    {{
        "title": "Initial Analysis",
        "description": "Analyze the current system architecture",
        "next_steps": "Need to break down into: 1. Frontend architecture analysis 2. Backend architecture analysis 3. Database design analysis"
    }},
    {{
        "title": "Component Identification",
        "description": "Identify all major components and their relationships",
        "next_steps": null
    }}
]

Please ensure the subtasks are:
1. Logically ordered
2. Independent where possible
3. Clear and specific
4. Cover all aspects of the main task

If a task needs further decomposition, please specify the steps in the next_steps field.
Please return valid JSON only, no additional text or explanation."""

        try:
            logger.debug("Sending prompt to LLM")
            # Use the agent to get the task breakdown
            result = await Runner.run(
                starting_agent=self.agent_manager.agent,
                input=prompt,
                max_turns=1000,
                hooks=self.agent_manager.agent.hooks
            )

            # Log the raw response for debugging
            logger.debug(f"Raw LLM response: {result.final_output}")

            # Try to extract and parse JSON from the response
            subtasks = self._extract_json_from_response(result.final_output)
            
            if not subtasks:
                logger.error("Could not extract valid JSON from LLM response")
                raise ValueError("Could not extract valid JSON from LLM response")
            
            # Validate the response format
            if not isinstance(subtasks, list):
                logger.error("LLM response is not a list")
                raise ValueError("LLM response is not a list")
            
            for subtask in subtasks:
                if not isinstance(subtask, dict):
                    logger.error("Invalid subtask format: not a dictionary")
                    raise ValueError("Invalid subtask format: not a dictionary")
                if 'title' not in subtask:
                    logger.error("Invalid subtask format: missing 'title'")
                    raise ValueError("Invalid subtask format: missing 'title'")
                if 'description' not in subtask:
                    logger.error("Invalid subtask format: missing 'description'")
                    raise ValueError("Invalid subtask format: missing 'description'")
            
            logger.info(f"Successfully extracted {len(subtasks)} subtasks from LLM response")
            return subtasks

        except Exception as e:
            logger.error(f"Error using LLM for task decomposition: {str(e)}")
            logger.info("Falling back to simple task decomposition")
            return self._break_down_task_simple(task_content)

    def _break_down_task_simple(self, task_content: str) -> List[Dict[str, str]]:
        """
        Simple heuristic-based task breakdown as a fallback.
        """
        logger.info("Using simple task decomposition")
        sentences = [s.strip() for s in task_content.split('.') if s.strip()]
        subtasks = []

        for i, sentence in enumerate(sentences):
            # 检查是否包含下一步骤
            next_steps = None
            if "next steps" in sentence.lower() or "下一步" in sentence:
                next_steps = sentence
                sentence = sentences[i-1] if i > 0 else sentence
                logger.debug(f"Found next steps in sentence {i}: {next_steps}")

            subtask = {
                "title": f"Step {i+1}",
                "description": sentence,
                "next_steps": next_steps
            }
            subtasks.append(subtask)
            logger.debug(f"Created simple subtask {i+1}: {sentence}")

        logger.info(f"Created {len(subtasks)} simple subtasks")
        return subtasks

    def _task_to_dict(self, task: Task) -> dict:
        """Convert a Task object to a dictionary for serialization."""
        return {
            "id": task.id,
            "title": task.title,
            "description": task.description,
            "status": task.status,
            "result": task.result,
            "parent_id": task.parent_id,
            "is_decomposed": getattr(task, 'is_decomposed', False)
        }

    def _generate_context_prompt(self, current_task: Task, completed_tasks: List[Task]) -> str:
        """Generate a prompt that includes context from completed tasks."""
        prompt = f"""You are working on a complex task that has been broken down into subtasks.
Here are the results of previously completed subtasks:

"""
        for task in completed_tasks:
            task_dict = self._task_to_dict(task)
            prompt += f"\nCompleted Task: {task_dict['title']}\n"
            prompt += f"Description: {task_dict['description']}\n"
            prompt += f"Result: {task_dict['result']}\n"

        task_dict = self._task_to_dict(current_task)
        prompt += f"\nNow, please work on the following subtask:\n"
        prompt += f"Title: {task_dict['title']}\n"
        prompt += f"Description: {task_dict['description']}\n\n"
        prompt += "Please consider the results of previous subtasks when working on this task. "
        prompt += "Your response should build upon and complement the previous results."

        return prompt

    def _parse_llm_response_to_json(self, response: str) -> Optional[Union[Dict, List]]:
        """Parse LLM's string response into proper JSON format."""
        try:
            # First try direct JSON parsing
            return json.loads(response)
        except json.JSONDecodeError:
            # Try to extract JSON from markdown code blocks
            try:
                # Remove markdown code block markers if present
                response = re.sub(r'^```(?:json)?\s*', '', response)
                response = re.sub(r'\s*```$', '', response)
                
                # Try parsing the cleaned response
                return json.loads(response)
            except json.JSONDecodeError:
                # Try to extract JSON-like structure from the response
                try:
                    # Find the first [ or {
                    start_idx = min(
                        response.find('[') if response.find('[') != -1 else float('inf'),
                        response.find('{') if response.find('{') != -1 else float('inf')
                    )
                    if start_idx == float('inf'):
                        return None
                    response = response[start_idx:]
                    
                    # Find the matching closing bracket or brace
                    stack = []
                    for i, char in enumerate(response):
                        if char in '[{':
                            stack.append(char)
                        elif char in ']}':
                            if not stack:
                                continue
                            if (char == ']' and stack[-1] == '[') or (char == '}' and stack[-1] == '{'):
                                stack.pop()
                                if not stack:  # Found the matching closing bracket/brace
                                    response = response[:i + 1]
                                    break
                    
                    # Replace single quotes with double quotes
                    response = response.replace("'", '"')
                    
                    # Fix common JSON formatting issues
                    response = response.replace('None', 'null')
                    response = response.replace('True', 'true')
                    response = response.replace('False', 'false')
                    
                    # Add quotes to unquoted keys
                    response = re.sub(r'([{,])\s*([a-zA-Z_][a-zA-Z0-9_]*)\s*:', r'\1"\2":', response)
                    
                    # Try parsing the cleaned response
                    return json.loads(response)
                except Exception as e:
                    Logger.error(f"Failed to parse LLM response: {str(e)}")
                    Logger.error(f"Original response: {response}")
                    return None

    async def process_task_layer(self, root_task_id: str, current_depth: int, agent_manager) -> None:
        """Process all tasks at a specific depth and handle their decomposition."""
        # Get the max depth from the root task
        root_task = self.task_manager.tasks[root_task_id]
        max_depth = getattr(root_task, 'max_depth', self.max_depth)
        
        # Create hooks for tracing
        class TracingHooks(RunHooks):
            async def on_agent_start(self, context, agent):
                Logger.agent_start(agent.name)

            async def on_tool_start(self, context, agent, tool):
                Logger.tool_call(tool.name)

            async def on_tool_end(self, context, agent, tool, result):
                Logger.tool_result(tool.name, result)

            async def on_handoff(self, context, from_agent, to_agent):
                Logger.handoff(from_agent.name, to_agent.name)
        
        tracing_hooks = TracingHooks()
        
        while current_depth < max_depth:
            Logger.info(f"\nProcessing tasks at depth {current_depth} (max depth: {max_depth})")
            
            # Get all tasks at current depth
            current_layer_tasks = self.task_manager.get_tasks_at_depth(root_task_id, current_depth)
            
            # Process each task in the current layer
            for task in current_layer_tasks:
                if task.status == "todo":
                    Logger.info(f"\nProcessing task: {task.title}")
                    Logger.thinking()
                    
                    # Generate context prompt
                    context_prompt = self._generate_context_prompt(task, self.task_manager.get_completed_tasks())
                    
                    # Process the task with LLM
                    result = await Runner.run(
                        starting_agent=agent_manager.agent,
                        input=context_prompt,
                        max_turns=1000,
                        hooks=tracing_hooks
                    )
                    
                    # Update task status and result
                    self.task_manager.update_task_status(
                        task_id=task.id,
                        status="completed",
                        result=result.final_output
                    )
                    
                    # Print the response
                    Logger.response(result.final_output)
            
            # Check if all tasks in current layer are completed
            if all(task.status == "completed" for task in current_layer_tasks):
                Logger.info(f"\nAll tasks at depth {current_depth} completed. Checking for tasks that need further decomposition...")
                
                # Filter out tasks that have already been decomposed
                decomposable_tasks = [
                    task for task in current_layer_tasks
                    if not getattr(task, 'is_decomposed', False)
                ]
                
                if not decomposable_tasks:
                    Logger.info(f"No tasks available for decomposition at depth {current_depth}.")
                    break
                
                # Generate task description for current layer tasks only
                current_layer_description = "\n".join([
                    f"Task ID: {task.id}\n"
                    f"Title: {task.title}\n"
                    f"Description: {task.description}\n"
                    f"Result: {task.result}\n"
                    for task in decomposable_tasks
                ])
                
                # Let LLM decide which tasks need further decomposition
                decomposition_prompt = f"""Please analyze the following completed tasks and determine which ones need further decomposition.
Only consider the tasks listed below, do not include any parent tasks or tasks from other layers.

Completed Tasks:
{current_layer_description}

Your task is to identify which of these tasks need to be broken down into smaller, more manageable subtasks.
For each task that needs decomposition:
1. Provide a clear reason why it needs to be broken down
2. Specify the next steps or subtasks that should be created
3. Ensure the decomposition makes the task more manageable and clearer

Please return a JSON object with the following format:
{{
    "tasks_to_decompose": [
        {{
            "task_id": "task_id",
            "reason": "reason for decomposition",
            "next_steps": ["step1", "step2", "step3"]  # Can be a list of steps or a single string
        }}
    ]
}}

Important guidelines:
1. If a task is complex or has multiple aspects, it should be decomposed
2. If a task's result contains next_steps, it should be decomposed
3. If a task's description suggests multiple steps or components, it should be decomposed
4. Each task in tasks_to_decompose must have a valid task_id from the list above
5. The next_steps should be specific and actionable
6. Return ONLY the JSON object, no additional text or explanation
7. Make sure the JSON is properly formatted with double quotes for keys and string values

Remember: The goal is to break down complex tasks into smaller, more manageable pieces."""

                Logger.info("\nAnalyzing tasks for further decomposition...")
                Logger.thinking()
                decomposition_result = await Runner.run(
                    starting_agent=agent_manager.agent,
                    input=decomposition_prompt,
                    max_turns=1000,
                    hooks=tracing_hooks
                )
                Logger.response(decomposition_result.final_output)
                
                try:
                    # Parse LLM's response into JSON
                    decomposition_decision = self._parse_llm_response_to_json(decomposition_result.final_output)
                    if not decomposition_decision:
                        raise ValueError("Failed to parse LLM's response into JSON")
                    
                    # Handle both dictionary and list responses
                    if isinstance(decomposition_decision, list):
                        tasks_to_decompose = decomposition_decision
                    elif isinstance(decomposition_decision, dict):
                        tasks_to_decompose = decomposition_decision.get("tasks_to_decompose", [])
                    else:
                        raise ValueError("Response is neither a list nor a dictionary")
                    
                    if not isinstance(tasks_to_decompose, list):
                        raise ValueError("tasks_to_decompose is not a list")
                    
                    # Filter tasks_to_decompose to only include tasks from current layer
                    current_layer_task_ids = {task.id for task in decomposable_tasks}
                    tasks_to_decompose = [
                        task_info for task_info in tasks_to_decompose
                        if task_info.get("task_id") in current_layer_task_ids
                    ]
                    
                    # Track if any tasks were decomposed in this iteration
                    tasks_decomposed = False
                    new_subtasks = []
                    
                    for task_info in tasks_to_decompose:
                        if not isinstance(task_info, dict):
                            Logger.warn(f"Invalid task info format: {task_info}")
                            continue
                            
                        task_id = task_info.get("task_id")
                        if not task_id or task_id not in self.task_manager.tasks:
                            Logger.warn(f"Invalid or missing task_id: {task_id}")
                            continue
                            
                        task = self.task_manager.tasks[task_id]
                        Logger.info(f"\nDecomposing task: {task.title}")
                        Logger.info(f"Reason: {task_info.get('reason', 'No reason provided')}")
                        
                        # Use next_steps from task result or LLM's decision
                        next_steps = task_info.get("next_steps")
                        if not next_steps and task.result:
                            try:
                                result_data = self._parse_llm_response_to_json(task.result)
                                if result_data:
                                    if isinstance(result_data, dict) and "next_steps" in result_data:
                                        next_steps = result_data["next_steps"]
                                    elif isinstance(result_data, list):
                                        next_steps = result_data
                            except Exception:
                                if "next steps" in task.result.lower():
                                    next_steps = task.result
                        
                        if next_steps:
                            # Format next_steps into a string
                            formatted_next_steps = self._format_next_steps(next_steps)
                            
                            # Create a complex task input with the next steps
                            complex_task_input = f"-c {formatted_next_steps}"
                            
                            # Decompose the task
                            decomposed_task = await self.analyze_and_decompose(
                                complex_task_input,
                                current_depth=current_depth,
                                parent_id=task.id
                            )
                            
                            if decomposed_task and decomposed_task.children:
                                # Update parent task status and mark as decomposed
                                self.task_manager.update_task_status(
                                    task_id=task.id,
                                    status="decomposed",
                                    result=f"Task has been decomposed into {len(decomposed_task.children)} subtasks based on next steps"
                                )
                                # Mark task as decomposed
                                task.is_decomposed = True
                                # Update parent-child relationship
                                self.task_manager.update_task_parent(decomposed_task.id, task.id)
                                # Add new subtasks to the list
                                new_subtasks.extend([self.task_manager.tasks[child_id] for child_id in decomposed_task.children])
                                tasks_decomposed = True
                                Logger.info(f"Successfully decomposed task into {len(decomposed_task.children)} subtasks")
                            else:
                                Logger.warn(f"Failed to decompose task: {task.title}")
                        else:
                            Logger.warn(f"No next steps found for task: {task.title}")
                    
                    # Process newly created subtasks immediately
                    if new_subtasks:
                        Logger.info(f"\nProcessing {len(new_subtasks)} newly created subtasks...")
                        for subtask in new_subtasks:
                            if subtask.status == "todo":
                                Logger.info(f"\nProcessing subtask: {subtask.title}")
                                Logger.thinking()
                                
                                # Generate context prompt for the subtask
                                context_prompt = self._generate_context_prompt(subtask, self.task_manager.get_completed_tasks())
                                
                                # Process the subtask with LLM
                                result = await Runner.run(
                                    starting_agent=agent_manager.agent,
                                    input=context_prompt,
                                    max_turns=1000,
                                    hooks=tracing_hooks
                                )
                                
                                # Update subtask status and result
                                self.task_manager.update_task_status(
                                    task_id=subtask.id,
                                    status="completed",
                                    result=result.final_output
                                )
                    else:
                        Logger.info("No new subtasks were created in this iteration")
                    
                    # If no tasks were decomposed in this iteration, break the loop
                    if not tasks_decomposed:
                        Logger.info(f"No tasks were decomposed at depth {current_depth}.")
                        break
                    
                    # Increment depth for next iteration
                    current_depth += 1
                    
                except Exception as e:
                    Logger.error(f"Error processing decomposition decision: {str(e)}")
                    Logger.error(f"Raw response: {decomposition_result.final_output}")
                    break
            else:
                # If not all tasks are completed, break the loop
                break 