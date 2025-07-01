#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <optional>

// 用于在内存中表示一条待重放的消息
struct ReplayMessage {
    HWND hwnd;
    UINT msg;
    WPARAM wParam;
    LPARAM lParam;
};

/**
 * @brief 负责从日志文件加载并重放Windows消息序列的类。
 * 包含一个静态方法用于在目录中发现需要重放的文件，
 * 以及管理交互式重放会话的功能。
 */
class MessageReplayer {
public:
    // 构造函数
    MessageReplayer() = default;

    // --- 高层级的会话管理方法 ---

    /**
     * @brief 运行一个完整的交互式重放会话。
     * 它会自动查找文件，逐个重放，并弹出消息框让用户确认。
     * 用户确认有效的文件路径会被记录在内部列表中。
     * @param directory 要搜索的目录。
     * @param prefix 文件名需要匹配的前缀 (例如 "message_")。
     * @param delayBetweenMessagesMs 重放时每条消息的延迟（毫秒）。
     */
    void runInteractiveSession(
        const std::string& directory,
        const std::string& prefix,
        int delayBetweenMessagesMs = 0
    );

    /**
     * @brief 将在会话中被标记为有效的所有文件，统一复制到指定的输出目录。
     * @param outputDirectory 保存有效文件的目标文件夹。
     * @return 成功保存返回 true，否则返回 false。
     */
    bool saveEffectiveFiles(const std::string& outputDirectory) const;

    /**
     * @brief 获取当前已记录的有效文件列表。
     * @return const std::vector<std::string>& 有效文件路径的列表。
     */
    const std::vector<std::string>& getEffectiveFiles() const;

    /**
     * @brief 清空已记录的有效文件列表，以便开始一个新的会话。
     */
    void clearEffectiveFilesList();


    // --- 底层方法（保持公开，以便灵活使用） ---

    /**
     * @brief 在指定目录中查找并排序所有符合前缀的文件。
     * 这是一个静态方法，可以直接通过类名调用，无需创建实例。
     */
    static std::vector<std::string> findAndSortSequenceFiles(const std::string& directory, const std::string& prefix);

    /**
     * @brief 从指定的日志文件加载一个消息序列。
     */
    bool loadSequenceFromFile(const std::string& filePath);

    /**
     * @brief 按顺序重放当前已加载的消息序列。
     */
    void replaySequence(int delayBetweenMessagesMs = 0) const;

    /**
     * @brief 清空内部加载的消息序列。
     */
    void clearSequence();

    /**
     * @brief 获取当前加载的序列中的消息数量。
     */
    size_t getSequenceSize() const;

private:
    std::vector<ReplayMessage> m_messageSequence;
    std::vector<std::string> m_effectiveFiles; // 用于存储有效文件路径的成员变量

    // 私有辅助函数，用于解析单行日志文本
    std::optional<ReplayMessage> parseLine(const std::string& line) const;
};