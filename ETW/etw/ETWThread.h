#pragma once
#include <vector>
#include <windows.h>
#include <evntcons.h>	// PEVENT_RECORD 从这来(自动带入 evntrace.h)

class ETWThread{
    public:
    void Start();				// 启动ETW线程
    void Stop();				// 停止ETW线程
    private:
    void StopStaleSession();	// 清理残留的ETW会话
    void CreateSession();		// 创建新的ETW会话
    void EnableProviders();		// 启用(订阅)Providers
    void CreateETWProperties(); // 创建ETW会话配置单
    void OpenRealTimeTrace();   // 打开实时会话(拿消费端句柄)
    static void WINAPI OnEvent(PEVENT_RECORD pEvent); // ETW回调函数
    std::vector<BYTE> propsBuf_; // 配置单存储空间
    EVENT_TRACE_PROPERTIES* props_ = nullptr; // 配置单指针
    CONTROLTRACE_ID sessionHandle_ = 0; // ETW会话ID(0=无会话)
    PROCESSTRACE_HANDLE traceHandle_ = INVALID_PROCESSTRACE_HANDLE; // 阻塞监听句柄
};
