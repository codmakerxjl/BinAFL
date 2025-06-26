import unittest
import os
import shutil
from .task_manager import TaskManager, Task

class TestTaskRelations(unittest.TestCase):
    def setUp(self):
        # Create temporary test directory
        self.test_dir = "tasks/test_data"
        self.task_manager = TaskManager(tasks_dir=self.test_dir)
        
        # Create test tasks
        self.task1 = self.task_manager.create_task(
            title="Implement User Authentication",
            description="Create login and registration functionality with JWT tokens"
        )
        
        self.task2 = self.task_manager.create_task(
            title="Add User Profile Management",
            description="Implement user profile CRUD operations and JWT token refresh"
        )
        
        self.task3 = self.task_manager.create_task(
            title="Design Database Schema",
            description="Create database tables for users and authentication"
        )
        
        self.task4 = self.task_manager.create_task(
            title="Implement Frontend UI",
            description="Create responsive UI components for the application"
        )

    def tearDown(self):
        # Clean up test directory
        if os.path.exists(self.test_dir):
            shutil.rmtree(self.test_dir)

    def test_related_tasks_creation(self):
        """Test automatic creation of related tasks"""
        # Verify that tasks 1 and 2 should be identified as related (both involve users and JWT)
        related_tasks_1 = self.task_manager.get_related_tasks(self.task1.id)
        related_tasks_2 = self.task_manager.get_related_tasks(self.task2.id)
        
        self.assertIn(self.task2.id, [t.id for t in related_tasks_1])
        self.assertIn(self.task1.id, [t.id for t in related_tasks_2])
        
        # Verify that task 3 should be related to tasks 1 and 2 (involves user database)
        related_tasks_3 = self.task_manager.get_related_tasks(self.task3.id)
        self.assertTrue(
            self.task1.id in [t.id for t in related_tasks_3] or 
            self.task2.id in [t.id for t in related_tasks_3]
        )
        
        # Verify that task 4 should not be related to tasks 1, 2, 3 (completely different domain)
        related_tasks_4 = self.task_manager.get_related_tasks(self.task4.id)
        self.assertNotIn(self.task1.id, [t.id for t in related_tasks_4])
        self.assertNotIn(self.task2.id, [t.id for t in related_tasks_4])
        self.assertNotIn(self.task3.id, [t.id for t in related_tasks_4])

    def test_related_tasks_context(self):
        """Test integration of related task results into context"""
        # Manually add related task relationships to ensure task 3 is related to both tasks 1 and 2
        self.task_manager.add_related_task(self.task3.id, self.task1.id)
        self.task_manager.add_related_task(self.task3.id, self.task2.id)
        
        # Complete some related tasks
        self.task_manager.update_task_status(
            self.task1.id,
            "completed",
            "Implemented JWT authentication with refresh tokens"
        )
        
        self.task_manager.update_task_status(
            self.task2.id,
            "completed",
            "Created user profile management with token refresh support"
        )
        
        # Complete a task that should include context from related tasks
        self.task_manager.update_task_status(
            self.task3.id,
            "completed",
            "Created user and auth tables"
        )
        
        # Verify that task 3's result includes context from related tasks
        task3 = self.task_manager.tasks[self.task3.id]
        self.assertIn("Related task context", task3.result)
        self.assertIn("JWT authentication", task3.result)
        self.assertIn("user profile management", task3.result)

    def test_manual_related_task_addition(self):
        """Test manual addition of related tasks"""
        # Manually add related task
        self.task_manager.add_related_task(self.task1.id, self.task4.id)
        
        # Verify relationship is established
        related_tasks_1 = self.task_manager.get_related_tasks(self.task1.id)
        self.assertIn(self.task4.id, [t.id for t in related_tasks_1])

    def test_completed_related_tasks(self):
        """Test retrieval of completed related tasks"""
        # Complete some tasks
        self.task_manager.update_task_status(
            self.task1.id,
            "completed",
            "Task 1 completed"
        )
        
        self.task_manager.update_task_status(
            self.task2.id,
            "completed",
            "Task 2 completed"
        )
        
        # Get completed related tasks for task 1
        completed_related = self.task_manager.get_completed_related_tasks(self.task1.id)
        self.assertIn(self.task2.id, [t.id for t in completed_related])

    def test_task_relationship_persistence(self):
        """Test persistence of task relationships"""
        # Create some related tasks
        self.task_manager.add_related_task(self.task1.id, self.task2.id)
        
        # Reload task manager
        new_task_manager = TaskManager(tasks_dir=self.test_dir)
        new_task_manager.load_tasks()
        
        # Verify relationships are maintained
        related_tasks = new_task_manager.get_related_tasks(self.task1.id)
        self.assertIn(self.task2.id, [t.id for t in related_tasks])

    def test_task_completion_validation(self):
        """Test task completion and automatic execution of next task"""
        # Complete first task, should return next task to execute
        next_task = self.task_manager.update_task_status(
            self.task1.id,
            "completed",
            "Task 1 completed"
        )
        
        # Verify next task is returned
        self.assertIsNotNone(next_task)
        self.assertEqual(next_task.id, self.task2.id)
        
        # Complete second task
        next_task = self.task_manager.update_task_status(
            self.task2.id,
            "completed",
            "Task 2 completed"
        )
        
        # Verify next task is returned
        self.assertIsNotNone(next_task)
        self.assertEqual(next_task.id, self.task3.id)
        
        # Complete third task
        next_task = self.task_manager.update_task_status(
            self.task3.id,
            "completed",
            "Task 3 completed"
        )
        
        # Verify next task is returned
        self.assertIsNotNone(next_task)
        self.assertEqual(next_task.id, self.task4.id)
        
        # Complete final task
        next_task = self.task_manager.update_task_status(
            self.task4.id,
            "completed",
            "Task 4 completed"
        )
        
        # Verify no more tasks to execute
        self.assertIsNone(next_task)
        
        # Verify all tasks are completed
        todo_tasks = self.task_manager.get_todo_tasks()
        self.assertEqual(len(todo_tasks), 0)

if __name__ == '__main__':
    unittest.main() 