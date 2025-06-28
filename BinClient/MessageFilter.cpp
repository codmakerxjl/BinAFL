#include "pch.h"
#include "MessageFilter.h"

MessageFilter& MessageFilter::GetInstance() {
    static MessageFilter instance;
    return instance;
}

FilterAction MessageFilter::check(UINT uMsg) {
    // 1. 如果消息已在黑名单中，直接过滤
    if (blacklistedMessages_.count(uMsg)) {
        return FilterAction::FILTER;
    }

    // 2. 增加该消息的计数
    messageCounts_[uMsg]++;

    // 3. 检查是否达到阈值
    if (messageCounts_[uMsg] > THRESHOLD) {
        // 达到阈值，将其加入黑名单
        blacklistedMessages_.insert(uMsg);
        // 清理计数map中的条目以节省内存
        messageCounts_.erase(uMsg);
        // 返回特殊动作，通知调用者这是第一次超限
        return FilterAction::BLACKLIST_AND_FILTER;
    }

    // 4. 未达到阈值，正常记录
    return FilterAction::LOG;
}
