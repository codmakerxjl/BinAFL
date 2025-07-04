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
    // 仅当提供了 OVERLAPPED 结构时才处理，因为需要明确的偏移量
    // 并且读取大小必须大于阈值
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
        // 缓存未命中
        return false;
    }

    // 缓存命中
    auto& entry = *(it->second);

    // 检查缓存的数据是否足够本次读取
    if (entry.data.size() < nNumberOfBytesToRead) {
        // 虽然命中了起始偏移，但缓存的数据长度不足，视为未命中
        // 这种情况在实践中可能需要更复杂的逻辑（如部分读取），但为简化，我们直接返回false
        return false;
    }

    // 将数据从缓存复制到目标缓冲区
    memcpy(lpBuffer, entry.data.data(), nNumberOfBytesToRead);
    *lpNumberOfBytesRead = nNumberOfBytesToRead;

    // 将此条目移动到LRU列表的前端（表示最近使用）
    lruList.splice(lruList.begin(), lruList, it->second);

    return true;
}
void FileCacheManager::clear() {
    std::lock_guard<std::mutex> lock(cacheMutex); // 保证线程安全
    lruList.clear();
    cacheMap.clear();
    std::wcout << L"[CACHE CLEARED]" << std::endl; // 可选的日志输出
}
void FileCacheManager::PutInCache(
    HANDLE hFile,
    LPCVOID lpBuffer,
    DWORD bytesRead,
    LPOVERLAPPED lpOverlapped
) {
    // 同样，只缓存满足条件的读取
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

    // 如果该条目已存在，则先删除旧的
    auto it = cacheMap.find(key);
    if (it != cacheMap.end()) {
        lruList.erase(it->second);
        cacheMap.erase(it);
    }

    // 如果缓存已满，则进行驱逐
    if (cacheMap.size() >= maxCacheEntries) {
        Evict();
    }

    // 创建新的缓存条目
    CacheEntry newEntry;
    newEntry.key = key;
    newEntry.data.assign(static_cast<const BYTE*>(lpBuffer), static_cast<const BYTE*>(lpBuffer) + bytesRead);

    // 插入到LRU列表的前端
    lruList.push_front(std::move(newEntry));

    // 在map中存储指向列表节点的迭代器
    cacheMap[key] = lruList.begin();

}

void FileCacheManager::Evict() {
    // 驱逐 lruList 尾部的条目（最近最少使用的）
    if (!lruList.empty()) {
        const CacheEntry& last = lruList.back();
        cacheMap.erase(last.key);
        lruList.pop_back();
    }
}

std::wstring FileCacheManager::GetPathFromHandle(HANDLE hFile) {
    // 使用 GetFinalPathNameByHandle 获取文件的规范路径，这比 GetFilePathByHandle 更健壮
    wchar_t filePath[MAX_PATH];
    DWORD pathLen = GetFinalPathNameByHandleW(hFile, filePath, MAX_PATH, FILE_NAME_NORMALIZED);

    if (pathLen > 0 && pathLen < MAX_PATH) {
        // GetFinalPathNameByHandleW 的结果可能以 "\\?\" 开头，我们将其移除
        return std::wstring(wcsstr(filePath, L"\\??\\") ? filePath + 4 : filePath);
    }

    return L"";
}