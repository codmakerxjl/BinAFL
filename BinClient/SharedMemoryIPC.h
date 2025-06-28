#pragma once

#include <windows.h>
#include <string>
#include <stdexcept> // �����׳��쳣
#include "ipc_control.h"


// �������ݽṹ
struct SharedData {
    int buff[256];
};

// Ϊ�����ڴ���ź�������Ψһ���ַ�������
const wchar_t* const SHM_NAME = L"BinAFLSharedMemory";
const wchar_t* const SEM_SERVER_READY_NAME = L"SemServerReady"; // ��ʾ���������У�����д��
const wchar_t* const SEM_DATA_READY_NAME = L"SemDataReady";     // ��ʾ������д�룬���Զ�ȡ

class SharedMemoryIPC {
public:
    // �����ɫ�������Ǵ�����Դ���Ǵ�������Դ
    enum class Role {
        SERVER,
        CLIENT
    };

    /**
     * @brief ���캯�������ݽ�ɫ��ʼ�������ڴ��ͬ������
     * @param role ָ������Ϊ����ˣ������ߣ����ǿͻ��ˣ������ߣ���
     * @throws std::runtime_error ����κ� WinAPI ����ʧ�ܡ�
     */
    SharedMemoryIPC(Role role);

    /**
     * @brief �����������Զ��������о�����ڴ�ӳ�䡣
     */
    ~SharedMemoryIPC();

    // ��ֹ�����͸�ֵ����Ϊ���������Ψһ��ϵͳ��Դ���
    SharedMemoryIPC(const SharedMemoryIPC&) = delete;
    SharedMemoryIPC& operator=(const SharedMemoryIPC&) = delete;

    /**
     * @brief (������) �ȴ����������У�Ȼ��д�����ݡ�
     * @param data Ҫд�빲���ڴ�����ݡ�
     * @param timeout ��ʱʱ�䣨���룩��INFINITE ��ʾ���޵ȴ���
     * @return ����ɹ�д����Ϊ true�������ʱ��Ϊ false��
     * @throws std::runtime_error ����ȴ����ͷ��ź���ʧ�ܡ�
     */
    bool write(const SharedData& data, DWORD timeout = INFINITE);

    /**
     * @brief (������) �ȴ������ݵ��Ȼ���ȡ���ݡ�
     * @param data ���ڽ��մӹ����ڴ��ȡ�����ݵ����á�
     * @param timeout ��ʱʱ�䣨���룩��INFINITE ��ʾ���޵ȴ���
     * @return ����ɹ���ȡ����������Ϊ true�������ʱ��Ϊ false��
     * @throws std::runtime_error ����ȴ����ͷ��ź���ʧ�ܡ�
     */
    bool read(SharedData& data, DWORD timeout = INFINITE);

    //�Կ���packet�Ķ�д����
    bool write(const ControlPacket& packet, DWORD timeout = INFINITE);
    bool read(ControlPacket& packet, DWORD timeout = INFINITE);
private:
    void create_resources();
    void open_resources();

    Role role_;
    HANDLE hMapFile_ = NULL;
    HANDLE hSemServerReady_ = NULL;
    HANDLE hSemDataReady_ = NULL;
    SharedData* pSharedData_ = nullptr;
    ControlPacket* pControlPacket_ = nullptr;
};