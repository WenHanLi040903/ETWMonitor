#pragma once
#include <windows.h>	// ULONG 从这来
#include <guiddef.h>
#include <evntrace.h>	// EVENT_TRACE_REAL_TIME_MODE 从这来

namespace cfg{
    // ETW会话GUID{9F939802-BBC8-4ADD-8BD0-E3980DF86B77}
    inline constexpr GUID kSessionGuid = 
    { 0x9f939802, 0xbbc8, 0x4add, { 0x8b, 0xd0, 0xe3, 0x98, 0xd, 0xf8, 0x6b, 0x77 } };
    inline constexpr wchar_t kSessionName[]  = L"EtwMonitorSession";	// 会话名
    inline constexpr ULONG kBufferSizeKB  = 64;    						// 单个缓冲区大小（KB）
    inline constexpr ULONG kMinBuffers    = 20;    						// 最小缓冲区数
    inline constexpr ULONG kMaxBuffers    = 200;   						// 最大缓冲区数
    inline constexpr ULONG kFlushTimerSec = 1;     						// 缓冲区刷新间隔（秒）
    inline constexpr ULONG kLogFileMode = EVENT_TRACE_REAL_TIME_MODE; 	// 时间监控模式，不写etl文件
}// namespace cfg