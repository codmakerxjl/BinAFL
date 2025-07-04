#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <list>
#include <unordered_map>
#include <mutex>
#include <memory>

// ���建��������ļ�·����64λƫ�������
struct CacheKey {
    std::wstring filePath;
    ULONGLONG offset;

    bool operator==(const CacheKey& other) const {
        return offset == other.offset && filePath == other.filePath;
    }
};

// Ϊ CacheKey �ṩ��ϣ�������Ա��� unordered_map ��ʹ��
struct CacheKeyHasher {
    std::size_t operator()(const CacheKey& k) const {
        // ����ļ�·����ƫ�����Ĺ�ϣֵ
        return std::hash<std::wstring>()(k.filePath) ^ (std::hash<ULONGLONG>()(k.offset) << 1);
    }
};

// �����������
class FileCacheManager {
public:
    // ���캯��
    // maxCacheEntries: ����������Ŀ��
    // minSizeToCache: ֻ�е���ȡ��С������ֵʱ�Ž��л���
    FileCacheManager(size_t maxCacheEntries, DWORD minSizeToCache);
    void clear();
    // ���Դӻ����л�ȡ����
    // ������л��棬�����ݸ��Ƶ� lpBuffer����� lpNumberOfBytesRead�������� true
    // ���δ���л����㻺���������򷵻� false
    bool TryGetFromCache(
        HANDLE hFile,
        LPVOID lpBuffer,
        DWORD nNumberOfBytesToRead,
        LPDWORD lpNumberOfBytesRead,
        LPOVERLAPPED lpOverlapped
    );

    // ���¶�ȡ�����ݷ��뻺��
    // �ڵ���ԭʼ ReadFile �ɹ�����ô˺���
    void PutInCache(
        HANDLE hFile,
        LPCVOID lpBuffer,
        DWORD bytesRead,
        LPOVERLAPPED lpOverlapped
    );

private:
    // ������Ŀ�ṹ
    struct CacheEntry {
        CacheKey key;
        std::vector<BYTE> data;
    };

    // �����������ʹ�õ���Ŀ
    void Evict();

    // ���ļ������ȡ�淶�����ļ�·��
    static std::wstring GetPathFromHandle(HANDLE hFile);

    // ��Ա����
    std::list<CacheEntry> lruList; // ����ά��LRU˳��
    std::unordered_map<CacheKey, std::list<CacheEntry>::iterator, CacheKeyHasher> cacheMap; // ���ڿ��ٲ���

    const size_t maxCacheEntries;      // ���������Ŀ��
    const DWORD minSizeToCache;        // �����������С��ȡ��С
    std::mutex cacheMutex;             // ���ڱ�֤�̰߳�ȫ
};