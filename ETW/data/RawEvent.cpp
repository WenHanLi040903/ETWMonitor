#include "RawEvent.h"

// 实现RawEvent构造函数,把一个 ETW 回调给的临时事件深度拷贝成自包含对象
// 只能在 OnEvent 回调内构造 —— 回调返回后 pEvent 指向的整块内存都会失效
RawEvent::RawEvent(PEVENT_RECORD pEvent){
    // ---- 1) 判空 ----
    if (pEvent == nullptr) {
        return;					// valid_ 保持 false
    }

    // ---- 2) 壳子先拷过来(里面的指针还指着 ETW 缓冲区,最后统一修) ----
    rec_ = *pEvent;

    // ---- 3) 拷贝 UserData ----
    // pEvent->UserData 是 void*,不允许指针运算,所以解释成 BYTE*(一单位 = 1 字节)
    // pEvent->UserDataLength 是载荷所占字节数
    // 校验:有长度却空指针 → 坏数据,直接退出
    if (pEvent->UserDataLength > 0 && pEvent->UserData == nullptr) {
        return;
    }

    if (pEvent->UserDataLength > 0) {
        userData_.assign(reinterpret_cast<const BYTE*>(pEvent->UserData),
                         reinterpret_cast<const BYTE*>(pEvent->UserData) + pEvent->UserDataLength);
    }

    // ---- 4) 拷贝 ExtendedData ----
    // ExtendedData 指向结构体数组:共 ExtendedDataCount 项,每项 sizeof(EVENT_HEADER_EXTENDED_DATA_ITEM) 字节
    const size_t extCount = pEvent->ExtendedDataCount;
    const size_t extBytes = extCount * sizeof(EVENT_HEADER_EXTENDED_DATA_ITEM);

    // 校验:有项数却空指针 → 坏数据
    if (extCount > 0 && pEvent->ExtendedData == nullptr) {
        return;
    }

    if (extBytes > 0) {
        extendedData_.assign(reinterpret_cast<const BYTE*>(pEvent->ExtendedData),
                             reinterpret_cast<const BYTE*>(pEvent->ExtendedData) + extBytes);
        EVENT_HEADER_EXTENDED_DATA_ITEM* ext =
            reinterpret_cast<EVENT_HEADER_EXTENDED_DATA_ITEM*>(extendedData_.data());

        // 源载荷区间 [srcBegin, srcEnd)
        const BYTE* srcBegin = reinterpret_cast<const BYTE*>(pEvent->UserData);
        const BYTE* srcEnd   = srcBegin + pEvent->UserDataLength;

        for (size_t i = 0; i < extCount; ++i) {
            // 读源
            const BYTE* srcData =
                reinterpret_cast<const BYTE*>(pEvent->ExtendedData[i].DataPtr);

            // 源为空,或数据不在载荷区间内
            if (srcData == nullptr || srcData < srcBegin || srcData >= srcEnd) {
                ext[i].DataPtr = 0;
                continue;
            }

            // 算相对载荷首地址的偏移,再换到新基址上
            const size_t off = static_cast<size_t>(srcData - srcBegin);
            ext[i].DataPtr = reinterpret_cast<ULONGLONG>(userData_.data() + off);
        }
    }

    rec_.UserData     = userData_.empty() ? nullptr : userData_.data();
    rec_.ExtendedData = extendedData_.empty()
                      ? nullptr
                      : reinterpret_cast<PEVENT_HEADER_EXTENDED_DATA_ITEM>(extendedData_.data());
    rec_.UserContext  = nullptr;

    valid_ = true;
}
