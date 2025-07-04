#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <list>
#include <unordered_map>
#include <mutex>
#include <memory>

// 定义缓存键，由文件路径和64位偏移量组成
struct CacheKey {
    std::wstring filePath;
    ULONGLONG offset;

    bool operator==(const CacheKey& other) const {
        return offset == other.offset && filePath == other.filePath;
    }
};

// 为 CacheKey 提供哈希函数，以便在 unordered_map 中使用
struct CacheKeyHasher {
    std::size_t operator()(const CacheKey& k) const {
        // 结合文件路径和偏移量的哈希值
        return std::hash<std::wstring>()(k.filePath) ^ (std::hash<ULONGLONG>()(k.offset) << 1);
    }
};

// 缓存管理器类
class FileCacheManager {
public:
    // 构造函数
    // maxCacheEntries: 缓存的最大条目数
    // minSizeToCache: 只有当读取大小超过此值时才进行缓存
    FileCacheManager(size_t maxCacheEntries, DWORD minSizeToCache);
    void clear();
    // 尝试从缓存中获取数据
    // 如果命中缓存，则将数据复制到 lpBuffer，填充 lpNumberOfBytesRead，并返回 true
    // 如果未命中或不满足缓存条件，则返回 false
    bool TryGetFromCache(
        HANDLE hFile,
        LPVOID lpBuffer,
        DWORD nNumberOfBytesToRead,
        LPDWORD lpNumberOfBytesRead,
        LPOVERLAPPED lpOverlapped
    );

    // 将新读取的数据放入缓存
    // 在调用原始 ReadFile 成功后调用此函数
    void PutInCache(
        HANDLE hFile,
        LPCVOID lpBuffer,
        DWORD bytesRead,
        LPOVERLAPPED lpOverlapped
    );

private:
    // 缓存条目结构
    struct CacheEntry {
        CacheKey key;
        std::vector<BYTE> data;
    };

    // 驱逐最近最少使用的条目
    void Evict();

    // 从文件句柄获取规范化的文件路径
    static std::wstring GetPathFromHandle(HANDLE hFile);

    // 成员变量
    std::list<CacheEntry> lruList; // 用于维护LRU顺序
    std::unordered_map<CacheKey, std::list<CacheEntry>::iterator, CacheKeyHasher> cacheMap; // 用于快速查找

    const size_t maxCacheEntries;      // 缓存最大条目数
    const DWORD minSizeToCache;        // 触发缓存的最小读取大小
    std::mutex cacheMutex;             // 用于保证线程安全
};