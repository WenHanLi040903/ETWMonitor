#include "RawEventQueue.h"

// 生产者:入队
// TODO: 加锁 -> deque_.push_back(ev) -> 解锁 -> cv_.notify_one()
void RawEventQueue::Push(std::shared_ptr<RawEvent> ev){
    (void)ev;	// TODO
}

// 消费者:阻塞取事件
// TODO: 加锁;cv_.wait(队列非空 或 已停止);
//       队列空且已停止 -> 返回 nullptr;
//       否则取出队头(pop_front)返回
std::shared_ptr<RawEvent> RawEventQueue::Pop(){
    return nullptr;	// TODO
}

// 停止:唤醒所有等待者
// TODO: 加锁 -> stopped_ = true -> 解锁 -> cv_.notify_all()
void RawEventQueue::Stop(){
    // TODO
}

// 当前积压数
// TODO: 加锁 -> 返回 deque_.size()
size_t RawEventQueue::Size() const{
    return 0;	// TODO
}
