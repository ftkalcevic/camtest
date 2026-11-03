// PocLog.h : logging for the API proof of concept.
//
// Every line goes to %LOCALAPPDATA%\camtest\poc.log and is flushed at once, so
// the log survives a crash. Lines logged on the UI thread also appear in the
// list box on the CAM tab.

#pragma once

void PocLogInit();
void PocLog(const wchar_t* format, ...);

std::wstring PocLogFolder();
std::wstring PocLogPath();

void PocLogAddList(HWND hList);
void PocLogRemoveList(HWND hList);
