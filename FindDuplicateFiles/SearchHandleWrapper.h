#pragma once

#include <windows.h>

class SearchHandleWrapper
{
public:
    explicit SearchHandleWrapper(HANDLE hSearch) : handle(hSearch) {}
    ~SearchHandleWrapper()
    {
        if (handle != INVALID_HANDLE_VALUE)
        {
            FindClose(handle);
        }
    }
    SearchHandleWrapper() = delete;
    SearchHandleWrapper(const SearchHandleWrapper&) = delete;
    SearchHandleWrapper(SearchHandleWrapper&&) = delete;
    SearchHandleWrapper& operator=(const SearchHandleWrapper&) = delete;
    SearchHandleWrapper& operator=(SearchHandleWrapper&&) = delete;

public:
    HANDLE handle;
};
