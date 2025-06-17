#include <stdio.h>
#include <stdlib.h>
#include <cuda_runtime.h>
#include <nccl.h>

// Copied from the NCCL documentation
#define CUDACHECK(cmd) do {                         \
  cudaError_t err = cmd;                            \
  if (err != cudaSuccess) {                         \
    printf("Failed: Cuda error %s:%d '%s'\n",       \
        __FILE__,__LINE__,cudaGetErrorString(err)); \
    exit(EXIT_FAILURE);                             \
  }                                                 \
} while(0)


#define NCCLCHECK(cmd) do {                         \
  ncclResult_t res = cmd;                           \
  if (res != ncclSuccess) {                         \
    printf("Failed, NCCL error %s:%d '%s'\n",       \
        __FILE__,__LINE__,ncclGetErrorString(res)); \
    exit(EXIT_FAILURE);                             \
  }                                                 \
} while(0)

/**
 * @brief 使用NCCL执行多GPU数据广播操作
 * @param data 需要广播的数据缓冲区指针
 * @param count 数据元素的个数
 * @param root 广播发送方的GPU ID
 * @param datatype 数据类型（如ncclFloat）
 * @param comm NCCL通信器
 * @param stream CUDA流
 * @return ncclSuccess表示成功，其他错误码请参照NCCL官方文档
 */
ncclResult_t nccl_broadcast_data(void* data, 
                                 size_t count, 
                                 int root, 
                                 ncclDataType_t datatype,
                                 ncclComm_t comm, 
                                 cudaStream_t stream) {
    return ncclBroadcast(data, data, count, datatype, root, comm, stream);
}

/**
 * @brief 使用NCCL执行多GPU All-Reduce操作
 * @param sendbuff 发送数据的设备指针（每个GPU上都要分配）
 * @param recvbuff 接收结果的设备指针（每个GPU上都要分配）
 * @param count 数据元素的个数
 * @param datatype 数据类型（如ncclFloat）
 * @param op 归约操作类型（如ncclSum, ncclMax, ncclMin, ncclProd等）
 * @param comm NCCL通信器
 * @param stream CUDA流
 * @return ncclSuccess表示成功，其他错误码请参照NCCL官方文档
 * 此API会将所有GPU上的sendbuff数据做归约，结果写入recvbuff，并同步到所有GPU
 */
ncclResult_t nccl_allreduce_data(void* sendbuff,
                                 void* recvbuff,
                                 size_t count,
                                 ncclDataType_t datatype,
                                 ncclRedOp_t op,
                                 ncclComm_t comm,
                                 cudaStream_t stream) {
    return ncclAllReduce(sendbuff, recvbuff, count, datatype, op, comm, stream);
}

