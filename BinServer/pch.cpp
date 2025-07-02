
#include "pch.h"

bool Initialize() {
	//初始化ipc 
	SharedMemoryIPC ipc(SharedMemoryIPC::Role::SERVER);
	return 0;

}

std::string buildAgentPrompt(
    const std::string& logFilePath,
    const std::string& outputDir,
    int serverPort)
{
    // 直接将多行字符串字面量作为第一个参数传入
    return std::format(
        "读{}地址的文件，分析它的消息，然后提取重要且核心的用户操作的消息序列，保存到{}的目录下，同一操作的相关的消息放在同一个文件中，命名格式是message_1.log,message_2.log,...,当你分析完所有的操作之后，向端口为{}的服务器发送 done的字符串 ",
        // 传入参数
        logFilePath, outputDir, serverPort
    );
}