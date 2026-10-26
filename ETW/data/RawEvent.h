#pragma once
#include <cstddef>	// size_t
#include <vector>
#include <windows.h>
#include <evntcons.h>	// PEVENT_RECORD / EVENT_RECORD 从这来

// ETW 事件的深拷贝容器:把回调里的临时事件复制成自包含对象
// 只能在本对象内部保管数据:rec_ 里的指针指向 userData_ / extendedData_
class RawEvent{
    public:
    	RawEvent(PEVENT_RECORD pEvent);
    	bool IsValid() const noexcept { return valid_; }
    	PEVENT_RECORD Pevent() noexcept { return &rec_; } // 这里返回拷贝后的指针
    private:
    	EVENT_RECORD rec_{}; // 这个属性用来存放EVENT_RECORD
    	std::vector<BYTE> extendedData_;
    	std::vector<BYTE> userData_;
    	bool valid_ = false; // 标识位，标识对象是否已完成拷贝
};
