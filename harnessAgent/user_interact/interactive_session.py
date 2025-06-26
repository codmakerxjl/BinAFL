# interact/interactive_session.py
import asyncio
from utils.logger import Logger
from agents.run import Runner
from agents.lifecycle import RunHooks
from tasks import TaskManager, TaskAnalyzer
import json

class InteractiveSession:
    def __init__(self, agent_manager, memory_manager, max_depth: int = 3):
        self.agent_manager = agent_manager
        self.memory_manager = memory_manager
        self.memory_manager.start_new_session()
        self.task_manager = TaskManager()
        self.task_analyzer = TaskAnalyzer(
            task_manager=self.task_manager,
            agent_manager=agent_manager,
            max_depth=max_depth
        )

    class TracingHooks(RunHooks):
        def __init__(self, parent):
            self.parent = parent  
            
        async def on_agent_start(self, context, agent):
            Logger.agent_start(agent.name)

        async def on_tool_start(self, context, agent, tool):
            Logger.tool_call(tool.name)

        async def on_tool_end(self, context, agent, tool, result):
            Logger.tool_result(tool.name, result)

        async def on_handoff(self, context, from_agent, to_agent):
            Logger.handoff(from_agent.name, to_agent.name)

    def _display_task_tree(self, task_id: str, level: int = 0):
        """Display a task and its subtasks in a tree format."""
        task = self.task_manager.tasks[task_id]
        indent = "  " * level
        
        # Display current task
        Logger.info(f"{indent}└─ {task.title}")
        Logger.info(f"{indent}   Description: {task.description}")
        Logger.info(f"{indent}   Status: {task.status}")
        if task.result:
            Logger.info(f"{indent}   Result: {task.result}")
        
        # Display subtasks
        for child_id in task.children:
            self._display_task_tree(child_id, level + 1)

    async def start(self):
        tracing_hooks = self.TracingHooks(self)
        Logger.info("File assistant is running! Enter your question (type 'quit' to exit)")
        Logger.info("Available commands:")
        Logger.info("  - 'quit', 'exit', 'q': Exit the program")
        Logger.info("  - 'history': Show conversation history")
        Logger.info("  - 'sessions': List all saved sessions")
        Logger.info("  - 'load <session_name>': Load a specific session")
        Logger.info("  - '-c <n> <task>' or '--complex <n> <task>': Mark a task as complex with max depth n (will be broken down into subtasks)")
        Logger.info("  - '-t' or '--tasks': View task history trees")
        
        while True:
            try:
                user_input = await asyncio.get_event_loop().run_in_executor(
                    None, 
                    lambda: input("\n\033[1;34mYou:\033[0m ")
                )
                
                if user_input.lower() in ['quit', 'exit', 'q']:
                    Logger.warn("Exiting...")
                    break
                
                # 处理特殊命令
                if user_input.lower() == 'history':
                    memory = self.memory_manager.get_memory()
                    if memory:
                        Logger.info("\nConversation History:")
                        for msg in memory:
                            role = msg['role']
                            content = msg['content']
                            timestamp = msg['timestamp']
                            Logger.info(f"[{timestamp}] {role}: {content}")
                    else:
                        Logger.info("No conversation history available.")
                    continue
                
                elif user_input.lower() == 'sessions':
                    sessions = self.memory_manager.list_sessions()
                    if sessions:
                        Logger.info("\nAvailable Sessions:")
                        for session in sessions:
                            Logger.info(f"- {session}")
                    else:
                        Logger.info("No saved sessions available.")
                    continue
                
                elif user_input.lower().startswith('load '):
                    session_name = user_input[5:].strip()
                    memory = self.memory_manager.load_session(session_name)
                    if memory:
                        Logger.info(f"\nLoaded session: {session_name}")
                        for msg in memory:
                            role = msg['role']
                            content = msg['content']
                            timestamp = msg['timestamp']
                            Logger.info(f"[{timestamp}] {role}: {content}")
                    else:
                        Logger.info(f"Session '{session_name}' not found.")
                    continue

                elif user_input.lower() in ['-t', '--tasks']:
                    root_tasks = self.task_manager.get_root_tasks()
                    if root_tasks:
                        Logger.info("\nTask History Trees:")
                        for task in root_tasks:
                            Logger.info("\n" + "="*50)
                            self._display_task_tree(task.id)
                    else:
                        Logger.info("No tasks available.")
                    continue

                # 保存用户输入到记忆
                self.memory_manager.add_memory("user", user_input)
                Logger.request(user_input)

                # 检查是否是复杂任务
                if self.task_analyzer.is_complex_task(user_input):
                    Logger.info("Complex task detected. Breaking down into subtasks...")
                    main_task = await self.task_analyzer.analyze_and_decompose(user_input)
                    if main_task:
                        todo_tasks = self.task_manager.get_todo_tasks()
                        Logger.info(f"\nTask broken down into {len(todo_tasks)} subtasks:")
                        for task in todo_tasks:
                            Logger.info(f"- {task.title}: {task.description}")
                        
                        # 获取已完成的任务
                        completed_tasks = []
                        
                        # Process each subtask
                        for task in todo_tasks:
                            Logger.info(f"\nProcessing subtask: {task.title}")
                            Logger.thinking()
                            
                            # 生成包含上下文的提示词
                            context_prompt = self.task_analyzer._generate_context_prompt(task, completed_tasks)
                            
                            # 将子任务和上下文作为输入传递给大模型
                            result = await Runner.run(
                                starting_agent=self.agent_manager.agent,
                                input=context_prompt,
                                max_turns=1000,
                                hooks=tracing_hooks
                            )
                            
                            # 更新任务状态和结果
                            self.task_manager.update_task_status(
                                task_id=task.id,
                                status="completed",
                                result=result.final_output
                            )
                            
                            # 将完成的任务添加到已完成任务列表
                            completed_tasks.append(task)
                            
                            # 保存助手响应到记忆
                            self.memory_manager.add_memory("assistant", result.final_output)
                            Logger.response(result.final_output)
                        
                        # Process tasks layer by layer
                        current_depth = 1
                        while True:
                            # Process current layer
                            await self.task_analyzer.process_task_layer(main_task.id, current_depth, self.agent_manager)
                            
                            # Check if there are any tasks at the next depth
                            next_layer_tasks = self.task_manager.get_tasks_at_depth(main_task.id, current_depth + 1)
                            if not next_layer_tasks:
                                break
                            
                            current_depth += 1
                        
                        # Generate final summary
                        final_summary = self.task_manager.generate_task_summary(main_task.id)
                        Logger.info("\nTask Summary:")
                        Logger.info(final_summary)
                        continue
                
                # Process the request with large model
                Logger.thinking()
                result = await Runner.run(
                    starting_agent=self.agent_manager.agent,
                    input=user_input,
                    max_turns=1000,
                    hooks=tracing_hooks
                )

                # 保存助手响应到记忆
                self.memory_manager.add_memory("assistant", result.final_output)
                
                # Print the response
                Logger.response(result.final_output)

            except (KeyboardInterrupt, asyncio.CancelledError):
                Logger.interrupt()
                break
            except Exception as e:
                Logger.error(f"An error occurred while processing the request: {str(e)}")
                if 'user_input' in locals():
                    Logger.error(f"Request content with error: {user_input}")