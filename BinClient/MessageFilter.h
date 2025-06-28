#pragma once
#include <windows.h>
#include <map>
#include <set>

// �������������Ϊ���
enum class FilterAction {
    LOG,                  // ������¼����Ϣ
    FILTER,               // ���˴���Ϣ����Ϊ�����ں������У�
    BLACKLIST_AND_FILTER  // ���˴���Ϣ���������������������ǵ�һ�γ��ޣ�
};

/**
 * @class MessageFilter
 * @brief һ�������࣬����ͳ����Ϣ�����������Ƿ�Ӧ��¼��������ǡ�
 */
class MessageFilter {
public:
    static MessageFilter& GetInstance();

    MessageFilter(const MessageFilter&) = delete;
    MessageFilter& operator=(const MessageFilter&) = delete;

    /**
     * @brief ��鲢ͳ��һ����Ϣ��������Ӧ�Ĵ�������
     * @param uMsg Ҫ������ϢID��
     * @return ����һ��FilterActionö��ֵ��ָʾ������Ӧ��δ������Ϣ��
     */
    FilterAction check(UINT uMsg);

private:
    MessageFilter() = default;
    ~MessageFilter() = default;

    // ��ֵ������Ϣ����������ʱ�������������
    const size_t THRESHOLD = 10;

    std::map<UINT, size_t> messageCounts_;  // �洢ÿ����Ϣ�ļ���
    std::set<UINT> blacklistedMessages_;    // �洢�Ѽ������������ϢID
};
