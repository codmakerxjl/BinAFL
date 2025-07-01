import socket
import sys
import time
import json
from typing import Any
from mcp.server.fastmcp import FastMCP
import os


# Initialize FastMCP server
mcp = FastMCP("BinAgentServer")
import socket
import json

class CppConnector:
    """
    一个管理与C++服务器持久连接的类，具有独立的 send 和 receive 方法。
    """
    def __init__(self, host: str = "127.0.0.1", port: int = 22222):
        self.host = host
        self.port = port
        self.sock = None  # 初始化时没有socket连接

    def connect(self) -> bool:
        """建立到服务器的连接。"""
        try:
            if self.sock:
                self.disconnect()
            
            print(f"正在连接到 {self.host}:{self.port}...")
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            # 设置一个合理的超时时间，避免在receive时无限期阻塞
            self.sock.settimeout(10.0) 
            self.sock.connect((self.host, self.port))
            print("连接成功！")
            return True
        except Exception as e:
            print(f"连接失败: {e}")
            self.sock = None
            return False

    def disconnect(self):
        """关闭连接。"""
        if self.sock:
            print("正在关闭连接...")
            self.sock.close()
            self.sock = None
            print("连接已关闭。")

    def send(self, message: str) -> bool:
        """
        通过已建立的连接发送一条消息。
        
        Returns:
            bool: True表示发送成功，False表示失败。
        """
        if not self.sock:
            print("错误：未连接到服务器。")
            return False
        
        try:
            print(f"Python -> C++: {message}")
            self.sock.sendall(message.encode('utf-8'))
            return True
        except Exception as e:
            print(f"发送时发生错误: {e}")
            self.disconnect() # 发送出错通常意味着连接已断开
            return False

    def receive(self, buffer_size: int = 1024) -> str | None:
        """
        从已建立的连接接收数据。这是一个阻塞操作。
        
        Returns:
            str: 接收到的数据字符串。
            None: 如果连接已关闭或发生错误。
        """
        if not self.sock:
            print("错误：未连接到服务器。")
            return None
        
        try:
            data = self.sock.recv(buffer_size)
            if data:
                response = data.decode('utf-8')
                print(f"Python <- C++: {response}")
                return response
            else:
                # 如果recv返回空，说明服务器端正常关闭了连接
                print("服务器已主动断开连接。")
                self.disconnect()
                return None
        except socket.timeout:
            print("接收数据超时。")
            return None
        except Exception as e:
            print(f"接收时发生错误: {e}")
            self.disconnect()
            return None

    # with 语句管理依然保留，非常方便
    def __enter__(self):
        self.connect()
        return self
    
    def __exit__(self, exc_type, exc_val, exc_tb):
        self.disconnect()


connector_instance: CppConnector | None = None

@mcp.tool()
def connect_cpp(port: int = 22222) -> str:
    """
    创建一个到C++服务器的持久连接。
    如果已有连接，会先断开旧的再建立新的。
    args:
        port (int): C++服务器监听的端口号，默认为22222。
    """
    global connector_instance
    connector_instance = CppConnector(port=port)
    if connector_instance.connect():
        return "成功连接到C++服务器。"
    else:
        return "连接C++服务器失败。"

@mcp.tool()
def send_to_cpp(message: str) -> str:
    """
    通过已建立的连接向C++服务器发送一条消息。
    """
    global connector_instance
    if not connector_instance:
        return "错误：请先调用 connect_cpp 建立连接。"
    
    if connector_instance.send(message):
        return f"消息 '{message}' 已发送。"
    else:
        return "发送失败，连接可能已断开。"

@mcp.tool()
def receive_from_cpp() -> str:
    """
    从C++服务器接收一条消息。这是一个阻塞操作，会等待直到收到数据或超时。
    """
    global connector_instance
    if not connector_instance:
        return "错误：请先调用 connect_cpp 建立连接。"
        
    response = connector_instance.receive()
    if response is not None:
        return f"收到回复: {response}"
    else:
        return "未能收到回复，可能已超时或连接已断开。"

@mcp.tool()
def disconnect_cpp() -> str:
    """
    断开与C++服务器的连接。
    """
    global connector_instance
    if not connector_instance:
        return "信息：当前没有活动的连接。"
    
    connector_instance.disconnect()
    connector_instance = None # 清理实例
    return "已成功断开与C++服务器的连接。"

@mcp.tool()
def read_file_content(file_path: str) -> str:
    """
    读取指定文件的全部内容并作为字符串返回。

    这个函数会自动处理文件的打开和关闭，并包含了常见的错误处理。

    Args:
        file_path (str): 要读取的文件的完整路径。

    Returns:
        str: 文件的内容。如果读取过程中发生错误，则返回一条描述错误的字符串。
    """
    try:
        # 使用 'with' 语句是最佳实践，它能确保文件在操作完成后被自动关闭。
        # 指定 encoding='utf-8' 是一个好习惯，可以避免在不同操作系统上出现乱码问题。
        with open(file_path, 'r', encoding='utf-8') as file:
            content = file.read()
            return content
            
    except FileNotFoundError:
        return f"错误：找不到文件 '{file_path}'"
        
    except PermissionError:
        return f"错误：没有权限读取文件 '{file_path}'"
        
    except UnicodeDecodeError:
        return f"错误：文件 '{file_path}' 的编码格式不是 UTF-8，无法正确解码。"
        
    except Exception as e:
        return f"读取文件时发生了一个未知错误: {e}"

@mcp.tool()
def write_content_to_file(path: str, filename: str, content: str) -> str:
    """
    将字符串内容写入到指定文件夹的指定文件中。

    这个函数会自动检查文件夹是否存在，如果不存在，则会创建所有必需的中间文件夹。
    如果文件已存在，其内容将会被新内容覆盖。

    Args:
        path (str): 目标文件夹的路径 (例如: "C:/my_output", "data/results")。
        filename (str): 要写入的文件的名称 (例如: "report.txt", "log_01.json")。
        content (str): 要写入文件的字符串内容。

    Returns:
        str: 一条描述操作结果（成功或失败）的消息。
    """
    try:
        # 步骤 1: 确保目标文件夹存在。
        # os.makedirs() 可以创建多层嵌套的文件夹。
        # exist_ok=True 参数使得在文件夹已经存在时，函数不会报错。
        os.makedirs(path, exist_ok=True)
        print(f"确认文件夹 '{path}' 已存在。")

        # 步骤 2: 将文件夹路径和文件名安全地拼接成完整路径。
        # os.path.join() 会根据你的操作系统使用正确的路径分隔符（'/' 或 '\'）。
        full_path = os.path.join(path, filename)
        
        # 步骤 3: 写入文件。
        # 使用 'w' 模式：如果文件不存在则创建；如果文件存在则覆盖。
        # 使用 encoding='utf-8' 来确保中文字符或其他特殊字符能被正确写入。
        with open(full_path, 'w', encoding='utf-8') as file:
            file.write(content)
        
        success_message = f"内容已成功写入到文件: '{full_path}'"
        print(success_message)
        return success_message

    except PermissionError:
        error_message = f"错误：权限不足，无法在 '{path}' 中创建文件夹或写入文件。"
        print(error_message)
        return error_message
    except Exception as e:
        error_message = f"写入文件时发生了一个未知错误: {e}"
        print(error_message)
        return error_message


if __name__ == "__main__":
    # Initialize and run the server
    mcp.run(transport='stdio')