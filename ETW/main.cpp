#include <windows.h>
#include <shellapi.h>	// ShellExecuteExW

#include <clocale>	// std::setlocale
#include <cstdio>	// std::wprintf
#include <string>	// std::wstring

#include "etw/ETWThread.h"

#pragma comment(lib, "shell32.lib")	// ShellExecuteExW 需要
#pragma comment(lib, "advapi32.lib")	// OpenProcessToken / ETW API 需要

namespace {

// 检测当前进程是否以管理员权限运行
bool IsElevated(){
    HANDLE token = nullptr;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token) == FALSE) {
        return false;
    }

    TOKEN_ELEVATION elevation{};
    DWORD size = sizeof(elevation);
    const BOOL ok = GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size);

    CloseHandle(token);
    return ok != FALSE && elevation.TokenIsElevated != 0;
}

// 会弹出 UAC 提权窗口
// 提权成功后本进程应立即退出,由新的提权进程接管
bool RelaunchAsAdmin(){
    wchar_t exePath[MAX_PATH]{};
    if (GetModuleFileNameW(nullptr, exePath, MAX_PATH) == 0) {
        return false;
    }

    // 取 exe 所在目录,保证提权后的进程工作目录一致
    std::wstring exeDir = exePath;
    const size_t pos = exeDir.find_last_of(L"\\/");
    if (pos != std::wstring::npos) {
        exeDir.resize(pos);
    }

    SHELLEXECUTEINFOW sei{};
    sei.cbSize      = sizeof(sei);
    sei.lpVerb      = L"runas";		// runas = 请求提权,触发 UAC
    sei.lpFile      = exePath;
    sei.lpDirectory = exeDir.c_str();
    sei.nShow       = SW_SHOWNORMAL;

    return ShellExecuteExW(&sei) != FALSE;
}

} // namespace

int main(){
    // ---- 控制台按 UTF-8 输出,保证中文正常显示 ----
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    std::setlocale(LC_ALL, ".UTF-8");

    // ---- ETW 内核 Provider 需要管理员权限 ----
    if (!IsElevated()) {
        std::wprintf(L"当前没有管理员权限,正在请求提权...\n");
        if (RelaunchAsAdmin()) {
            return 0;	// 提权后的新进程已启动,本进程退出
        }
        std::wprintf(L"提权被取消或失败,程序退出。\n");
        return 1;
    }

    // ---- 启动ETW:Start()同步完成setup,内部再开线程阻塞收事件 ----
    ETWThread etw;
    if (!etw.Start()) {
        std::wprintf(L"启动失败,程序退出。\n");
        return 1;
    }

    std::wprintf(L"正在采集事件,按任意键停止...\n");
    (void)std::getchar();	// 主线程等用户按键(返回值故意忽略)

    // ---- 停止:CloseTrace解除ProcessTrace阻塞,并回收采集线程 ----
    etw.Stop();

    return 0;
}
