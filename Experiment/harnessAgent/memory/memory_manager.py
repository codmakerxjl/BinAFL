import json
import os
from datetime import datetime
from typing import List, Dict, Any

class MemoryManager:
    def __init__(self, memory_dir: str = "memory"):
        self.memory_dir = memory_dir
        self.current_session = None
        self._ensure_memory_dir()
    
    def _ensure_memory_dir(self):
        """确保memory目录存在"""
        if not os.path.exists(self.memory_dir):
            os.makedirs(self.memory_dir)
    
    def start_new_session(self):
        """开始新的对话会话"""
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        self.current_session = f"session_{timestamp}"
        self._save_memory([])
    
    def add_memory(self, role: str, content: str):
        """添加新的对话记忆"""
        if not self.current_session:
            self.start_new_session()
        
        memory = self._load_memory()
        memory.append({
            "role": role,
            "content": content,
            "timestamp": datetime.now().isoformat()
        })
        self._save_memory(memory)
    
    def get_memory(self) -> List[Dict[str, Any]]:
        """获取当前会话的所有记忆"""
        if not self.current_session:
            return []
        return self._load_memory()
    
    def _save_memory(self, memory: List[Dict[str, Any]]):
        """保存记忆到文件"""
        if not self.current_session:
            return
        
        file_path = os.path.join(self.memory_dir, f"{self.current_session}.json")
        with open(file_path, 'w', encoding='utf-8') as f:
            json.dump(memory, f, ensure_ascii=False, indent=2)
    
    def _load_memory(self) -> List[Dict[str, Any]]:
        """从文件加载记忆"""
        if not self.current_session:
            return []
        
        file_path = os.path.join(self.memory_dir, f"{self.current_session}.json")
        if not os.path.exists(file_path):
            return []
        
        with open(file_path, 'r', encoding='utf-8') as f:
            return json.load(f)
    
    def list_sessions(self) -> List[str]:
        """列出所有保存的会话"""
        sessions = []
        for file in os.listdir(self.memory_dir):
            if file.endswith('.json'):
                sessions.append(file[:-5])  # 移除.json后缀
        return sorted(sessions, reverse=True)
    
    def load_session(self, session_name: str):
        """加载指定的会话"""
        if not session_name.endswith('.json'):
            session_name = f"{session_name}.json"
        
        file_path = os.path.join(self.memory_dir, session_name)
        if os.path.exists(file_path):
            self.current_session = session_name[:-5]  # 移除.json后缀
            return self._load_memory()
        return [] 