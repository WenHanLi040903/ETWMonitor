#include "ParserThread.h"

// 启动解析线程
void ParserThread::Start(){
    worker_ = std::thread(&ParserThread::Worker, this);
}

// 析构:确保解析线程被回收
ParserThread::~ParserThread(){
    Stop();
}

// 停止:先停队列(唤醒阻塞中的 Pop),再回收线程
// TODO: g_eventQueue.Stop();
//       if (worker_.joinable()) { worker_.join(); }
void ParserThread::Stop(){
    // TODO
}

// 消费循环
// TODO: while (true) {
//           std::shared_ptr<RawEvent> ev = g_eventQueue.Pop();
//           if (ev == nullptr) break;		// 已停止且排空
//           // 解析 ev -> JSON -> 输出
//       }
void ParserThread::Worker(){
    // TODO
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
