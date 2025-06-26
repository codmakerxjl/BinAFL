import os
import json
from datetime import datetime
from typing import List, Dict, Optional
import uuid
import logging

# 配置日志记录
logging.basicConfig(
    level=logging.DEBUG,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s',
    handlers=[
        logging.FileHandler('task_manager.log')
    ]
)
logger = logging.getLogger('TaskManager')

class Task:
    def __init__(self, title: str, description: str, parent_id: Optional[str] = None):
        self.id = str(uuid.uuid4())
        self.title = title
        self.description = description
        self.status = "todo"
        self.parent_id = parent_id
        self.children: List[str] = []
        self.related_tasks: List[str] = []  # List of related task IDs
        self.result: Optional[str] = None
        self.created_at = datetime.now().isoformat()
        self.updated_at = self.created_at
        self.depth = 0  # Initialize depth to 0
        logger.debug(f"Created new task: {self.id} - {title}")

    def to_dict(self) -> Dict:
        return {
            "id": self.id,
            "title": self.title,
            "description": self.description,
            "status": self.status,
            "parent_id": self.parent_id,
            "children": self.children,
            "related_tasks": self.related_tasks,  # Add related tasks to dict
            "result": self.result,
            "created_at": self.created_at,
            "updated_at": self.updated_at,
            "depth": self.depth  # Add depth to serialization
        }

    @classmethod
    def from_dict(cls, data: Dict) -> 'Task':
        task = cls(data["title"], data["description"], data["parent_id"])
        task.id = data["id"]
        task.status = data["status"]
        task.children = data["children"]
        task.related_tasks = data.get("related_tasks", [])  # Get related tasks from dict
        task.result = data["result"]
        task.created_at = data["created_at"]
        task.updated_at = data["updated_at"]
        task.depth = data.get("depth", 0)  # Get depth from dict, default to 0
        return task

