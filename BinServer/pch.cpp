
#include "pch.h"

bool Initialize() {
	//��ʼ��ipc 
	SharedMemoryIPC ipc(SharedMemoryIPC::Role::SERVER);
	return 0;

}

std::string buildAgentPrompt(
    const std::string& logFilePath,
    const std::string& outputDir,
    int serverPort)
{
    // ֱ�ӽ������ַ�����������Ϊ��һ����������
    return std::format(
        "��{}��ַ���ļ�������������Ϣ��Ȼ����ȡ��Ҫ�Һ��ĵ��û���������Ϣ���У����浽{}��Ŀ¼�£�ͬһ��������ص���Ϣ����ͬһ���ļ��У�������ʽ��message_1.log,message_2.log,...,������������еĲ���֮����˿�Ϊ{}�ķ��������� done���ַ��� ",
        // �������
        logFilePath, outputDir, serverPort
    );
}