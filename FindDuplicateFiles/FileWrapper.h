#pragma once

#include <windows.h>

class FileWrapper
{
public:
    explicit FileWrapper(HANDLE hFile) : handle(hFile) {}
    ~FileWrapper()
    {
        if (handle != INVALID_HANDLE_VALUE)
        {
            CloseHandle(handle);
        }
    }
    FileWrapper() = delete;
    FileWrapper(const FileWrapper&) = delete;
    FileWrapper(FileWrapper&&) = delete;
    FileWrapper& operator=(const FileWrapper&) = delete;
    FileWrapper& operator=(FileWrapper&&) = delete;

public:
    HANDLE handle;
};
