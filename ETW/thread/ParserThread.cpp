#include "ParserThread.h"
#include <atomic>	// std::atomic
#include <cstdio>	// std::wprintf / snprintf
#include <cstring>	// memcpy
#include <cwchar>	// wcsnlen
#include <iterator>	// std::size
#include <string>
#include <vector>
#include <iostream>
#include <windows.h>
#include <tdh.h>	// TdhGetProperty
#include <common/providers.h>
#include <json.hpp>

#pragma comment(lib, "tdh.lib")	// TDH 接口需要

// ================= 匿名命名空间:TDH 解析工具 =================
// 按"被使用"的顺序排列,避免前向声明:
//   WideToUtf8       —— 无依赖
//   ReadPropertyRaw  —— 无依赖
//   FormatValueUtf8  —— 用 WideToUtf8
//   GetEventFields   —— 用 ReadPropertyRaw / FormatValueUtf8
//   ParseEventToJson —— 用 GetEventFields / WideToUtf8
namespace{

    // 1.UTF-16转UTF-8
    std::string WideToUtf8(const wchar_t* wstr) {
        // 判空
        if (wstr == nullptr || wstr[0] == L'\0') {
            return std::string();
        }
        const int need = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, nullptr, 0, nullptr, nullptr);
        if (need <= 0) {
            std::cout << "宽字符串转换出错";
            return std::string();
        }
        std::string out(static_cast<size_t>(need), '\0');
        const int written = WideCharToMultiByte(CP_UTF8, 0, wstr, -1,out.data(), need, nullptr, nullptr);
        if (written <= 0) {
            return std::string();
        }
        out.resize(static_cast<size_t>(written) - 1);
        return out;
    }

    // 2.取单个字段的原始字节
    bool ReadPropertyRaw(PEVENT_RECORD pEvent,
        const providers::FieldDef& def,
        std::vector<BYTE>& out) {
        // 1) 组装描述符:告诉 TDH "我要哪个字段"
        PROPERTY_DATA_DESCRIPTOR desc{};
        desc.PropertyName = reinterpret_cast<ULONGLONG>(def.name);	// 宽字符串字段名(和 manifest 一致)
        desc.ArrayIndex = ULONG_MAX;	// 非数组字段必须传这个
        desc.Reserved = 0;			// 必须 0
        // 2) 第一趟:问 TDH 这个字段占多少字节
        ULONG size = 0;
        if (TdhGetPropertySize(pEvent, 0, nullptr, 1, &desc, &size) != ERROR_SUCCESS) {
            return false;		// 字段不存在
        }
        if (size == 0) {
            return false;
        }
        // 3) 按所需长度分配，多一个宽
        out.assign(static_cast<size_t>(size) + sizeof(wchar_t), '\0');
        // 4) 第二趟:真正取值
        if (TdhGetProperty(pEvent, 0, nullptr, 1, &desc,
            static_cast<ULONG>(out.size()), out.data()) != ERROR_SUCCESS) {
            return false;
        }
        return true;
    }

    // 3.原始字节 → UTF-8 文本
    std::string FormatValueUtf8(providers::FieldType type, const std::vector<BYTE>& raw) {
        char buf[64]{};

        switch (type) {
            // UTF-16原始字节转UTF-8
        case providers::FieldType::UnicodeString: {
            const wchar_t* p = reinterpret_cast<const wchar_t*>(raw.data());
            const size_t maxChars = raw.size() / sizeof(wchar_t);
            const size_t len = wcsnlen(p, maxChars);	// 找终止符,不越界
            return WideToUtf8(std::wstring(p, len).c_str());
        }
            // 32位整数转UTF-8
        case providers::FieldType::UInt32: {
            if (raw.size() < sizeof(ULONG)) { return std::string(); }
            ULONG v = 0;
            memcpy(&v, raw.data(), sizeof(v));
            snprintf(buf, sizeof(buf), "%lu", v);
            return buf;
        }
            // 64位无符号整数转UTF-8
        case providers::FieldType::UInt64: {
            if (raw.size() < sizeof(ULONGLONG)) { return std::string(); }
            ULONGLONG v = 0;
            memcpy(&v, raw.data(), sizeof(v));
            snprintf(buf, sizeof(buf), "%llu", v);
            return buf;
        }
            // 原始字节转可读时间
        case providers::FieldType::FILETIME: {
            if (raw.size() < sizeof(ULONGLONG)) { return std::string(); }
            ULONGLONG v = 0;
            memcpy(&v, raw.data(), sizeof(v));

            FILETIME ft{};
            ft.dwLowDateTime = static_cast<DWORD>(v & 0xFFFFFFFFULL);
            ft.dwHighDateTime = static_cast<DWORD>(v >> 32);

            SYSTEMTIME st{};
            if (!FileTimeToSystemTime(&ft, &st)) { return std::string(); }

            snprintf(buf, sizeof(buf), "%04u-%02u-%02u %02u:%02u:%02u",
                st.wYear, st.wMonth, st.wDay,
                st.wHour, st.wMinute, st.wSecond);
            return buf;
        }

        default:
            return std::string();
        }
    }

    // 4.按字段表批量取值,返回 UTF-8 值数组
    std::vector<std::string> GetEventFields(PEVENT_RECORD pEvent,const providers::FieldDef* defs,size_t count) {
        std::vector<std::string> result;
        result.reserve(count);		// 预分配,避免反复扩容

        for (size_t i = 0; i < count; ++i) {
            std::vector<BYTE> raw;

            // 取不到 → 放一个空串占位(不能跳过,否则后面错位)
            if (!ReadPropertyRaw(pEvent, defs[i], raw)) {
                result.emplace_back();
                continue;
            }

            // 取到了 → 按类型转 UTF-8 文本
            result.push_back(FormatValueUtf8(defs[i].type, raw));
        }

        return result;
    }

    // 5.输入事件指针，事件字段清单，返回解析后的json
    nlohmann::json ParseEventToJson(PEVENT_RECORD pEvent,
        const providers::FieldDef* defs,
        size_t count) {
        // 1) 按字段表取值(内部循环 ReadPropertyRaw + FormatValueUtf8)
        const std::vector<std::string> values = GetEventFields(pEvent, defs, count);

        // 2) 组装 JSON:字段名 → 值
        nlohmann::json j;
        for (size_t i = 0; i < count; ++i) {
            j[WideToUtf8(defs[i].name)] = values[i];
        }

        // 3) 返回
        return j;
    }

} // namespace

// ================= ParserThread 成员函数 =================

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

// 消费循环:取事件 → 按事件 ID 选字段表 → 解析成 JSON → 输出
void ParserThread::Worker(){
    static std::atomic<unsigned long long> count{ 0 };	// 成功解析的事件计数

    while (true) {
        // 1) 阻塞取一个事件(队列停且空时返回 nullptr)
        std::shared_ptr<RawEvent> ev = g_eventQueue.Pop();
        if (ev == nullptr) {
            break;		// 已停止且排空,退出
        }

        // 2) 按事件 ID 分派字段表,解析成 JSON
        const USHORT id = ev->Pevent()->EventHeader.EventDescriptor.Id;

        nlohmann::json j;
        if (id == 1) {	// ProcessStart
            j = ParseEventToJson(ev->Pevent(),
                                 providers::kProcessStart,
                                 std::size(providers::kProcessStart));
        }
        // TODO: id == 2 (ProcessStop) 等事件,补字段表后加在这里

        if (j.empty()) {
            continue;	// 没有对应字段表的事件,跳过
        }

        // 3) 输出解析结果
        const unsigned long long n = ++count;
        std::wprintf(L"[解析线程] 第 %llu 个事件: %hs\n", n, j.dump().c_str());
    }
}
