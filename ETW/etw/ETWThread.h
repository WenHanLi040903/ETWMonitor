#pragma once
#include <cstddef>	// size_t
#include <span>		// std::span(只读视图)
#include <thread>	// std::thread
#include <vector>
#include <windows.h>
#include <evntcons.h>	// PEVENT_RECORD 从这来(自动带入 evntrace.h)

class RawEvent{
    public:
    // 构造函数,传入PEVENT_RECORD
    RawEvent(PEVENT_RECORD pEvent);

    // 返回构造体是否完整
    bool IsValid() const noexcept { return valid_; }

    // 禁用按值拷贝:rec_ 里的指针指向本对象的 vector,
    // 一旦按值搬运,vector 会深拷到新地址而指针仍指旧对象的内存
    // (悬空且不崩溃,最难查),所以把运行期错误变成编译期错误
    RawEvent(const RawEvent&)            = delete;
    RawEvent& operator=(const RawEvent&) = delete;

    // 取原始记录指针,可直接当 PEVENT_RECORD 喂给 TdhGetEventInformation 等 API
    // (注意:会话级字段 UserContext/SessionGuid/KernelLogfile 不可依赖)
    PEVENT_RECORD precord() noexcept { return &rec_; }

    // 只读载荷视图:span 自带长度,遍历时不用手写 "下标 < UserDataLength"
    std::span<const BYTE> payload() const noexcept{
        return std::span<const BYTE>(userData_.data(), userData_.size());
    }

    // 扩展数据项只读视图:extendedData_ 是 vector<BYTE>,
    // 这里帮调用方把字节流解释成 EVENT_HEADER_EXTENDED_DATA_ITEM 数组,省掉每次手写 reinterpret_cast
    std::span<const EVENT_HEADER_EXTENDED_DATA_ITEM> extendedItems() const noexcept{
        return std::span<const EVENT_HEADER_EXTENDED_DATA_ITEM>(
            reinterpret_cast<const EVENT_HEADER_EXTENDED_DATA_ITEM*>(extendedData_.data()),
            extendedData_.size() / sizeof(EVENT_HEADER_EXTENDED_DATA_ITEM));
    }

    private:
    // 单事件载荷上限:挡住 Provider 报的垃圾长度,
    // 否则下面 assign 申请几个 GB 会抛 bad_alloc,异常穿过 ETW 回调会直接崩进程
    static constexpr size_t kMaxPayloadBytes = 1024 * 1024;

    EVENT_RECORD      rec_{};		// 值成员,必须放第一位保证对齐
    std::vector<BYTE> extendedData_;	// 扩展数据项数组
    std::vector<BYTE> userData_;	// 载荷
    bool              valid_ = false;	// 私有:只能在构造函数里置位
};

class ETWThread{
    public:
    ~ETWThread();				// 析构:确保采集线程被回收
    bool Start();				// 启动ETW线程
    void Stop();				// 停止ETW线程
    private:
    void Cleanup();				// 释放已获取的资源
    ULONG StopStaleSession();	// 清理残留的ETW会话
    ULONG CreateSession();		// 创建新的ETW会话
    ULONG EnableProviders();	// 启用(订阅)Providers
    void CreateETWProperties(); // 创建ETW会话配置单
    ULONG OpenRealTimeTrace();  // 打开实时会话(拿消费端句柄)
    static void WINAPI OnEvent(PEVENT_RECORD pEvent); // ETW回调函数
    static bool IsFailed(ULONG status, const wchar_t* what); // 检查Win32错误码
    std::vector<BYTE> propsBuf_; // 配置单存储空间
    EVENT_TRACE_PROPERTIES* props_ = nullptr; // 配置单指针
    CONTROLTRACE_ID sessionHandle_ = 0; // ETW会话ID(0=无会话)
    PROCESSTRACE_HANDLE traceHandle_ = INVALID_PROCESSTRACE_HANDLE; // 阻塞监听句柄
    std::thread worker_;		// 采集线程(跑ProcessTrace)
};
