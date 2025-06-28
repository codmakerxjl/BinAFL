#pragma once
#include <windows.h>
#include <map>
#include <set>

// 定义过滤器的行为结果
enum class FilterAction {
    LOG,                  // 正常记录此消息
    FILTER,               // 过滤此消息（因为它已在黑名单中）
    BLACKLIST_AND_FILTER  // 过滤此消息，并将其加入黑名单（这是第一次超限）
};

/**
 * @class MessageFilter
 * @brief 一个单例类，用于统计消息数量并决定是否应记录或过滤它们。
 */
class MessageFilter {
public:
    static MessageFilter& GetInstance();

    MessageFilter(const MessageFilter&) = delete;
    MessageFilter& operator=(const MessageFilter&) = delete;

    /**
     * @brief 检查并统计一条消息，返回相应的处理动作。
     * @param uMsg 要检查的消息ID。
     * @return 返回一个FilterAction枚举值，指示调用者应如何处理此消息。
     */
    FilterAction check(UINT uMsg);

private:
    MessageFilter() = default;
    ~MessageFilter() = default;

    // 阈值，当消息数量超过此时将被加入黑名单
    const size_t THRESHOLD = 10;

    std::map<UINT, size_t> messageCounts_;  // 存储每条消息的计数
    std::set<UINT> blacklistedMessages_;    // 存储已加入黑名单的消息ID
};
