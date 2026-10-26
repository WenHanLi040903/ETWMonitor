#include "ParserThread.h"
#include <atomic>	// std::atomic
#include <cstdio>	// std::wprintf

// 启动解析线程
void ParserThread::Start(){
    worker_ = std::thread(&ParserThread::Worker, this);
}

// 析构:确保解析线程被回收
ParserThread::~ParserThread(){
    Stop();
}

// 停止:先停队列(唤醒阻塞中的 Pop),再回收线程
void ParserThread::Stop(){
    g_eventQueue.Stop();		// 置停止标志并唤醒阻塞中的 Pop

    if (worker_.joinable()) {
        worker_.join();			// 等 Worker 排空队列后退出
    }
}

// 消费循环(当前:只取数据并输出,解析后续再加)
void ParserThread::Worker(){
    static std::atomic<unsigned long long> count{ 0 };	// 解析线程的处理计数

    while (true) {
        // 1) 阻塞取一个事件(队列停且空时返回 nullptr)
        std::shared_ptr<RawEvent> ev = g_eventQueue.Pop();
        if (ev == nullptr) {
            break;		// 已停止且排空,退出
        }

        // 2) 输出事件信息 + 当前队列积压(验证链路用)
        const unsigned long long n = ++count;
        std::wprintf(L"[解析线程] 第 %llu 个事件 EventId=%u, 队列积压=%llu\n",
                     n,
                     static_cast<unsigned>(ev->Pevent()->EventHeader.EventDescriptor.Id),
                     static_cast<unsigned long long>(g_eventQueue.Size()));
    }
}

// ================= TDH 解析(待实现) =================
// 以下函数放在本文件的匿名命名空间里,只在本文件内可见
//
// TODO: std::string WideToUtf8(const wchar_t* wstr);
//         UTF-16 -> UTF-8
//
// TODO: bool ReadPropertyRaw(PEVENT_RECORD pEvent,
//                            const providers::FieldDef& def,
//                            std::vector<BYTE>& out);
//         TdhGetProperty 两趟调用:先探长度,再取值
//
// TODO: std::string FormatValueUtf8(providers::FieldType type,
//                                   const std::vector<BYTE>& raw);
//         按类型转 UTF-8 文本(UInt32 / UInt64 / UnicodeString / FILETIME)
//
// TODO: std::vector<std::string> GetEventFields(PEVENT_RECORD pEvent,
//                                               const providers::FieldDef* defs,
//                                               size_t count);
//         按字段表批量取值
//
// TODO: nlohmann::json ParseEventToJson(PEVENT_RECORD pEvent);
//         按事件 ID 分派字段表 -> 取值 -> 组装 JSON
//         需要 #include <tdh.h>、json.hpp 和 "common/providers.h"
