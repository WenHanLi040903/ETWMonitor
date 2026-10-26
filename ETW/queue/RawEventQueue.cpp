#include "RawEventQueue.h"

// 生产者:入队
void RawEventQueue::Push(std::shared_ptr<RawEvent> ev){
    {
        std::lock_guard<std::mutex> lock(mutex_);
        deque_.push_back(std::move(ev));
    }							// 锁在这里释放
    cv_.notify_one();			// 锁外唤醒一个消费者(减少被唤醒线程的争锁)
}

// 消费者:阻塞取事件
std::shared_ptr<RawEvent> RawEventQueue::Pop(){
    std::unique_lock<std::mutex> lock(mutex_);	// unique_lock:wait 需要临时放锁/加锁
    cv_.wait(lock, [this]{ return !deque_.empty() || stopped_; });

    if (deque_.empty()) {
        return nullptr;		// 已停止且排空,通知调用方退出
    }

    std::shared_ptr<RawEvent> ev = std::move(deque_.front());
    deque_.pop_front();		// 取出队头
    return ev;
}

// 停止:唤醒所有等待者
void RawEventQueue::Stop(){
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopped_ = true;		// 置停止标志(锁内,与 Pop 的检查互斥)
    }							// 锁在这里释放
    cv_.notify_all();			// 锁外唤醒全部(让所有等待者醒来检查)
}

// 当前积压数
size_t RawEventQueue::Size() const{
    std::lock_guard<std::mutex> lock(mutex_);	// 读也要加锁(可能和 Push/Pop 并发)
    return deque_.size();
}
