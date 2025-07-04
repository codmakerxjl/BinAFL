#pragma once

#include <vector>
#include <cstdint>
#include <string>

// �ֵ���Ŀ�ṹ
struct AFLDictEntry {
    const uint8_t* data;
    size_t len;
};

// ��������ö��
enum class AFLMutateType {
    Bitflip,
    Arith,
    Interest,
    Extras,
    Havoc,
    All // 'All' �������ѡ��һ�ֲ���
};

/**
 * @brief �Ի�����ִ��һ������ı��ط�ת���� (1, 2, 4, 8, 16, 32λ)��
 * @param buffer ��Ҫ����Ļ��������������á�
 */
void afl_mutate_bitflip(std::vector<uint8_t>& buffer);

/**
 * @brief �Ի�����ִ��һ�������������/������ (8, 16, 32λ)��
 * @param buffer ��Ҫ����Ļ��������������á�
 */
void afl_mutate_arith(std::vector<uint8_t>& buffer);

/**
 * @brief ��һ��Ԥ����ġ�����Ȥ��ֵ�б��У������ѡһ�������ǻ������е�ĳ��λ�á�
 * @param buffer ��Ҫ����Ļ��������������á�
 */
void afl_mutate_interest(std::vector<uint8_t>& buffer);

/**
 * @brief ʹ���û��ṩ���ֵ䣨extras�����Ի��������б��죨���ǻ���룩��
 * @param buffer ��Ҫ����Ļ��������������á�
 * @param extras �����ֵ���Ŀ��������
 */
void afl_mutate_extras(std::vector<uint8_t>& buffer, const std::vector<AFLDictEntry>& extras);

/**
 * @brief �����ƻ����׶Σ�����ء���ε�Ӧ�ø��ֲ�ͬ�ı�����ԡ�
 * @param buffer ��Ҫ����Ļ��������������á�
 * @param extras ��ѡ���ֵ䣬������havoc�׶��н����ֵ���졣
 */
void afl_mutate_havoc(std::vector<uint8_t>& buffer, const std::vector<AFLDictEntry>& extras = {});

/**
 * @brief �����캯���ַ�����
 * @param buffer ��Ҫ����Ļ��������������á�
 * @param type Ҫִ�еı������͡����ΪAll�������ѡ��һ�֡�
 * @param extras �û��ṩ���ֵ䣬������Ҫʱʹ�á�
 */
void afl_mutate_buffer(std::vector<uint8_t>& buffer, AFLMutateType type, const std::vector<AFLDictEntry>& extras = {});
