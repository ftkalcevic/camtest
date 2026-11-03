// PocLog.cpp : logging for the API proof of concept.

#include "stdafx.h"
#include "PocLog.h"

namespace
{
    std::mutex g_mutex;
    std::wstring g_folder;
    std::vector<HWND> g_lists;
    DWORD g_uiThread = 0;

    const int kMaxListLines = 2000;
}

std::wstring PocLogFolder()
{
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_folder.empty())
    {
        wchar_t buffer[MAX_PATH] = {};
        DWORD len = GetEnvironmentVariableW(L"LOCALAPPDATA", buffer, MAX_PATH);
        if (len == 0 || len >= MAX_PATH)
            len = GetTempPathW(MAX_PATH, buffer);

        g_folder.assign(buffer, len);
        if (!g_folder.empty() && g_folder.back() != L'\\')
            g_folder += L'\\';
        g_folder += L"camtest\\";
        CreateDirectoryW(g_folder.c_str(), nullptr);
    }
    return g_folder;
}

std::wstring PocLogPath()
{
    return PocLogFolder() + L"poc.log";
}

void PocLogInit()
{
    g_uiThread = GetCurrentThreadId();
    PocLog(L"==== CAM POC loaded ====");
}

void PocLog(const wchar_t* format, ...)
{
    wchar_t message[2048] = {};
    va_list args;
    va_start(args, format);
    _vsnwprintf_s(message, _countof(message), _TRUNCATE, format, args);
    va_end(args);

    SYSTEMTIME now = {};
    GetLocalTime(&now);

    wchar_t line[2200] = {};
    _snwprintf_s(line, _countof(line), _TRUNCATE, L"%02u:%02u:%02u.%03u [%lu] %s",
                 now.wHour, now.wMinute, now.wSecond, now.wMilliseconds,
                 GetCurrentThreadId(), message);

    const std::wstring path = PocLogPath();

    std::lock_guard<std::mutex> lock(g_mutex);

    FILE* file = nullptr;
    if (_wfopen_s(&file, path.c_str(), L"a, ccs=UTF-8") == 0 && file != nullptr)
    {
        fputws(line, file);
        fputws(L"\n", file);
        fclose(file);
    }

    // Only touch the list boxes on the UI thread. Events could arrive on
    // another thread, and that thread id is itself logged above.
    if (GetCurrentThreadId() != g_uiThread)
        return;

    for (HWND hList : g_lists)
    {
        if (!::IsWindow(hList))
            continue;

        ::SendMessageW(hList, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(line));
        LRESULT count = ::SendMessageW(hList, LB_GETCOUNT, 0, 0);
        if (count > kMaxListLines)
        {
            ::SendMessageW(hList, LB_DELETESTRING, 0, 0);
            --count;
        }
        ::SendMessageW(hList, LB_SETTOPINDEX, static_cast<WPARAM>(count - 1), 0);
    }
}

void PocLogAddList(HWND hList)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    g_lists.push_back(hList);
}

void PocLogRemoveList(HWND hList)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    g_lists.erase(std::remove(g_lists.begin(), g_lists.end(), hList), g_lists.end());
}
