#include "SharedMemoryIPC.h"
#include <system_error> // ���ڸ���ϸ�Ĵ�����Ϣ
#include "pch.h"
// ���������������׳��� WinAPI ��������쳣
void throw_windows_error(const std::string& message) {
    throw std::runtime_error(
        message + " (Error code: " + std::to_string(GetLastError()) + ")"
    );
}

SharedMemoryIPC::SharedMemoryIPC(Role role) : role_(role) {
    if (role_ == Role::SERVER) {
        create_resources();
    }
    else {
        open_resources();
    }

    // �������ڴ�ӳ�䵽���̵�ַ�ռ� (����˺Ϳͻ��˶���Ҫ)
    pSharedData_ = static_cast<SharedData*>(MapViewOfFile(
        hMapFile_,
        FILE_MAP_ALL_ACCESS,
        0,
        0,
        sizeof(SharedData)
    ));
    pControlPacket_ = static_cast<ControlPacket*>(MapViewOfFile(
        hMapFile_,
        FILE_MAP_ALL_ACCESS,
        0,
        0,
        sizeof(ControlPacket)
    ));
    if (pSharedData_ == nullptr) {
        throw_windows_error("pSharedData_MapViewOfFile failed");
    }
    if (pControlPacket_ == nullptr) {
        throw_windows_error("pControlPacket_MapViewOfFile failed");
    }
}

SharedMemoryIPC::~SharedMemoryIPC() {
    if (pSharedData_ != nullptr) {
        UnmapViewOfFile(pSharedData_);
    }
    if (hMapFile_ != NULL) {
        CloseHandle(hMapFile_);
    }
    if (hSemDataReady_ != NULL) {
        CloseHandle(hSemDataReady_);
    }
    if (hSemServerReady_ != NULL) {
        CloseHandle(hSemServerReady_);
    }
}

void SharedMemoryIPC::create_resources() {
    // �����ź���:
    // SemServerReady: ��ʼֵΪ1�����ֵΪ1����ʾ��������ʼʱ�ǿ��еġ�
    hSemServerReady_ = CreateSemaphoreW(NULL, 1, 1, SEM_SERVER_READY_NAME);
    if (hSemServerReady_ == NULL) {
        throw_windows_error("CreateSemaphoreW for SemServerReady failed");
    }

    // SemDataReady: ��ʼֵΪ0�����ֵΪ1����ʾ��ʼʱû�����ݡ�
    hSemDataReady_ = CreateSemaphoreW(NULL, 0, 1, SEM_DATA_READY_NAME);
    if (hSemDataReady_ == NULL) {
        throw_windows_error("CreateSemaphoreW for SemDataReady failed");
    }

    // �����ļ�ӳ�����
    hMapFile_ = CreateFileMappingW(
        INVALID_HANDLE_VALUE,
        NULL,
        PAGE_READWRITE,
        0,
        sizeof(SharedData),
        SHM_NAME
    );

    if (hMapFile_ == NULL) {
        throw_windows_error("CreateFileMappingW failed");
    }
}

void SharedMemoryIPC::open_resources() {
    // ���Ѵ��ڵ��ź���
    hSemServerReady_ = OpenSemaphoreW(SEMAPHORE_ALL_ACCESS, FALSE, SEM_SERVER_READY_NAME);
    if (hSemServerReady_ == NULL) {
        throw_windows_error("OpenSemaphoreW for SemServerReady failed. Is the server running?");
    }

    hSemDataReady_ = OpenSemaphoreW(SEMAPHORE_ALL_ACCESS, FALSE, SEM_DATA_READY_NAME);
    if (hSemDataReady_ == NULL) {
        throw_windows_error("OpenSemaphoreW for SemDataReady failed. Is the server running?");
    }

    // ���Ѵ��ڵ��ļ�ӳ�����
    hMapFile_ = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, SHM_NAME);
    if (hMapFile_ == NULL) {
        throw_windows_error("OpenFileMappingW failed. Is the server running?");
    }
}

bool SharedMemoryIPC::write(const SharedData& data, DWORD timeout) {
    // 1. �ȴ����������� (�ȴ� SemServerReady �ź���)
    DWORD waitResult = WaitForSingleObject(hSemServerReady_, timeout);
    if (waitResult != WAIT_OBJECT_0) {
        if (waitResult == WAIT_TIMEOUT) return false; // ��ʱ
        throw_windows_error("WaitForSingleObject on SemServerReady failed in write()");
    }

    // 2. д������
    memcpy(pSharedData_, &data, sizeof(SharedData));

    // 3. ֪ͨ������������׼���� (�ͷ� SemDataReady �ź���)
    if (!ReleaseSemaphore(hSemDataReady_, 1, NULL)) {
        throw_windows_error("ReleaseSemaphore on SemDataReady failed in write()");
    }

    return true;
}
bool SharedMemoryIPC::write(const ControlPacket& data, DWORD timeout) {
    // 1. �ȴ����������� (�ȴ� SemServerReady �ź���)
    DWORD waitResult = WaitForSingleObject(hSemServerReady_, timeout);
    if (waitResult != WAIT_OBJECT_0) {
        if (waitResult == WAIT_TIMEOUT) return false; // ��ʱ
        throw_windows_error("WaitForSingleObject on SemServerReady failed in write()");
    }

    // 2. д������
    memcpy(pControlPacket_, &data, sizeof(ControlPacket));

    // 3. ֪ͨ������������׼���� (�ͷ� SemDataReady �ź���)
    if (!ReleaseSemaphore(hSemDataReady_, 1, NULL)) {
        throw_windows_error("ReleaseSemaphore on SemDataReady failed in write()");
    }

    return true;
}

bool SharedMemoryIPC::read(SharedData& data, DWORD timeout) {
    // 1. �ȴ����ݾ��� (�ȴ� SemDataReady �ź���)
    DWORD waitResult = WaitForSingleObject(hSemDataReady_, timeout);
    if (waitResult != WAIT_OBJECT_0) {
        if (waitResult == WAIT_TIMEOUT) return false; // ��ʱ
        throw_windows_error("WaitForSingleObject on SemDataReady failed in read()");
    }

    // 2. ��ȡ����
    memcpy(&data, pSharedData_, sizeof(SharedData));

    // 3. ֪ͨ�����߻������ѿ��� (�ͷ� SemServerReady �ź���)
    if (!ReleaseSemaphore(hSemServerReady_, 1, NULL)) {
        throw_windows_error("ReleaseSemaphore on SemServerReady failed in read()");
    }

    return true;
}

bool SharedMemoryIPC::read(ControlPacket& data, DWORD timeout) {
    // 1. �ȴ����ݾ��� (�ȴ� SemDataReady �ź���)
    DWORD waitResult = WaitForSingleObject(hSemDataReady_, timeout);
    if (waitResult != WAIT_OBJECT_0) {
        if (waitResult == WAIT_TIMEOUT) return false; // ��ʱ
        throw_windows_error("WaitForSingleObject on SemDataReady failed in read()");
    }

    // 2. ��ȡ����
    memcpy(&data, pControlPacket_, sizeof(ControlPacket));

    // 3. ֪ͨ�����߻������ѿ��� (�ͷ� SemServerReady �ź���)
    if (!ReleaseSemaphore(hSemServerReady_, 1, NULL)) {
        throw_windows_error("ReleaseSemaphore on SemServerReady failed in read()");
    }

    return true;
}