import sys
import os

# Add the parent directory to Python path
sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from tasks.task_manager import TaskManager
from tasks.task_analyzer import TaskAnalyzer

def main():
    # Initialize the task manager and analyzer
    task_manager = TaskManager()
    task_analyzer = TaskAnalyzer(task_manager)

    # Example 1: Simple task
    print("\n=== Example 1: Simple Task ===")
    simple_task = task_manager.create_task(
        title="Simple Task",
        description="This is a simple task without subtasks"
    )
    print(f"Created simple task with ID: {simple_task.id}")
    
    # Update task status
    task_manager.update_task_status(
        task_id=simple_task.id,
        status="completed",
        result="Task completed successfully"
    )
    print("\nTask Summary:")
    print(task_manager.generate_task_summary(simple_task.id))

    # Example 2: Complex task
    print("\n=== Example 2: Complex Task ===")
    complex_input = "-c First, analyze the codebase structure. Then, identify the main components. Finally, suggest improvements."
    
    # Analyze and decompose the complex task
    main_task = task_analyzer.analyze_and_decompose(complex_input)
    if main_task:
        print(f"Created complex task with ID: {main_task.id}")
        
        # Get all todo tasks
        todo_tasks = task_manager.get_todo_tasks()
        print(f"\nNumber of subtasks created: {len(todo_tasks)}")
        
        # Simulate completing each subtask
        for task in todo_tasks:
            result = f"Completed {task.title}: {task.description}"
            task_manager.update_task_status(
                task_id=task.id,
                status="completed",
                result=result
            )
        
        # Generate final summary
        print("\nFinal Task Summary:")
        print(task_manager.generate_task_summary(main_task.id))

if __name__ == "__main__":
    main() 