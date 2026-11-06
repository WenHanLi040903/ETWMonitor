#pragma once
#include <thread>
#include <windows.h>
#include <evntcons.h>	// PEVENT_RECORD / EVENT_RECORD(其实 ParserThread.h 已带入)
#include <tdh.h>		// TdhGetProperty 等
#include "data/RawEvent.h"	// RawEvent
#include "queue/RawEventQueue.h"	// RawEventQueue / g_eventQueue

// 解析线程:从队列取事件,TDH 解析,输出 JSON
class ParserThread{
    public:
    void Start();			// 启动解析线程
    ~ParserThread();
    void Stop();			// 停止并回收
    private:
    void Worker();			// 消费循环
    std::thread worker_;	// 解析线程
};
