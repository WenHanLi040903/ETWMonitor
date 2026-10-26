#pragma once
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include "data/RawEvent.h"	// RawEvent

// 线程安全的阻塞队列:生产者 Push,消费者 Pop
class RawEventQueue{
    public:
    void Push(std::shared_ptr<RawEvent> ev);	// 生产者:入队(不阻塞)
    std::shared_ptr<RawEvent> Pop();			// 消费者:阻塞取;停止且空返回 nullptr
    void Stop();								// 停止:唤醒所有等待者
    size_t Size() const;						// 当前积压数(调试用)

    private:
    std::deque<std::shared_ptr<RawEvent>> deque_;	// 双端队列
    mutable std::mutex      mutex_;				// 保护 deque_(mutable 供 const 函数加锁)
    std::condition_variable cv_;				// 队列为空时等待
    bool                    stopped_ = false;	// 停止标志
};

// 全局唯一实例(C++17 inline 变量,头文件定义不冲突)
inline RawEventQueue g_eventQueue;
