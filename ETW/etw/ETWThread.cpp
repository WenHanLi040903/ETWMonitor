#include "ETWThread.h"
#include "common/cfg.h"
#include "common/providers.h"
#include <cwchar>	// std::wcslen
#include <cstring>	// memcpy
#include <cstdio>	// std::wprintf
#include <atomic>	// 打印计数

// 实现CreateETWProperties(),创建ETW会话配置单
void ETWThread::CreateETWProperties(){
    // ETW会话名所占字节
    // 这里的计算公式是先求出会话名的字符数然后+1，这个+1是"\0"结束符号,然后用字符数乘宽字符大小,求出会话名大小
    const size_t nameBytes = (std::wcslen(cfg::kSessionName) + 1) * sizeof(wchar_t);
    // ETW配置块所占字节
    // 这里的计算公式是用配置类大小+会话名大小,求出所需空间大小
    const size_t  propsBufBytes = nameBytes + sizeof(EVENT_TRACE_PROPERTIES);
    
    // 给成员变量中的属性赋值
    propsBuf_.assign(propsBufBytes, 0);
    props_ = reinterpret_cast<EVENT_TRACE_PROPERTIES*>(propsBuf_.data());
    
    // 填充 EVENT_TRACE_PROPERTIES 字段
    props_->Wnode.BufferSize    = static_cast<ULONG>(propsBufBytes);
    props_->Wnode.Flags         = WNODE_FLAG_TRACED_GUID;
    props_->BufferSize          = cfg::kBufferSizeKB;    
    props_->MinimumBuffers      = cfg::kMinBuffers;
    props_->MaximumBuffers      = cfg::kMaxBuffers;
    props_->FlushTimer          = cfg::kFlushTimerSec;
    props_->LogFileMode         = cfg::kLogFileMode;
    props_->LoggerNameOffset    = sizeof(EVENT_TRACE_PROPERTIES);
    props_->Wnode.Guid = cfg::kSessionGuid;
    // 拷贝会话名到结构体后面的位置
    memcpy(reinterpret_cast<BYTE*>(props_) + sizeof(EVENT_TRACE_PROPERTIES),
       cfg::kSessionName,
       nameBytes);
}


// 实现StopStaleSession(),清理残留的ETW会话,这里需要ETW配置单
void ETWThread::StopStaleSession(){
    ULONG status = ControlTraceW(sessionHandle_,	// 初始化是0,创建前清理是无句柄,析构时则有句柄
                                 cfg::kSessionName, 				// 配置常量中的会话名
                                 props_,					// 配置单地址
                                 EVENT_TRACE_CONTROL_STOP			// 停止会话控制code
                                );
    // TODO:异常处理
}

// 实现CreatSession(),开启ETW会话
void ETWThread::CreateSession(){
    // 先调用会话清理函数清理残留会话
    StopStaleSession();
    
    // 创新新的会话
     ULONG status = StartTraceW(&sessionHandle_,
                                cfg::kSessionName,
                                props_);
    
    //TODO:异常处理
}


// 实现EnableProviders(),启用多个Provider事件提供者
void ETWThread::EnableProviders(){
    for (const auto& p : providers::kProviderList) {
        EnableTraceEx2(sessionHandle_, p.guid,
                       EVENT_CONTROL_CODE_ENABLE_PROVIDER,
                       TRACE_LEVEL_INFORMATION,
                       p.maskCode, 0, 0, nullptr);
    }
}

// 实现OpenRealTimeTrace(),开启阻塞监听事件
void ETWThread::OpenRealTimeTrace(){
    EVENT_TRACE_LOGFILE logFile{};
    logFile.LoggerName          = const_cast<LPWSTR>(cfg::kSessionName);	// 要接的实时会话名
    logFile.ProcessTraceMode    = PROCESS_TRACE_MODE_REAL_TIME | PROCESS_TRACE_MODE_EVENT_RECORD;
    logFile.EventRecordCallback = &ETWThread::OnEvent;	// 事件回调
    logFile.Context             = this;	// 传给回调,回调里从 pEvent->UserContext 取回

    traceHandle_ = OpenTraceW(&logFile);
    if (traceHandle_ == INVALID_PROCESSTRACE_HANDLE) {
        // TODO: 失败处理
    }
}

// 实现回调函数
void WINAPI ETWThread::OnEvent(PEVENT_RECORD pEvent){
    // 流程验证:打印事件基本信息(限制打印量,避免刷屏)
    static std::atomic<unsigned long long> count{ 0 };
    const unsigned long long n = ++count;
    if (n <= 20 || n % 1000 == 0) {
        const EVENT_HEADER& h = pEvent->EventHeader;
        std::wprintf(L"[事件 %llu] EventId=%-5u PID=%-6lu TID=%-6lu Provider={%08lX-%04X-%04X}\n",
                     n,
                     static_cast<unsigned>(h.EventDescriptor.Id),
                     h.ProcessId,
                     h.ThreadId,
                     h.ProviderId.Data1,
                     static_cast<unsigned>(h.ProviderId.Data2),
                     static_cast<unsigned>(h.ProviderId.Data3));
    }

    // TODO:将Event深拷然后推送到全局队列
}


// 实现Start(),启动ETW线程
void ETWThread::Start(){
    // 1.生成ETW会话配置单
    CreateETWProperties();
    
    // 2.创建并启动会话(内部会先清理残留会话)
    CreateSession();
    //TODO:异常处理
    
    // 3.订阅Provider
    EnableProviders();
    
    // 4.打开实时会话,拿到消费端句柄
    OpenRealTimeTrace();
    //TODO:失败处理
    
    // 5.阻塞式获取事件
    //   ProcessTrace会一直卡在这里,直到别的线程调用Stop()
    ProcessTrace(&traceHandle_, 1, nullptr, nullptr);
}

// 实现Stop(),停止ETW线程
void ETWThread::Stop(){
    // 1.先关闭消费端句柄,让阻塞中的ProcessTrace返回
    if (traceHandle_ != INVALID_PROCESSTRACE_HANDLE) {
        CloseTrace(traceHandle_);
        traceHandle_ = INVALID_PROCESSTRACE_HANDLE;
    }
    
    // 2.再停止ETW会话
    if (sessionHandle_ != 0) {
        ControlTraceW(sessionHandle_, cfg::kSessionName, props_, EVENT_TRACE_CONTROL_STOP);
        sessionHandle_ = 0;
    }
}
