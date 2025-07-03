#include "pch.h"
#include "MessageFilter.h"

MessageFilter& MessageFilter::GetInstance() {
    static MessageFilter instance;
    return instance;
}

FilterAction MessageFilter::check(UINT uMsg) {
    // 1. �����Ϣ���ں������У�ֱ�ӹ���
    if (blacklistedMessages_.count(uMsg)) {
        return FilterAction::FILTER;
    }

    // 2. ���Ӹ���Ϣ�ļ���
    messageCounts_[uMsg]++;

    // 3. ����Ƿ�ﵽ��ֵ
    if (messageCounts_[uMsg] > THRESHOLD) {
        // �ﵽ��ֵ��������������
        blacklistedMessages_.insert(uMsg);
        // �������map�е���Ŀ�Խ�ʡ�ڴ�
        messageCounts_.erase(uMsg);
        // �������⶯����֪ͨ���������ǵ�һ�γ���
        return FilterAction::BLACKLIST_AND_FILTER;
    }

    // 4. δ�ﵽ��ֵ��������¼
    return FilterAction::LOG;
}
