#include "message_replayer.h"
#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>
#include <filesystem>
#include <algorithm>

// Ϊ C++17 �����ϰ汾���ļ�ϵͳ�ⶨ��һ�������ռ����
namespace fs = std::filesystem;

// --- ��̬������ʵ�� ---
std::vector<std::string> MessageReplayer::findAndSortSequenceFiles(const std::string& directory, const std::string& prefix)
{
    std::vector<std::string> foundFiles;
    try {
        for (const auto& entry : fs::directory_iterator(directory)) {
            if (entry.is_regular_file()) {
                std::string filename = entry.path().filename().string();
                if (filename.rfind(prefix, 0) == 0) {
                    foundFiles.push_back(entry.path().string());
                }
            }
        }
    }
    catch (const fs::filesystem_error& e) {
        std::cerr << "Error accessing directory " << directory << ": " << e.what() << std::endl;
        return {};
    }

    std::sort(foundFiles.begin(), foundFiles.end(),
        [prefix](const std::string& a, const std::string& b) {
            std::string filename_a = fs::path(a).filename().string();
            std::string filename_b = fs::path(b).filename().string();
            std::string num_str_a = filename_a.substr(prefix.length());
            std::string num_str_b = filename_b.substr(prefix.length());
            try {
                return std::stoi(num_str_a) < std::stoi(num_str_b);
            }
            catch (const std::exception&) {
                return a < b;
            }
        });
    return foundFiles;
}

// --- �߲㼶�Ự��������ʵ�� ---

void MessageReplayer::runInteractiveSession(const std::string& directory, const std::string& prefix, int delayBetweenMessagesMs)
{
    clearEffectiveFilesList(); // ��ʼ�»Ựǰ����վɵ���Ч�б�
    auto sequenceFiles = findAndSortSequenceFiles(directory, prefix);

    if (sequenceFiles.empty()) {
        MessageBoxW(NULL, L"��ָ��Ŀ¼δ�ҵ��κ���Ҫ�طŵ���Ϣ�ļ���", L"�Ự��ʾ", MB_ICONINFORMATION);
        return;
    }

    for (const auto& filePath : sequenceFiles) {
        std::cout << "\n--- Loading and Replaying sequence from: " << filePath << " ---" << std::endl;
        if (this->loadSequenceFromFile(filePath)) {
            this->replaySequence(delayBetweenMessagesMs);
            std::wstring wideFilePath(filePath.begin(), filePath.end());
            std::wstring promptText = L"�ո��ط����ļ�:\n" + wideFilePath + L"\n\n�����Ϣ�����Ƿ���Ч��";
            int result = MessageBoxW(NULL, promptText.c_str(), L"��ȷ��������Ч��", MB_YESNO | MB_ICONQUESTION);
            if (result == IDYES) {
                m_effectiveFiles.push_back(filePath);
                std::cout << "  > Marked as EFFECTIVE." << std::endl;
            }
            else {
                std::cout << "  > Marked as ineffective." << std::endl;
            }
        }
        else {
            std::cerr << "Failed to load sequence from " << filePath << ". Skipping." << std::endl;
        }
    }
}

bool MessageReplayer::saveEffectiveFiles(const std::string& outputDirectory) const
{
    if (m_effectiveFiles.empty()) {
        MessageBoxW(NULL, L"û�з��ֱ����Ϊ��Ч����Ϣ���пɹ����档", L"������ʾ", MB_ICONINFORMATION);
        return true;
    }
    try {
        fs::create_directory(outputDirectory);
        for (const auto& effectiveFile : m_effectiveFiles) {
            fs::path sourcePath(effectiveFile);
            fs::path destPath = fs::path(outputDirectory) / sourcePath.filename();
            fs::copy(sourcePath, destPath, fs::copy_options::overwrite_existing);
            std::cout << "Saved: " << destPath.string() << std::endl;
        }
        std::wstring finalMessage = L"�ѳɹ��� " + std::to_wstring(m_effectiveFiles.size())
            + L" ����Ч�����ļ����浽 '" + std::wstring(outputDirectory.begin(), outputDirectory.end()) + L"' �ļ����С�";
        MessageBoxW(NULL, finalMessage.c_str(), L"�������", MB_ICONINFORMATION);
        return true;
    }
    catch (const fs::filesystem_error& e) {
        std::string errorMsg = "�����ļ�ʱ����: " + std::string(e.what());
        std::wstring wideErrorMsg(errorMsg.begin(), errorMsg.end());
        MessageBoxW(NULL, wideErrorMsg.c_str(), L"����", MB_ICONERROR);
        return false;
    }
}

const std::vector<std::string>& MessageReplayer::getEffectiveFiles() const {
    return m_effectiveFiles;
}

void MessageReplayer::clearEffectiveFilesList() {
    m_effectiveFiles.clear();
}


// --- �ײ㷽����ʵ�� ---

bool MessageReplayer::loadSequenceFromFile(const std::string& filePath) {
    clearSequence();
    std::ifstream file(filePath);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file: " << filePath << std::endl;
        return false;
    }
    std::string line;
    while (std::getline(file, line)) {
        if (auto parsedMsg = parseLine(line)) {
            m_messageSequence.push_back(*parsedMsg);
        }
    }
    return true;
}

void MessageReplayer::replaySequence(int delayBetweenMessagesMs) const {
    std::cout << "Replaying " << m_messageSequence.size() << " messages..." << std::endl;
    for (const auto& msg : m_messageSequence) {
        PostMessage(msg.hwnd, msg.msg, msg.wParam, msg.lParam);
        if (delayBetweenMessagesMs > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(delayBetweenMessagesMs));
        }
    }
    std::cout << "Replay finished." << std::endl;
}

void MessageReplayer::clearSequence() {
    m_messageSequence.clear();
}

size_t MessageReplayer::getSequenceSize() const {
    return m_messageSequence.size();
}

std::optional<ReplayMessage> MessageReplayer::parseLine(const std::string& line) const {
    ReplayMessage msg{};
    char messageNameBuffer[128] = { 0 };
    int itemsParsed = sscanf_s(line.c_str(), "HWND: %p, Msg: %127s (%i), wParam: %p, lParam: %p",
        &msg.hwnd,
        messageNameBuffer, (unsigned)_countof(messageNameBuffer),
        &msg.msg,
        &msg.wParam,
        &msg.lParam);

    if (itemsParsed == 5) {
        return msg;
    }
    return std::nullopt;
}