import configparser
import os
from typing import Any

class ConfigLoader:
    def __init__(self, path: str = "config.ini"):
        self.path = path
        self.parser = configparser.ConfigParser()
        self._config = {}
        self._loaded = False
        self.load()

    def load(self):
        if not os.path.exists(self.path):
            raise FileNotFoundError(f"[配置错误] 找不到配置文件: {self.path}")
        self.parser.read(self.path, encoding='utf-8')
        self._config = {
            section: dict(self.parser.items(section))
            for section in self.parser.sections()
        }
        self._loaded = True

    def get(self, section: str, key: str, fallback: Any = None, cast_type: type = str) -> Any:
        try:
            value = self._config.get(section, {}).get(key, fallback)
            if value is not None and cast_type != str:
                return cast_type(value)
            return value
        except Exception:
            return fallback

    def all(self) -> dict:
        return self._config


# 单例模式，供全局导入使用
_loader_instance: ConfigLoader | None = None

def get_config(section: str = None) -> dict:
    global _loader_instance
    if _loader_instance is None:
        _loader_instance = ConfigLoader()

    if section:
        return _loader_instance._config.get(section, {})
    return _loader_instance.all()

def get_value(section: str, key: str, fallback: Any = None, cast_type: type = str) -> Any:
    global _loader_instance
    if _loader_instance is None:
        _loader_instance = ConfigLoader()
    return _loader_instance.get(section, key, fallback, cast_type)