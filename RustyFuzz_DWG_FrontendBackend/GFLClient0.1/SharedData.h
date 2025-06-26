// �ļ�: SharedData.h

#pragma once

// A buffer large enough to handle insertions, e.g., 5MB
const int SHARED_BUFFER_SIZE = 5 * 1024 * 1024;

// Maximum number of read operations to record during analysis
const int MAX_OFFSETS = 1024;

// Communication protocol between Fuzzer and Target
namespace FuzzProtocol {
    // Commands from Target to Fuzzer
    const int CMD_EXIT = 0;
    const int CMD_CONTINUE_FUZZING = 1;
    const int CMD_INIT = 2; // Signal that the target has begun analysis

    // Results from Target to Fuzzer
    const int RESULT_OK = 0;
    const int RESULT_CRASH = 1;
    const int RESULT_NEW_COVERAGE = 2;
}

struct SharedData {
    // ���ؼ��޸ġ�����һ���� Fuzzer ���Ƶ�ȫ��״̬��־
    // false = �����׶� (Analysis)
    // true  = Fuzzing �׶� (Fuzzing)
    bool analysis_completed;

    // --- �����ֶα��ֲ��� ---
    int command;
    int result;
    int data_size;
    char buffer[SHARED_BUFFER_SIZE];
    size_t op_count;
    unsigned long long offsets[MAX_OFFSETS];
    unsigned long sizes[MAX_OFFSETS];
};