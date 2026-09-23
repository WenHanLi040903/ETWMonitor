#pragma once

#include <windows.h>
#include <guiddef.h>
// 创建一个命名空间防止变量名重复
namespace providers{
    inline constexpr GUID kKernelFile =
    	{0xedd08927, 0x9cc4, 0x4e65, {0xb9, 0x70, 0xc2, 0x56, 0x0f, 0xb5, 0xc2, 0x89}};
    inline constexpr GUID kPowerShell =
    	{0xa0c1853b, 0x5c40, 0x4b15, {0x87, 0x66, 0x3c, 0xf1, 0xc5, 0x8f, 0x98, 0x5a}};
    inline constexpr GUID kKernelProcess =
    	{0x22fb2cd6, 0x0e7b, 0x422b, {0xa0, 0xc7, 0x2f, 0xad, 0x1f, 0xd0, 0xe7, 0x16}};
    inline constexpr GUID kKernelNetwork =
    	{0x7dd42a49, 0x5329, 0x4832, {0x8d, 0xfd, 0x43, 0xd9, 0x79, 0x15, 0x3a, 0x88}};
    
    struct ProviderInfo {
        const GUID* guid;      // Provider GUID 指针
        ULONGLONG   maskCode;  // 事件掩码（MatchAnyKeyword）,0 = 全部
    };
    
    inline constexpr ProviderInfo kProviderList[] = {
        //{ &kKernelFile,    0x3b0},
        //{ &kPowerShell,    0x1},
        { &kKernelProcess, 0x10},
        //{ &kKernelNetwork, 0x30},
    };
    
}// namespace providers