class TaskManager:
    def __init__(self, tasks_dir: str = "tasks/data"):
        self.tasks_dir = tasks_dir
        self.tasks: Dict[str, Task] = {}
        self._ensure_tasks_dir()
        logger.info(f"Initialized TaskManager with tasks directory: {tasks_dir}")

    def _ensure_tasks_dir(self):
        os.makedirs(self.tasks_dir, exist_ok=True)
        logger.debug(f"Ensured tasks directory exists: {self.tasks_dir}")

    def create_task(self, title: str, description: str, parent_id: Optional[str] = None) -> Task:
        logger.info(f"Creating new task: {title} (parent_id: {parent_id})")
        task = Task(title, description, parent_id)
        if parent_id and parent_id in self.tasks:
            if task.id not in self.tasks[parent_id].children:
                self.tasks[parent_id].children.append(task.id)
                logger.debug(f"Added task {task.id} as child of {parent_id}")
            task.depth = self.tasks[parent_id].depth + 1
            logger.debug(f"Set task depth to {task.depth}")
        self.tasks[task.id] = task
        
        related_tasks = self.find_related_tasks(task)
        logger.debug(f"Found {len(related_tasks)} related tasks for {task.id}")
        for related_task in related_tasks:
            self.add_related_task(task.id, related_task.id)
            self.add_related_task(related_task.id, task.id)
        
        self._save_task(task)
        return task

    def _save_task(self, task: Task):
        task_path = os.path.join(self.tasks_dir, f"{task.id}.json")
        try:
            with open(task_path, 'w', encoding='utf-8') as f:
                json.dump(task.to_dict(), f, ensure_ascii=False, indent=2)
            logger.debug(f"Saved task {task.id} to {task_path}")
        except Exception as e:
            logger.error(f"Error saving task {task.id}: {str(e)}")
            raise

    def load_tasks(self):
        logger.info("Loading tasks from directory")
        for filename in os.listdir(self.tasks_dir):
            if filename.endswith('.json'):
                try:
                    with open(os.path.join(self.tasks_dir, filename), 'r', encoding='utf-8') as f:
                        task_data = json.load(f)
                        task = Task.from_dict(task_data)
                        self.tasks[task.id] = task
                    logger.debug(f"Loaded task {task.id} from {filename}")
                except Exception as e:
                    logger.error(f"Error loading task from {filename}: {str(e)}")

    def update_task_status(self, task_id: str, status: str, result: Optional[str] = None):
        logger.info(f"Updating task {task_id} status to {status}")
        if task_id not in self.tasks:
            logger.error(f"Task with ID {task_id} not found")
            raise ValueError(f"Task with ID {task_id} not found")
            
        task = self.tasks[task_id]
        task.status = status
        if result is not None:
            task.result = result
            logger.debug(f"Updated result for task {task_id}")
            if status == "completed":
                logger.debug(f"Updating related context for completed task {task_id}")
                self.update_task_with_related_context(task_id)
        task.updated_at = datetime.now().isoformat()
        self._save_task(task)
        
        if status == "completed":
            todo_tasks = self.get_todo_tasks()
            if todo_tasks:
                logger.info(f"Next todo task: {todo_tasks[0].id}")
                return todo_tasks[0]
        return None

    def get_task_tree(self, task_id: str) -> Dict:
        task = self.tasks[task_id]
        tree = task.to_dict()
        tree["children"] = [self.get_task_tree(child_id) for child_id in task.children]
        return tree

    def get_root_tasks(self) -> List[Task]:
        return [task for task in self.tasks.values() if task.parent_id is None]

    def get_todo_tasks(self) -> List[Task]:
        return [task for task in self.tasks.values() if task.status == "todo"]

    def get_completed_tasks(self) -> List[Task]:
        return [task for task in self.tasks.values() if task.status == "completed"]

    def generate_task_summary(self, task_id: str) -> str:
        task = self.tasks[task_id]
        summary = f"Task: {task.title}\n"
        summary += f"Status: {task.status}\n"
        if task.result:
            summary += f"Result: {task.result}\n"
        
        if task.children:
            summary += "\nSubtasks:\n"
            for child_id in task.children:
                child_summary = self.generate_task_summary(child_id)
                summary += "\n" + child_summary.replace("\n", "\n  ")
        
        return summary 

    def update_task_parent(self, task_id: str, new_parent_id: str) -> None:
        """Update the parent of a task."""
        if task_id not in self.tasks:
            raise ValueError(f"Task with ID {task_id} not found")
        if new_parent_id not in self.tasks:
            raise ValueError(f"Parent task with ID {new_parent_id} not found")
            
        # Remove task from old parent's children
        old_parent_id = self.tasks[task_id].parent_id
        if old_parent_id and old_parent_id in self.tasks:
            self.tasks[old_parent_id].children.remove(task_id)
            
        # Update task's parent
        self.tasks[task_id].parent_id = new_parent_id
        
        # Add task to new parent's children
        self.tasks[new_parent_id].children.append(task_id)

    def get_tasks_at_depth(self, root_task_id: str, depth: int) -> List[Task]:
        """Get all tasks at a specific depth in the task tree."""
        if root_task_id not in self.tasks:
            raise ValueError(f"Root task with ID {root_task_id} not found")
            
        tasks_at_depth = []
        root_task = self.tasks[root_task_id]
        
        def traverse(task: Task, current_depth: int):
            if current_depth == depth:
                tasks_at_depth.append(task)
            elif current_depth < depth:
                for child_id in task.children:
                    if child_id in self.tasks:
                        traverse(self.tasks[child_id], current_depth + 1)
        
        traverse(root_task, 0)
        return tasks_at_depth

    def generate_task_tree_description(self, root_task_id: str) -> str:
        """Generate a human-readable description of the task tree."""
        if root_task_id not in self.tasks:
            raise ValueError(f"Root task with ID {root_task_id} not found")
            
        description = []
        
        def traverse(task: Task, level: int = 0):
            indent = "  " * level
            status = task.status
            if task.status == "decomposed":
                status = f"decomposed ({len(task.children)} subtasks)"
            
            task_info = f"{indent}ID: {task.id}\n"
            task_info += f"{indent}Title: {task.title}\n"
            task_info += f"{indent}Status: {status}\n"
            if task.result:
                task_info += f"{indent}Result: {task.result}\n"
            
            description.append(task_info)
            
            for child_id in task.children:
                if child_id in self.tasks:
                    traverse(self.tasks[child_id], level + 1)
        
        traverse(self.tasks[root_task_id])
        return "\n".join(description) 

    def add_related_task(self, task_id: str, related_task_id: str) -> None:
        logger.debug(f"Adding related task {related_task_id} to task {task_id}")
        if task_id not in self.tasks:
            logger.error(f"Task with ID {task_id} not found")
            raise ValueError(f"Task with ID {task_id} not found")
        if related_task_id not in self.tasks:
            logger.error(f"Related task with ID {related_task_id} not found")
            raise ValueError(f"Related task with ID {related_task_id} not found")
            
        task = self.tasks[task_id]
        if related_task_id not in task.related_tasks:
            task.related_tasks.append(related_task_id)
            logger.debug(f"Added related task relationship: {task_id} -> {related_task_id}")
            self._save_task(task)

    def get_related_tasks(self, task_id: str) -> List[Task]:
        """Get all related tasks for a given task."""
        if task_id not in self.tasks:
            raise ValueError(f"Task with ID {task_id} not found")
            
        task = self.tasks[task_id]
        return [self.tasks[related_id] for related_id in task.related_tasks if related_id in self.tasks]

    def get_completed_related_tasks(self, task_id: str) -> List[Task]:
        """Get all completed related tasks for a given task."""
        related_tasks = self.get_related_tasks(task_id)
        return [task for task in related_tasks if task.status == "completed"]

    def find_related_tasks(self, task: Task) -> List[Task]:
        logger.debug(f"Finding related tasks for task {task.id}")
        potential_related_tasks = []
        other_tasks = [t for t in self.tasks.values() if t.id != task.id]
        
        for potential_task in other_tasks:
            if self._are_tasks_related(task, potential_task):
                potential_related_tasks.append(potential_task)
                logger.debug(f"Found related task: {potential_task.id}")
        
        return potential_related_tasks

    def _are_tasks_related(self, task1: Task, task2: Task) -> bool:
        """Determine if two tasks are related based on their content."""
        # Combine title and description for analysis
        task1_content = f"{task1.title} {task1.description}".lower()
        task2_content = f"{task2.title} {task2.description}".lower()
        
        # Split content into words and remove common words
        common_words = {'the', 'and', 'or', 'but', 'in', 'on', 'at', 'to', 'for', 'with', 'by', 'from', 'of', 'a', 'an', 'is', 'are', 'was', 'were', 'be', 'been', 'being'}
        task1_words = {word for word in task1_content.split() if word not in common_words and len(word) > 2}
        task2_words = {word for word in task2_content.split() if word not in common_words and len(word) > 2}
        
        # Calculate word overlap
        common_words = task1_words.intersection(task2_words)
        
        # Tasks are considered related if they share significant common words
        # and have similar domain-specific terms
        domain_terms = {
            'user', 'auth', 'authentication', 'profile', 'database', 'schema', 
            'frontend', 'ui', 'component', 'token', 'jwt', 'refresh', 'crud',
            'operation', 'table', 'management'
        }
        task1_domain = {word for word in task1_words if word in domain_terms}
        task2_domain = {word for word in task2_words if word in domain_terms}
        
        # Tasks must share at least one domain term and have at least 2 common words
        # or share at least 3 common words
        return (len(common_words) >= 2 and len(task1_domain.intersection(task2_domain)) > 0) or len(common_words) >= 3

    def update_task_with_related_context(self, task_id: str) -> None:
        """Update a task's result using information from completed related tasks."""
        if task_id not in self.tasks:
            raise ValueError(f"Task with ID {task_id} not found")
            
        task = self.tasks[task_id]
        completed_related = self.get_completed_related_tasks(task_id)
        
        if completed_related:
            # Combine results from completed related tasks
            related_context = "\n".join([
                f"Related task '{rt.title}' result:\n{rt.result}\n"
                for rt in completed_related
            ])
            
            # Update task result with related context
            if task.result:
                task.result = f"{task.result}\n\nRelated task context:\n{related_context}"
            else:
                task.result = f"Related task context:\n{related_context}"
            self._save_task(task) 