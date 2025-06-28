#pragma once
#include "SharedMemoryIPC.h" // ������ļ�������Ŀ��

/**
 * @class CommandController
 * @brief ͨ��һ���ѽ�����IPCͨ������ͻ��˷��Ϳ������
 */
class CommandController {
public:
    /**
     * @brief ���캯����
     * @param ipc_channel һ���Ѿ���ʼ���õ� SharedMemoryIPC ͨ�����á�
     */
    explicit CommandController(SharedMemoryIPC& ipc_channel);

    /**
     * @brief �������֪ͨ�ͻ��˿�ʼ��Ϣ��ȡ�ͼ�¼��
     */
    void StartLogging();

    /**
     * @brief �������֪ͨ�ͻ���ֹͣ��Ϣ��ȡ�ͼ�¼��
     */
    void StopLogging();

    /**
     * @brief �������֪ͨ�ͻ��˵Ŀ����߳̿��԰�ȫ�˳��ˡ�
     */
    void SendTerminate();

private:
    SharedMemoryIPC& channel_; // ��ͨ��ͨ��������
};
