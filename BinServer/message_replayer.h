#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <optional>

// �������ڴ��б�ʾһ�����طŵ���Ϣ
struct ReplayMessage {
    HWND hwnd;
    UINT msg;
    WPARAM wParam;
    LPARAM lParam;
};

/**
 * @brief �������־�ļ����ز��ط�Windows��Ϣ���е��ࡣ
 * ����һ����̬����������Ŀ¼�з�����Ҫ�طŵ��ļ���
 * �Լ�������ʽ�طŻỰ�Ĺ��ܡ�
 */
class MessageReplayer {
public:
    // ���캯��
    MessageReplayer() = default;

    // --- �߲㼶�ĻỰ������ ---

    /**
     * @brief ����һ�������Ľ���ʽ�طŻỰ��
     * �����Զ������ļ�������طţ���������Ϣ�����û�ȷ�ϡ�
     * �û�ȷ����Ч���ļ�·���ᱻ��¼���ڲ��б��С�
     * @param directory Ҫ������Ŀ¼��
     * @param prefix �ļ�����Ҫƥ���ǰ׺ (���� "message_")��
     * @param delayBetweenMessagesMs �ط�ʱÿ����Ϣ���ӳ٣����룩��
     */
    void runInteractiveSession(
        const std::string& directory,
        const std::string& prefix,
        int delayBetweenMessagesMs = 0
    );

    /**
     * @brief ���ڻỰ�б����Ϊ��Ч�������ļ���ͳһ���Ƶ�ָ�������Ŀ¼��
     * @param outputDirectory ������Ч�ļ���Ŀ���ļ��С�
     * @return �ɹ����淵�� true�����򷵻� false��
     */
    bool saveEffectiveFiles(const std::string& outputDirectory) const;

    /**
     * @brief ��ȡ��ǰ�Ѽ�¼����Ч�ļ��б�
     * @return const std::vector<std::string>& ��Ч�ļ�·�����б�
     */
    const std::vector<std::string>& getEffectiveFiles() const;

    /**
     * @brief ����Ѽ�¼����Ч�ļ��б��Ա㿪ʼһ���µĻỰ��
     */
    void clearEffectiveFilesList();


    // --- �ײ㷽�������ֹ������Ա����ʹ�ã� ---

    /**
     * @brief ��ָ��Ŀ¼�в��Ҳ��������з���ǰ׺���ļ���
     * ����һ����̬����������ֱ��ͨ���������ã����贴��ʵ����
     */
    static std::vector<std::string> findAndSortSequenceFiles(const std::string& directory, const std::string& prefix);

    /**
     * @brief ��ָ������־�ļ�����һ����Ϣ���С�
     */
    bool loadSequenceFromFile(const std::string& filePath);

    /**
     * @brief ��˳���طŵ�ǰ�Ѽ��ص���Ϣ���С�
     */
    void replaySequence(int delayBetweenMessagesMs = 0) const;

    /**
     * @brief ����ڲ����ص���Ϣ���С�
     */
    void clearSequence();

    /**
     * @brief ��ȡ��ǰ���ص������е���Ϣ������
     */
    size_t getSequenceSize() const;

private:
    std::vector<ReplayMessage> m_messageSequence;
    std::vector<std::string> m_effectiveFiles; // ���ڴ洢��Ч�ļ�·���ĳ�Ա����

    // ˽�и������������ڽ���������־�ı�
    std::optional<ReplayMessage> parseLine(const std::string& line) const;
};