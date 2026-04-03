#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_N 2048
#define REPEAT 10      // 每个规模重复次数
#define CACHE_LINE 64  // 假设缓存行大小 64 字节

// 高精度计时函数
double get_time() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

// 分配二维矩阵（连续内存，Cache友好）
double** alloc_matrix(int n) {
    double *data = (double*)malloc(n * n * sizeof(double));
    double **mat = (double**)malloc(n * sizeof(double*));
    for (int i = 0; i < n; i++) {
        mat[i] = data + i * n;
    }
    return mat;
}

void free_matrix(double **mat) {
    free(mat[0]);
    free(mat);
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
void row_dot_product(double **A, double *v, double *result, int n) {
    memset(result, 0, n * sizeof(double));
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            result[j] += A[i][j] * v[j];
        }
    }
}

// 验证结果一致性
int verify(double *r1, double *r2, int n) {
    for (int i = 0; i < n; i++) {
        if (fabs(r1[i] - r2[i]) > 1e-6) return 0;
    }
    return 1;
}

// 2.a) 逐个累加的平凡算法
double sum_naive(double *arr, int n) {
    double sum = 0.0;
    for (int i = 0; i < n; i++) {
        sum += arr[i];
    }
    return sum;
}

// 2.b1) 两路链式累加
double sum_two_way(double *arr, int n) {
    double sum1 = 0.0, sum2 = 0.0;
    int i;
    for (i = 0; i < n - 1; i += 2) {
        sum1 += arr[i];
        sum2 += arr[i + 1];
    }
    for (; i < n; i++) sum1 += arr[i];
    return sum1 + sum2;
}

// 2.b2) 递归两两相加
double sum_recursive(double *arr, int n) {
    if (n <= 0) return 0.0;
    if (n == 1) return arr[0];
    int mid = n / 2;
    return sum_recursive(arr, mid) + sum_recursive(arr + mid, n - mid);
}

// 验证求和结果
int verify_sum(double s1, double s2, double s3) {
    double eps = 1e-6;
    return fabs(s1 - s2) < eps && fabs(s1 - s3) < eps;
}

// 测试矩阵内积
void test_matrix_dot() {
    printf("\n========== 矩阵列与向量内积测试 ==========\n");
    printf("%-8s %-15s %-15s %-10s\n", "规模N", "逐列(秒)", "逐行优化(秒)", "加速比");
    printf("------------------------------------------------\n");
    
    for (int n = 64; n <= MAX_N; n *= 2) {
        // 分配内存
        double **A = alloc_matrix(n);
        double *v = (double*)malloc(n * sizeof(double));
        double *r1 = (double*)malloc(n * sizeof(double));
        double *r2 = (double*)malloc(n * sizeof(double));
        
        // 固定数据（便于验证）
        for (int i = 0; i < n; i++) {
            v[i] = 1.0;
            for (int j = 0; j < n; j++) {
                A[i][j] = 1.0;
            }
        }
        
        double t1_total = 0, t2_total = 0;
        
        // 多次实验取平均
        for (int rep = 0; rep < REPEAT; rep++) {
            double start = get_time();
            col_dot_product(A, v, r1, n);
            t1_total += get_time() - start;
            
            start = get_time();
            row_dot_product(A, v, r2, n);
            t2_total += get_time() - start;
        }
        
        double t1_avg = t1_total / REPEAT;
        double t2_avg = t2_total / REPEAT;
        double speedup = t1_avg / t2_avg;
        
        // 验证正确性
        int ok = verify(r1, r2, n);
        
        printf("%-8d %-15.6f %-15.6f %-10.2f %s\n", 
               n, t1_avg, t2_avg, speedup, ok ? "✓" : "✗");
        
        free(v); free(r1); free(r2);
        free_matrix(A);
    }
}

// 测试求和
void test_sum() {
    printf("\n========== 求和算法测试 ==========\n");
    printf("%-8s %-15s %-15s %-15s %-10s\n", 
           "规模N", "平凡(秒)", "两路(秒)", "递归(秒)", "加速比(两路)");
    printf("------------------------------------------------------------\n");
    
    for (int n = 1024; n <= 1024 * 1024 * 8; n *= 2) {
        double *arr = (double*)malloc(n * sizeof(double));
        // 固定数据
        for (int i = 0; i < n; i++) {
            arr[i] = 1.0;
        }
        
        double t1_total = 0, t2_total = 0, t3_total = 0;
        
        for (int rep = 0; rep < REPEAT; rep++) {
            double start = get_time();
            double s1 = sum_naive(arr, n);
            t1_total += get_time() - start;
            
            start = get_time();
            double s2 = sum_two_way(arr, n);
            t2_total += get_time() - start;
            
            start = get_time();
            double s3 = sum_recursive(arr, n);
            t3_total += get_time() - start;
            
            // 验证正确性（只验证第一次，避免重复）
            if (rep == 0 && !verify_sum(s1, s2, s3)) {
                printf("警告：结果不一致！\n");
            }
        }
        
        double t1_avg = t1_total / REPEAT;
        double t2_avg = t2_total / REPEAT;
        double t3_avg = t3_total / REPEAT;
        double speedup = t1_avg / t2_avg;
        
        printf("%-8d %-15.6f %-15.6f %-15.6f %-10.2f\n",
               n, t1_avg, t2_avg, t3_avg, speedup);
        
        free(arr);
    }
}

// 分析Cache效应
void test_cache_effect() {
    printf("\n========== Cache效应分析 ==========\n");
    printf("说明：当矩阵大小超过L2/L3缓存时，性能会明显下降\n\n");
    
    // 假设L3缓存约8MB，每个double 8字节
    // 矩阵占用内存 = n * n * 8 字节
    printf("矩阵大小与内存占用关系：\n");
    for (int n = 256; n <= 4096; n *= 2) {
        double mem_mb = n * n * 8.0 / (1024 * 1024);
        printf("  N=%d -> %.2f MB\n", n, mem_mb);
    }
    printf("\n建议：观察上面矩阵内积测试中，当N超过约1024时（8MB），");
    printf("优化算法的优势会更明显\n");
}

int main() {
    printf("=== 高性能计算实验：平凡算法 vs 优化算法 ===\n");
    printf("重复次数: %d\n", REPEAT);
    
    test_matrix_dot();
    test_sum();
    test_cache_effect();
    
    return 0;
}