int main(int argc, char* argv[])
{
    const int nDev = 4;
    const int size = 1024 * 1024; // 1M float
    int devs[4];
    for (int i = 0; i < nDev; ++i) devs[i] = i;
    ncclComm_t comms[nDev];
    float** sendbuff = (float**)malloc(nDev * sizeof(float*));
    float** recvbuff = (float**)malloc(nDev * sizeof(float*));
    float** hostbuff = (float**)malloc(nDev * sizeof(float*));
    cudaStream_t* s = (cudaStream_t*)malloc(sizeof(cudaStream_t)*nDev);
    int root = 0;
    printf("NCCL Broadcast/AllReduce Test\n");

    // Initialize Data and NCCL
    for (int i = 0; i < nDev; ++i) {
        CUDACHECK(cudaSetDevice(i));
        CUDACHECK(cudaMalloc((void**)&sendbuff[i], size * sizeof(float)));
        CUDACHECK(cudaMalloc((void**)&recvbuff[i], size * sizeof(float)));
        hostbuff[i] = (float*)malloc(size * sizeof(float));
        for (int j = 0; j < size; ++j) hostbuff[i][j] = (i == root) ? (float)j : 0.0f;
        CUDACHECK(cudaMemcpy(sendbuff[i], hostbuff[i], size * sizeof(float), cudaMemcpyHostToDevice));
        CUDACHECK(cudaMemset(recvbuff[i], 0, size * sizeof(float)));
        CUDACHECK(cudaStreamCreate(s + i));
    }

    NCCLCHECK(ncclCommInitAll(comms, nDev, devs));

    // Test Broadcast
    printf("Testing nccl_broadcast_data...\n");
    NCCLCHECK(ncclGroupStart());
    for (int i = 0; i < nDev; ++i)
        NCCLCHECK(nccl_broadcast_data(sendbuff[i], size, root, ncclFloat, comms[i], s[i]));
    NCCLCHECK(ncclGroupEnd());
    for (int i = 0; i < nDev; ++i) {
        CUDACHECK(cudaStreamSynchronize(s[i]));
        CUDACHECK(cudaMemcpy(hostbuff[i], sendbuff[i], size * sizeof(float), cudaMemcpyDeviceToHost));
    }
    int broadcast_error = 0;
    for (int i = 0; i < nDev; ++i) {
        for (int j = 0; j < size; ++j) {
            if (hostbuff[i][j] != (float)j) { broadcast_error = 1; break; }
        }
    }
    printf("Broadcast %s\n", broadcast_error ? "FAILED" : "PASSED");

    // Test All-Reduce
    printf("Testing nccl_allreduce_data...\n");
    for (int i = 0; i < nDev; ++i) {
        for (int j = 0; j < size; ++j) hostbuff[i][j] = (float)(i + 1);
        CUDACHECK(cudaMemcpy(sendbuff[i], hostbuff[i], size * sizeof(float), cudaMemcpyHostToDevice));
    }
    NCCLCHECK(ncclGroupStart());
    for (int i = 0; i < nDev; ++i)
        NCCLCHECK(nccl_allreduce_data(sendbuff[i], recvbuff[i], size, ncclFloat, ncclSum, comms[i], s[i]));
    NCCLCHECK(ncclGroupEnd());
    for (int i = 0; i < nDev; ++i) {
        CUDACHECK(cudaStreamSynchronize(s[i]));
        CUDACHECK(cudaMemcpy(hostbuff[i], recvbuff[i], size * sizeof(float), cudaMemcpyDeviceToHost));
    }
    int allreduce_error = 0;
    float expect = 0.0f;
    for (int i = 0; i < nDev; ++i) expect += (float)(i + 1);
    for (int i = 0; i < nDev; ++i) {
        for (int j = 0; j < size; ++j) {
            if (hostbuff[i][j] != expect) { allreduce_error = 1; break; }
        }
    }
    printf("AllReduce %s\n", allreduce_error ? "FAILED" : "PASSED");
    
    // 性能测试参数
    int nIter = 100;
    float elapsed_ms = 0.0f;
    cudaEvent_t start, stop;
    CUDACHECK(cudaEventCreate(&start));
    CUDACHECK(cudaEventCreate(&stop));

    // --- NCCL Broadcast 性能测试 ---
    printf("\nNCCL Broadcast 性能测试...\n");
    // 预热
    NCCLCHECK(ncclGroupStart());
    for (int i = 0; i < nDev; ++i)
        NCCLCHECK(nccl_broadcast_data(sendbuff[i], size, root, ncclFloat, comms[i], s[i]));
    NCCLCHECK(ncclGroupEnd());
    for (int i = 0; i < nDev; ++i) CUDACHECK(cudaStreamSynchronize(s[i]));
    // 正式计时
    CUDACHECK(cudaEventRecord(start, 0));
    for (int iter = 0; iter < nIter; ++iter) {
        NCCLCHECK(ncclGroupStart());
        for (int i = 0; i < nDev; ++i)
            NCCLCHECK(nccl_broadcast_data(sendbuff[i], size, root, ncclFloat, comms[i], s[i]));
        NCCLCHECK(ncclGroupEnd());
        for (int i = 0; i < nDev; ++i) CUDACHECK(cudaStreamSynchronize(s[i]));
    }
    CUDACHECK(cudaEventRecord(stop, 0));
    CUDACHECK(cudaEventSynchronize(stop));
    CUDACHECK(cudaEventElapsedTime(&elapsed_ms, start, stop));
    float avg_ms = elapsed_ms / nIter;
    float total_bytes = (float)size * sizeof(float);
    float bandwidth = total_bytes / (avg_ms / 1000.0f) / 1e9f; // GB/s
    printf("Broadcast 平均延迟: %.3f ms, 带宽: %.3f GB/s\n", avg_ms, bandwidth);

    // --- NCCL AllReduce 性能测试 ---
    printf("\nNCCL AllReduce 性能测试...\n");
    // 预热
    NCCLCHECK(ncclGroupStart());
    for (int i = 0; i < nDev; ++i)
        NCCLCHECK(nccl_allreduce_data(sendbuff[i], recvbuff[i], size, ncclFloat, ncclSum, comms[i], s[i]));
    NCCLCHECK(ncclGroupEnd());
    for (int i = 0; i < nDev; ++i) CUDACHECK(cudaStreamSynchronize(s[i]));
    // 正式计时
    CUDACHECK(cudaEventRecord(start, 0));
    for (int iter = 0; iter < nIter; ++iter) {
        NCCLCHECK(ncclGroupStart());
        for (int i = 0; i < nDev; ++i)
            NCCLCHECK(nccl_allreduce_data(sendbuff[i], recvbuff[i], size, ncclFloat, ncclSum, comms[i], s[i]));
        NCCLCHECK(ncclGroupEnd());
        for (int i = 0; i < nDev; ++i) CUDACHECK(cudaStreamSynchronize(s[i]));
    }
    CUDACHECK(cudaEventRecord(stop, 0));
    CUDACHECK(cudaEventSynchronize(stop));
    CUDACHECK(cudaEventElapsedTime(&elapsed_ms, start, stop));
    avg_ms = elapsed_ms / nIter;
    bandwidth = total_bytes / (avg_ms / 1000.0f) / 1e9f; // GB/s
    printf("AllReduce 平均延迟: %.3f ms, 带宽: %.3f GB/s\n", avg_ms, bandwidth);

    CUDACHECK(cudaEventDestroy(start));
    CUDACHECK(cudaEventDestroy(stop));
    
    // Free
    for (int i = 0; i < nDev; ++i) {
        CUDACHECK(cudaFree(sendbuff[i]));
        CUDACHECK(cudaFree(recvbuff[i]));
        free(hostbuff[i]);
        cudaStreamDestroy(s[i]);
        ncclCommDestroy(comms[i]);
    }
    free(sendbuff); free(recvbuff); free(hostbuff); free(s);
    printf("Done.\n");
    return 0;
}