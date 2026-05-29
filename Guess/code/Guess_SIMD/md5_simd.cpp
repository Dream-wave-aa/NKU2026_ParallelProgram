// md5_simd.cpp
#include "md5_simd.h"
#include <cstring>
#include <algorithm>

using namespace std;

// ARM NEON 左旋转操作
inline uint32x4_t SIMD_ROTATELEFT(uint32x4_t num, int n) {
    return vorrq_u32(vshlq_n_u32(num, n), vshrq_n_u32(num, 32 - n));
}

// ARM NEON 版本的F, G, H, I函数
inline uint32x4_t SIMD_F(uint32x4_t x, uint32x4_t y, uint32x4_t z) {
    return vorrq_u32(vandq_u32(x, y), vbicq_u32(z, x));  // (x & y) | ((~x) & z)
}

inline uint32x4_t SIMD_G(uint32x4_t x, uint32x4_t y, uint32x4_t z) {
    return vorrq_u32(vandq_u32(x, z), vbicq_u32(y, z));  // (x & z) | (y & ~z)
}

inline uint32x4_t SIMD_H(uint32x4_t x, uint32x4_t y, uint32x4_t z) {
    return veorq_u32(veorq_u32(x, y), z);  // x ^ y ^ z
}

inline uint32x4_t SIMD_I(uint32x4_t x, uint32x4_t y, uint32x4_t z) {
    return veorq_u32(y, vorrq_u32(x, vmvnq_u32(z)));  // y ^ (x | (~z))
}

// ARM NEON 版本的FF, GG, HH, II操作
inline void SIMD_FF(uint32x4_t& a, uint32x4_t b, uint32x4_t c, uint32x4_t d, 
                    uint32x4_t x, int s, uint32_t ac) {
    a = vaddq_u32(a, SIMD_F(b, c, d));
    a = vaddq_u32(a, x);
    a = vaddq_u32(a, vdupq_n_u32(ac));
    a = SIMD_ROTATELEFT(a, s);
    a = vaddq_u32(a, b);
}

inline void SIMD_GG(uint32x4_t& a, uint32x4_t b, uint32x4_t c, uint32x4_t d, 
                    uint32x4_t x, int s, uint32_t ac) {
    a = vaddq_u32(a, SIMD_G(b, c, d));
    a = vaddq_u32(a, x);
    a = vaddq_u32(a, vdupq_n_u32(ac));
    a = SIMD_ROTATELEFT(a, s);
    a = vaddq_u32(a, b);
}

inline void SIMD_HH(uint32x4_t& a, uint32x4_t b, uint32x4_t c, uint32x4_t d, 
                    uint32x4_t x, int s, uint32_t ac) {
    a = vaddq_u32(a, SIMD_H(b, c, d));
    a = vaddq_u32(a, x);
    a = vaddq_u32(a, vdupq_n_u32(ac));
    a = SIMD_ROTATELEFT(a, s);
    a = vaddq_u32(a, b);
}

inline void SIMD_II(uint32x4_t& a, uint32x4_t b, uint32x4_t c, uint32x4_t d, 
                    uint32x4_t x, int s, uint32_t ac) {
    a = vaddq_u32(a, SIMD_I(b, c, d));
    a = vaddq_u32(a, x);
    a = vaddq_u32(a, vdupq_n_u32(ac));
    a = SIMD_ROTATELEFT(a, s);
    a = vaddq_u32(a, b);
}

/**
 * StringProcess: 将单个输入字符串转换成MD5计算所需的消息数组
 */
Byte *StringProcess_SIMD(string input, int *n_byte)
{
    Byte *blocks = (Byte *)input.c_str();
    int length = input.length();
    int bitLength = length * 8;

    int paddingBits = bitLength % 512;
    if (paddingBits > 448)
    {
        paddingBits += 512 - (paddingBits - 448);
    }
    else if (paddingBits < 448)
    {
        paddingBits = 448 - paddingBits;
    }
    else if (paddingBits == 448)
    {
        paddingBits = 512;
    }

    int paddingBytes = paddingBits / 8;
    int paddedLength = length + paddingBytes + 8;
    Byte *paddedMessage = new Byte[paddedLength];

    memcpy(paddedMessage, blocks, length);
    paddedMessage[length] = 0x80;
    memset(paddedMessage + length + 1, 0, paddingBytes - 1);

    for (int i = 0; i < 8; ++i)
    {
        paddedMessage[length + paddingBytes + i] = ((uint64_t)length * 8 >> (i * 8)) & 0xFF;
    }

    *n_byte = paddedLength;
    return paddedMessage;
}

/**
 * MD5Hash_SIMD_4: 同时处理4个字符串的MD5哈希 - ARM NEON版本
 * @param inputs 4个输入字符串
 * @param states 输出的4个状态数组
 */
