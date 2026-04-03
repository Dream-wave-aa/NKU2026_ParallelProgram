#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define N 1024  // 矩阵大小 N x N

// 初始化矩阵和向量
void init(double **A, double *v, int n) {
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            A[i][j] = (double)(rand() % 100) / 10.0;
        }
    }
    for (int j = 0; j < n; j++) {
        v[j] = (double)(rand() % 100) / 10.0;
    }
}

// 1.a) 逐列访问的平凡算法
void col_dot_product(double **A, double *v, double *result, int n) {
    for (int j = 0; j < n; j++) {
        double sum = 0.0;
        for (int i = 0; i < n; i++) {
            sum += A[i][j] * v[j];
        }
        result[j] = sum;
    }
}

// 1.b) Cache优化算法（逐行访问）
void cache_opt_dot_product(double **A, double *v, double *result, int n) {
    // 初始化结果数组为0
    for (int j = 0; j < n; j++) {
        result[j] = 0.0;
    }
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            result[j] += A[i][j] * v[j];
        }
    }
}

// 2.a) 逐个累加的平凡算法
double sum_naive(double *arr, int n) {
    double sum = 0.0;
    for (int i = 0; i < n; i++) {
        sum += arr[i];
    }
    return sum;
}

// 2.b1) 两路链式累加（超标量优化）
double sum_two_way(double *arr, int n) {
    double sum1 = 0.0, sum2 = 0.0;
    int i;
    for (i = 0; i < n - 1; i += 2) {
        sum1 += arr[i];
        sum2 += arr[i + 1];
    }
    // 处理剩余元素
    for (; i < n; i++) {
        sum1 += arr[i];
    }
    return sum1 + sum2;
}

// 2.b2) 递归两两相加（分治求和）
double sum_recursive(double *arr, int n) {
    if (n == 0) return 0.0;
    if (n == 1) return arr[0];
    int mid = n / 2;
    return sum_recursive(arr, mid) + sum_recursive(arr + mid, n - mid);
}

int main() {
    srand(time(NULL));

    // 分配矩阵和向量内存
    double **A = (double **)malloc(N * sizeof(double *));
    for (int i = 0; i < N; i++) {
        A[i] = (double *)malloc(N * sizeof(double));
    }
    double *v = (double *)malloc(N * sizeof(double));
    double *result1 = (double *)malloc(N * sizeof(double));
    double *result2 = (double *)malloc(N * sizeof(double));

    init(A, v, N);

    // 测试矩阵列与向量内积
    clock_t start, end;

    start = clock();
    col_dot_product(A, v, result1, N);
    end = clock();
    printf("1.a) 逐列平凡算法耗时: %f 秒\n", (double)(end - start) / CLOCKS_PER_SEC);

    start = clock();
    cache_opt_dot_product(A, v, result2, N);
    end = clock();
    printf("1.b) Cache优化算法耗时: %f 秒\n", (double)(end - start) / CLOCKS_PER_SEC);

    // 验证结果一致性（近似比较）
    int correct = 1;
    for (int j = 0; j < N; j++) {
        if (abs(result1[j] - result2[j]) > 1e-6) {
            correct = 0;
            break;
        }
    }
    printf("两种算法结果一致: %s\n\n", correct ? "是" : "否");

    // 准备求和测试数组
    double *arr = (double *)malloc(N * sizeof(double));
    for (int i = 0; i < N; i++) {
        arr[i] = (double)(rand() % 100) / 10.0;
    }

    // 测试求和
    start = clock();
    double s1 = sum_naive(arr, N);
    end = clock();
    printf("2.a) 平凡累加耗时: %f 秒, 结果: %f\n", (double)(end - start) / CLOCKS_PER_SEC, s1);

    start = clock();
    double s2 = sum_two_way(arr, N);
    end = clock();
    printf("2.b1) 两路链式累加耗时: %f 秒, 结果: %f\n", (double)(end - start) / CLOCKS_PER_SEC, s2);

    start = clock();
    double s3 = sum_recursive(arr, N);
    end = clock();
    printf("2.b2) 递归两两相加耗时: %f 秒, 结果: %f\n", (double)(end - start) / CLOCKS_PER_SEC, s3);

    // 释放内存
    for (int i = 0; i < N; i++) free(A[i]);
    free(A);
    free(v);
    free(result1);
    free(result2);
    free(arr);

    return 0;
}