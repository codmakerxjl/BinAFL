#pragma once

//�����BinServer����BinClient ipcģ��

// �����˷���˿��Է��͸��ͻ��˵Ŀ�������
enum class ControlCommand {
    NO_OP,          // �޲���
    START_LOGGING,  // ����ͻ��˿�ʼ��¼��Ϣ
    STOP_LOGGING,   // ����ͻ���ֹͣ��¼��Ϣ
    TERMINATE       // ����ͻ��˵Ŀ����߳��˳�
};

// �ڹ����ڴ��д��ݵ����ݰ��ṹ
struct ControlPacket {
    ControlCommand command;
};
