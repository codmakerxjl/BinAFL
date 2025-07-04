#include "pch.h"
#include "FileCacheManager.h"
#include <iostream>

FileCacheManager::FileCacheManager(size_t maxCacheEntries, DWORD minSizeToCache)
    : maxCacheEntries(maxCacheEntries), minSizeToCache(minSizeToCache) {}

bool FileCacheManager::TryGetFromCache(
    HANDLE hFile,
    LPVOID lpBuffer,
    DWORD nNumberOfBytesToRead,
    LPDWORD lpNumberOfBytesRead,
    LPOVERLAPPED lpOverlapped
) {
    // �����ṩ�� OVERLAPPED �ṹʱ�Ŵ�����Ϊ��Ҫ��ȷ��ƫ����
    // ���Ҷ�ȡ��С���������ֵ
    if (lpOverlapped == nullptr || nNumberOfBytesToRead <= minSizeToCache) {
        return false;
    }

    std::wstring filePath = GetPathFromHandle(hFile);
    if (filePath.empty()) {
        return false;
    }

    ULARGE_INTEGER offset;
    offset.LowPart = lpOverlapped->Offset;
    offset.HighPart = lpOverlapped->OffsetHigh;

    CacheKey key = { filePath, offset.QuadPart };

    std::lock_guard<std::mutex> lock(cacheMutex);

    auto it = cacheMap.find(key);
    if (it == cacheMap.end()) {
        // ����δ����
        return false;
    }

    // ��������
    auto& entry = *(it->second);

    // ��黺��������Ƿ��㹻���ζ�ȡ
    if (entry.data.size() < nNumberOfBytesToRead) {
        // ��Ȼ��������ʼƫ�ƣ�����������ݳ��Ȳ��㣬��Ϊδ����
        // ���������ʵ���п�����Ҫ�����ӵ��߼����粿�ֶ�ȡ������Ϊ�򻯣�����ֱ�ӷ���false
        return false;
    }

    // �����ݴӻ��渴�Ƶ�Ŀ�껺����
    memcpy(lpBuffer, entry.data.data(), nNumberOfBytesToRead);
    *lpNumberOfBytesRead = nNumberOfBytesToRead;

    // ������Ŀ�ƶ���LRU�б��ǰ�ˣ���ʾ���ʹ�ã�
    lruList.splice(lruList.begin(), lruList, it->second);

    return true;
}
void FileCacheManager::clear() {
    std::lock_guard<std::mutex> lock(cacheMutex); // ��֤�̰߳�ȫ
    lruList.clear();
    cacheMap.clear();
    std::wcout << L"[CACHE CLEARED]" << std::endl; // ��ѡ����־���
}
void FileCacheManager::PutInCache(
    HANDLE hFile,
    LPCVOID lpBuffer,
    DWORD bytesRead,
    LPOVERLAPPED lpOverlapped
) {
    // ͬ����ֻ�������������Ķ�ȡ
    if (lpOverlapped == nullptr || bytesRead <= minSizeToCache) {
        return;
    }

    std::wstring filePath = GetPathFromHandle(hFile);
    if (filePath.empty()) {
        return;
    }

    ULARGE_INTEGER offset;
    offset.LowPart = lpOverlapped->Offset;
    offset.HighPart = lpOverlapped->OffsetHigh;

    CacheKey key = { filePath, offset.QuadPart };

    std::lock_guard<std::mutex> lock(cacheMutex);

    // �������Ŀ�Ѵ��ڣ�����ɾ���ɵ�
    auto it = cacheMap.find(key);
    if (it != cacheMap.end()) {
        lruList.erase(it->second);
        cacheMap.erase(it);
    }

    // ����������������������
    if (cacheMap.size() >= maxCacheEntries) {
        Evict();
    }

    // �����µĻ�����Ŀ
    CacheEntry newEntry;
    newEntry.key = key;
    newEntry.data.assign(static_cast<const BYTE*>(lpBuffer), static_cast<const BYTE*>(lpBuffer) + bytesRead);

    // ���뵽LRU�б��ǰ��
    lruList.push_front(std::move(newEntry));

    // ��map�д洢ָ���б�ڵ�ĵ�����
    cacheMap[key] = lruList.begin();

}

void FileCacheManager::Evict() {
    // ���� lruList β������Ŀ���������ʹ�õģ�
    if (!lruList.empty()) {
        const CacheEntry& last = lruList.back();
        cacheMap.erase(last.key);
        lruList.pop_back();
    }
}

std::wstring FileCacheManager::GetPathFromHandle(HANDLE hFile) {
    // ʹ�� GetFinalPathNameByHandle ��ȡ�ļ��Ĺ淶·������� GetFilePathByHandle ����׳
    wchar_t filePath[MAX_PATH];
    DWORD pathLen = GetFinalPathNameByHandleW(hFile, filePath, MAX_PATH, FILE_NAME_NORMALIZED);

    if (pathLen > 0 && pathLen < MAX_PATH) {
        // GetFinalPathNameByHandleW �Ľ�������� "\\?\" ��ͷ�����ǽ����Ƴ�
        return std::wstring(wcsstr(filePath, L"\\??\\") ? filePath + 4 : filePath);
    }

    return L"";
}