#include "ETWThread.h"
#include "common/cfg.h"
#include "common/providers.h"
#include <cwchar>	// std::wcslen
#include <cstring>	// memcpy
#include <cstdio>	// std::wprintf
#include <atomic>	// 打印计数

// 实现IsFailed(),检查Win32错误码
// 成功返回false;失败则打印错误信息并返回true
bool ETWThread::IsFailed(ULONG status, const wchar_t* what){
    if (status == ERROR_SUCCESS) {
        return false;
    }
    
    // 把错误码翻译成系统描述
    LPWSTR sysMsg = nullptr;
    DWORD len = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, status, 0,
        reinterpret_cast<LPWSTR>(&sysMsg),	// ALLOCATE_BUFFER时这里要传指针的地址
        0, nullptr);
    
    // FormatMessage返回的文本通常以\r\n结尾,裁掉
    while (len > 0 && (sysMsg[len - 1] == L'\r' || sysMsg[len - 1] == L'\n')) {
        sysMsg[--len] = L'\0';
    }
    
    std::wprintf(L"[错误] %s 失败, 错误码 %lu (0x%08lX): %s\n",
                 what, status, status,
                 (len > 0) ? sysMsg : L"(无系统描述)");
    
    if (sysMsg != nullptr) {
        LocalFree(sysMsg);	// FormatMessage用LocalAlloc分配,必须LocalFree
    }
    return true;
}

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
ULONG ETWThread::StopStaleSession(){
    ULONG status = ControlTraceW(sessionHandle_,	// 初始化是0,创建前清理是无句柄,析构时则有句柄
                                 cfg::kSessionName, 				// 配置常量中的会话名
                                 props_,					// 配置单地址
                                 EVENT_TRACE_CONTROL_STOP			// 停止会话控制code
                                );
    
    if (status == ERROR_WMI_INSTANCE_NOT_FOUND) {
        return ERROR_SUCCESS;
    }
    
    IsFailed(status, L"ControlTraceW 清理残留会话");
    return status;
}

// 实现CreatSession(),开启ETW会话
ULONG ETWThread::CreateSession(){
    // 先调用会话清理函数清理残留会话
    ULONG status = StopStaleSession();
    if (status != ERROR_SUCCESS) {
        return status;
    }
    
    // 创建新的会话
    status = StartTraceW(&sessionHandle_,
                         cfg::kSessionName,
                         props_);
    
    IsFailed(status, L"StartTraceW 创建会话");
    return status;
}


// 实现EnableProviders(),启用多个Provider事件提供者
ULONG ETWThread::EnableProviders(){
    for (const auto& p : providers::kProviderList) {
        ULONG status = EnableTraceEx2(sessionHandle_, p.guid,
                                      EVENT_CONTROL_CODE_ENABLE_PROVIDER,
                                      TRACE_LEVEL_INFORMATION,
                                      p.maskCode, 0, 0, nullptr);
        if (IsFailed(status, L"EnableTraceEx2 订阅Provider")) {
            return status;
        }
    }
    return ERROR_SUCCESS;
}

// 实现OpenRealTimeTrace(),开启阻塞监听事件
ULONG ETWThread::OpenRealTimeTrace(){
    EVENT_TRACE_LOGFILE logFile{};
    logFile.LoggerName          = const_cast<LPWSTR>(cfg::kSessionName);	// 要接的实时会话名
    logFile.ProcessTraceMode    = PROCESS_TRACE_MODE_REAL_TIME | PROCESS_TRACE_MODE_EVENT_RECORD;
    logFile.EventRecordCallback = &ETWThread::OnEvent;	// 事件回调
    logFile.Context             = this;	// 传给回调,回调里从 pEvent->UserContext 取回

    traceHandle_ = OpenTraceW(&logFile);
    if (traceHandle_ == INVALID_PROCESSTRACE_HANDLE) {
        // OpenTraceW失败不返回错误码,原因要从GetLastError取
        ULONG status = GetLastError();
        IsFailed(status, L"OpenTraceW 打开实时会话");
        return status;
    }
    return ERROR_SUCCESS;
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


// 实现Cleanup(),释放所有已获取的资源,顺序不可颠倒
void ETWThread::Cleanup(){
    // 1.先关闭消费端句柄,让阻塞中的ProcessTrace返回
    if (traceHandle_ != INVALID_PROCESSTRACE_HANDLE) {
        CloseTrace(traceHandle_);
        traceHandle_ = INVALID_PROCESSTRACE_HANDLE;
    }
    
    // 2.回收采集线程(此时ProcessTrace已返回,join立刻成功)
    if (worker_.joinable()) {
        worker_.join();
    }
    
    // 3.再停止ETW会话
    if (sessionHandle_ != 0) {
        ControlTraceW(sessionHandle_, cfg::kSessionName, props_, EVENT_TRACE_CONTROL_STOP);
        sessionHandle_ = 0;
    }
}

// 析构:确保采集线程被回收
// 若worker_析构时仍是joinable状态,std::thread会直接调用std::terminate终止进程
ETWThread::~ETWThread(){
    Cleanup();	// Cleanup()每一步都有if保护,重复调用安全
}

// 实现Start(),启动ETW线程
bool ETWThread::Start(){
    // 1.生成ETW会话配置单
    CreateETWProperties();
    
    // 2.创建并启动会话(内部会先清理残留会话)
    if (CreateSession() != ERROR_SUCCESS) {
        Cleanup();	// 回滚:把已经拿到的资源全放掉
        return false;
    }
    
    // 3.订阅Provider
    if (EnableProviders() != ERROR_SUCCESS) {
        Cleanup();
        return false;
    }
    
    // 4.打开实时会话,拿到消费端句柄
    if (OpenRealTimeTrace() != ERROR_SUCCESS) {
        Cleanup();
        return false;
    }
    
    // 5.开线程去阻塞收事件
    //   ProcessTrace会一直卡在worker_里,直到Stop()从别的线程调用CloseTrace
    worker_ = std::thread([this]{
        ProcessTrace(&traceHandle_, 1, nullptr, nullptr);
    });
    
    return true;
}

// 实现Stop(),停止ETW线程
void ETWThread::Stop(){
    // 正常停止:目前就是释放资源
    // 将来如果需要"排空队列"之类的收尾,加在这里,不影响Start()的回滚路径
    Cleanup();
}