void MD5Hash_SIMD_4(const string inputs[4], bit32 states[4][4])
{
    int messageLengths[4];
    Byte *paddedMessages[4];
    int max_blocks = 0;

    // 预处理所有4个消息
    for (int i = 0; i < 4; i++)
    {
        paddedMessages[i] = StringProcess_SIMD(inputs[i], &messageLengths[i]);
        int n_blocks = messageLengths[i] / 64;
        if (n_blocks > max_blocks)
        {
            max_blocks = n_blocks;
        }
    }

    // 初始化4个状态的NEON寄存器 - 每个寄存器包含4个32位值
    uint32x4_t state_a = vdupq_n_u32(0x67452301);
    uint32x4_t state_b = vdupq_n_u32(0xefcdab89);
    uint32x4_t state_c = vdupq_n_u32(0x98badcfe);
    uint32x4_t state_d = vdupq_n_u32(0x10325476);

    // 处理所有消息块
    for (int block = 0; block < max_blocks; block++)
    {
        uint32x4_t x[16];
        
        // 为每个消息准备16个32位字
        for (int j = 0; j < 16; j++)
        {
            alignas(16) bit32 words[4];
            
            for (int i = 0; i < 4; i++)
            {
                int n_blocks = messageLengths[i] / 64;
                if (block < n_blocks)
                {
                    words[i] = (paddedMessages[i][4 * j + block * 64]) |
                              (paddedMessages[i][4 * j + 1 + block * 64] << 8) |
                              (paddedMessages[i][4 * j + 2 + block * 64] << 16) |
                              (paddedMessages[i][4 * j + 3 + block * 64] << 24);
                }
                else
                {
                    words[i] = 0;
                }
            }
            
            x[j] = vld1q_u32(words);
        }

        uint32x4_t a = state_a, b = state_b, c = state_c, d = state_d;

        /* Round 1 */
        SIMD_FF(a, b, c, d, x[0], s11, 0xd76aa478);
        SIMD_FF(d, a, b, c, x[1], s12, 0xe8c7b756);
        SIMD_FF(c, d, a, b, x[2], s13, 0x242070db);
        SIMD_FF(b, c, d, a, x[3], s14, 0xc1bdceee);
        SIMD_FF(a, b, c, d, x[4], s11, 0xf57c0faf);
        SIMD_FF(d, a, b, c, x[5], s12, 0x4787c62a);
        SIMD_FF(c, d, a, b, x[6], s13, 0xa8304613);
        SIMD_FF(b, c, d, a, x[7], s14, 0xfd469501);
        SIMD_FF(a, b, c, d, x[8], s11, 0x698098d8);
        SIMD_FF(d, a, b, c, x[9], s12, 0x8b44f7af);
        SIMD_FF(c, d, a, b, x[10], s13, 0xffff5bb1);
        SIMD_FF(b, c, d, a, x[11], s14, 0x895cd7be);
        SIMD_FF(a, b, c, d, x[12], s11, 0x6b901122);
        SIMD_FF(d, a, b, c, x[13], s12, 0xfd987193);
        SIMD_FF(c, d, a, b, x[14], s13, 0xa679438e);
        SIMD_FF(b, c, d, a, x[15], s14, 0x49b40821);

        /* Round 2 */
        SIMD_GG(a, b, c, d, x[1], s21, 0xf61e2562);
        SIMD_GG(d, a, b, c, x[6], s22, 0xc040b340);
        SIMD_GG(c, d, a, b, x[11], s23, 0x265e5a51);
        SIMD_GG(b, c, d, a, x[0], s24, 0xe9b6c7aa);
        SIMD_GG(a, b, c, d, x[5], s21, 0xd62f105d);
        SIMD_GG(d, a, b, c, x[10], s22, 0x2441453);
        SIMD_GG(c, d, a, b, x[15], s23, 0xd8a1e681);
        SIMD_GG(b, c, d, a, x[4], s24, 0xe7d3fbc8);
        SIMD_GG(a, b, c, d, x[9], s21, 0x21e1cde6);
        SIMD_GG(d, a, b, c, x[14], s22, 0xc33707d6);
        SIMD_GG(c, d, a, b, x[3], s23, 0xf4d50d87);
        SIMD_GG(b, c, d, a, x[8], s24, 0x455a14ed);
        SIMD_GG(a, b, c, d, x[13], s21, 0xa9e3e905);
        SIMD_GG(d, a, b, c, x[2], s22, 0xfcefa3f8);
        SIMD_GG(c, d, a, b, x[7], s23, 0x676f02d9);
        SIMD_GG(b, c, d, a, x[12], s24, 0x8d2a4c8a);

        /* Round 3 */
        SIMD_HH(a, b, c, d, x[5], s31, 0xfffa3942);
        SIMD_HH(d, a, b, c, x[8], s32, 0x8771f681);
        SIMD_HH(c, d, a, b, x[11], s33, 0x6d9d6122);
        SIMD_HH(b, c, d, a, x[14], s34, 0xfde5380c);
        SIMD_HH(a, b, c, d, x[1], s31, 0xa4beea44);
        SIMD_HH(d, a, b, c, x[4], s32, 0x4bdecfa9);
        SIMD_HH(c, d, a, b, x[7], s33, 0xf6bb4b60);
        SIMD_HH(b, c, d, a, x[10], s34, 0xbebfbc70);
        SIMD_HH(a, b, c, d, x[13], s31, 0x289b7ec6);
        SIMD_HH(d, a, b, c, x[0], s32, 0xeaa127fa);
        SIMD_HH(c, d, a, b, x[3], s33, 0xd4ef3085);
        SIMD_HH(b, c, d, a, x[6], s34, 0x4881d05);
        SIMD_HH(a, b, c, d, x[9], s31, 0xd9d4d039);
        SIMD_HH(d, a, b, c, x[12], s32, 0xe6db99e5);
        SIMD_HH(c, d, a, b, x[15], s33, 0x1fa27cf8);
        SIMD_HH(b, c, d, a, x[2], s34, 0xc4ac5665);

        /* Round 4 */
        SIMD_II(a, b, c, d, x[0], s41, 0xf4292244);
        SIMD_II(d, a, b, c, x[7], s42, 0x432aff97);
        SIMD_II(c, d, a, b, x[14], s43, 0xab9423a7);
        SIMD_II(b, c, d, a, x[5], s44, 0xfc93a039);
        SIMD_II(a, b, c, d, x[12], s41, 0x655b59c3);
        SIMD_II(d, a, b, c, x[3], s42, 0x8f0ccc92);
        SIMD_II(c, d, a, b, x[10], s43, 0xffeff47d);
        SIMD_II(b, c, d, a, x[1], s44, 0x85845dd1);
        SIMD_II(a, b, c, d, x[8], s41, 0x6fa87e4f);
        SIMD_II(d, a, b, c, x[15], s42, 0xfe2ce6e0);
        SIMD_II(c, d, a, b, x[6], s43, 0xa3014314);
        SIMD_II(b, c, d, a, x[13], s44, 0x4e0811a1);
        SIMD_II(a, b, c, d, x[4], s41, 0xf7537e82);
        SIMD_II(d, a, b, c, x[11], s42, 0xbd3af235);
        SIMD_II(c, d, a, b, x[2], s43, 0x2ad7d2bb);
        SIMD_II(b, c, d, a, x[9], s44, 0xeb86d391);

        state_a = vaddq_u32(state_a, a);
        state_b = vaddq_u32(state_b, b);
        state_c = vaddq_u32(state_c, c);
        state_d = vaddq_u32(state_d, d);
    }

    // 提取结果并转换为小端格式
    alignas(16) bit32 raw_a[4], raw_b[4], raw_c[4], raw_d[4];
    
    vst1q_u32(raw_a, state_a);
    vst1q_u32(raw_b, state_b);
    vst1q_u32(raw_c, state_c);
    vst1q_u32(raw_d, state_d);

    // 为每个输出处理结果
    for (int i = 0; i < 4; i++)
    {
        bit32 a_val = raw_a[i];
        bit32 b_val = raw_b[i];
        bit32 c_val = raw_c[i];
        bit32 d_val = raw_d[i];
        
        // 字节序转换
        states[i][0] = ((a_val & 0xff) << 24) | 
                       ((a_val & 0xff00) << 8) | 
                       ((a_val & 0xff0000) >> 8) | 
                       ((a_val & 0xff000000) >> 24);
        states[i][1] = ((b_val & 0xff) << 24) | 
                       ((b_val & 0xff00) << 8) | 
                       ((b_val & 0xff0000) >> 8) | 
                       ((b_val & 0xff000000) >> 24);
        states[i][2] = ((c_val & 0xff) << 24) | 
                       ((c_val & 0xff00) << 8) | 
                       ((c_val & 0xff0000) >> 8) | 
                       ((c_val & 0xff000000) >> 24);
        states[i][3] = ((d_val & 0xff) << 24) | 
                       ((d_val & 0xff00) << 8) | 
                       ((d_val & 0xff0000) >> 8) | 
                       ((d_val & 0xff000000) >> 24);
    }

    // 清理内存
    for (int i = 0; i < 4; i++)
    {
        delete[] paddedMessages[i];
    }
}

void MD5Hash_SIMD_4(const vector<string>& inputs, bit32 states[4][4])
{
    string in[4];
    for (int i = 0; i < 4 && i < static_cast<int>(inputs.size()); i++)
    {
        in[i] = inputs[i];
    }
    for (int i = inputs.size(); i < 4; i++)
    {
        in[i] = "";
    }
    MD5Hash_SIMD_4(in, states);
}