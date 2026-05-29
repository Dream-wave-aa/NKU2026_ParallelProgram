// md5_simd.h
#ifndef MD5_SIMD_H
#define MD5_SIMD_H

#include <iostream>
#include <string>
#include <cstring>
#include <vector>
#include <arm_neon.h>

using namespace std;

typedef unsigned char Byte;
typedef unsigned int bit32;

// MD5参数
#define s11 7
#define s12 12
#define s13 17
#define s14 22
#define s21 5
#define s22 9
#define s23 14
#define s24 20
#define s31 4
#define s32 11
#define s33 16
#define s34 23
#define s41 6
#define s42 10
#define s43 15
#define s44 21

// SIMD并行处理4个MD5哈希
void MD5Hash_SIMD_4(const string inputs[4], bit32 states[4][4]);
void MD5Hash_SIMD_4(const vector<string>& inputs, bit32 states[4][4]);

#endif