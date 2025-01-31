// FindDuplicateFiles.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <windows.h>
#include <iostream>
#include <io.h>
#include <fcntl.h>
#include <string>
#include <vector>
#include <list>
#include <map>
#include <algorithm>
#include <optional>
#include "FileWrapper.h"
#include "SearchHandleWrapper.h"
#include "Heartbeat.h"

struct FileData
{
    uint64_t size;
    std::wstring name;
    std::wstring fullPath;

    FileData(const WIN32_FIND_DATA& findData, const std::wstring& parentDirectoryPath)
        : size((uint64_t(findData.nFileSizeHigh) << 32) | uint64_t(findData.nFileSizeLow))
        , name(findData.cFileName)
        , fullPath(parentDirectoryPath + L"\\" + std::wstring(findData.cFileName))
    {
    }

    bool operator==(const FileData& other)
    {
        return size == other.size && fullPath == other.fullPath;
    }
};

using FileDataList = std::list<FileData>;

struct DuplicateData
{
    FileDataList files;
    std::optional<size_t> warningNotFullComparison;
};

class Scanner
{
public:
    explicit Scanner(const std::list<std::wstring>& paths)
        : mHeartbeat(10000)
    {
        for (const auto& path : paths)
        {
            std::wstring editedPath = path;
            if (*editedPath.rbegin() == L'\\')
            {
                editedPath = editedPath.substr(0, editedPath.size() - 1);
            }
            mPaths.push_back(editedPath);
        }
    }
    Scanner() = delete;
    Scanner(const Scanner&) = delete;
    Scanner(Scanner&&) = delete;
    Scanner& operator=(const Scanner&) = delete;
    Scanner& operator=(Scanner&&) = delete;

    void Scan()
    {
        mCandidatesCount = 0;
        mCandidatesMap.clear();
        mDuplicates.clear();
        
        ScanAllDirectories();
        mCandidatesCount = PrintAndCountCandidates();
        mMaxBufferSize = GetMaxBufferSize(mCandidatesMap.crbegin()->first);
        CheckDuplicateCandidates();
    }

private:
    void ScanAllDirectories()
    {
        std::wcout << L"[Stage 1] Stage start: Scan all files. Will scan " << mPaths.size() << L" directories:\n";
        for (const auto& path : mPaths)
        {
            std::wcout << L"    " << path << L"\n";
        }

        for (const auto& path : mPaths)
        {
            ScanDirectory(path);
        }
        std::wcout << L"[Stage 1] Stage end: Scan all files\n";
    }

    void ScanDirectory(const std::wstring& directory)
    {
        std::wcout << L"[Stage 1] ScanDirectory: " << directory.c_str() << L"\n";
        mHeartbeat.Reset();
        if (std::wcout.fail())
        {
            std::wcout.clear();
        }
        std::list<std::wstring> subDirectories;
        {
            WIN32_FIND_DATA findData{};
            std::wstring searchFilter = directory + L"\\*";
            SearchHandleWrapper search(FindFirstFileEx(searchFilter.c_str(), FindExInfoBasic, &findData, FindExSearchNameMatch, NULL, 0));
            BOOL found = search.handle != INVALID_HANDLE_VALUE;
            while (found)
            {
                const bool isDirectory = (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
                if (isDirectory)
                {
                    if (wcscmp(findData.cFileName, L".") != 0 && wcscmp(findData.cFileName, L"..") != 0)
                    {
                        std::wstring fullPath = directory + L"\\" + std::wstring(findData.cFileName);
                        subDirectories.push_back(fullPath);
                    }
                }
                else
                {
                    FileData fileData(findData, directory);
                    std::wcout << L"[Stage 1] Found file: " << fileData.name << L"\n";
                    mHeartbeat.Reset();
                    if (std::wcout.fail())
                    {
                        std::wcout.clear();
                    }
                    auto it = mCandidatesMap.find(fileData.size);
                    if (it == mCandidatesMap.end())
                    {
                        std::pair<uint64_t, FileDataList> pair(fileData.size, FileDataList(1, fileData));
                        mCandidatesMap.insert(pair);
                    }
                    else
                    {
                        FileDataList& fileDataList = it->second;
                        if (std::find(fileDataList.begin(), fileDataList.end(), fileData) == fileDataList.end())
                        {
                            fileDataList.push_back(fileData);
                        }
                        else
                        {
                            std::wcout << L"    [Stage 1] Warning: skipping FileData which is already in the list: " << fileData.fullPath << L"\n";
                        }
                    }
                }
                found = FindNextFileW(search.handle, &findData);
            }
        }

        for (const auto& subDirectory : subDirectories)
        {
            ScanDirectory(subDirectory);
        }
    }

    size_t PrintAndCountCandidates()
    {
        size_t candidatesCount = 0;
        std::wcout << L"==========\n";
        std::wcout << L"\n";
        std::wcout << L"\n";
        size_t totalKeys = mCandidatesMap.size();
        size_t currentKey = 0;
        std::wcout << L"[Stage 2] Stage start: Print candidates. Found " << totalKeys << L" different file sizes:\n";
        mHeartbeat.Reset();
        for (auto it = mCandidatesMap.crbegin(); it != mCandidatesMap.crend(); ++it) // from largest to smallest file
        {
            ++currentKey;
            if (mHeartbeat.CheckAndReset())
            {
                std::wcout << L"    [Stage 2] Heartbeat: checking number of files with size: " << it->first << L" (" << currentKey << L"/" << totalKeys << L")\n";
            }
            if (it->second.size() < 2)
            {
                continue;
            }
            std::wcout << L"[Stage 2] Candidates with size " << it->first << L":\n";
            std::wcout << L"{\n";
            for (const auto& fileData : it->second)
            {
                ++candidatesCount;
                std::wcout << L"    " << fileData.fullPath << L"\n";
            }
            std::wcout << L"}\n";
            mHeartbeat.Reset();
        }
        std::wcout << L"\n";
        std::wcout << L"\n";
        std::wcout << L"[Stage 2] Stage end: Print candidates. Number of candidates: " << candidatesCount << L"\n";
        std::wcout << L"==========\n";
        std::wcout << L"\n";
        std::wcout << L"\n";
        mHeartbeat.Reset();
        return candidatesCount;
    }

    DWORD GetClusterSize()
    {
        std::wstring rootPath = mPaths.begin()->substr(0, 2) + L"\\";
        DWORD sectorsPerCluster = 0;
        DWORD bytesPerSector = 0;
        DWORD numberOfFreeClusters = 0;
        DWORD totalNumberOfClusters = 0;
        BOOL result = GetDiskFreeSpace(rootPath.c_str(), &sectorsPerCluster, &bytesPerSector, &numberOfFreeClusters, &totalNumberOfClusters);
        if (result)
        {
            std::wcout << L"Info: GetDiskFreeSpace for '" << rootPath << L"', sectorsPerCluster: " << sectorsPerCluster << L", bytesPerSector: " << bytesPerSector << L"\n";
            return sectorsPerCluster * bytesPerSector;
        }
        std::wcout << L"Warning: GetDiskFreeSpace failed for '" << rootPath << L"', GetLastError: " << GetLastError() << L"\n";
        return 1024;
    }

    void CheckDuplicateCandidates()
    {
        std::wcout << L"[Stage 3] Stage start: Compare candidates\n";
        mHeartbeat.Reset();

        mCandidatesProcessed = 0;
        mTotalDuplicatesSize = 0;
        mClusterSize = GetClusterSize();
        for (auto it = mCandidatesMap.crbegin(); it != mCandidatesMap.crend(); ++it) // from largest to smallest file
        {
            if (it->second.size() < 2)
            {
                continue;
            }
            CheckDuplicateCandidates(it->second);
        }
        std::wcout << L"[Stage 3] Stage end: Compare candidates\n";
        mHeartbeat.Reset();
    }

    HANDLE FileOpenForRead(const std::wstring& filePath)
    {
        return CreateFile(
            filePath.c_str(),
            GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL
        );
    }

    std::vector<uint8_t> FileRead(const std::wstring& filePath, DWORD requestedSize)
    {
        std::vector<uint8_t> buffer;
        FileWrapper file(FileOpenForRead(filePath));
        if (file.handle != INVALID_HANDLE_VALUE)
        {
            buffer.resize(requestedSize);
            DWORD readSize = 0;
            if (ReadFile(file.handle, static_cast<void*>(&buffer[0]), requestedSize, &readSize, NULL))
            {
                buffer.resize(readSize);
            }
            else
            {
                DWORD error = GetLastError();
                std::wcout << L"ReadFile error(" << error << L"):" << filePath << L"\n";
            }
        }
        return buffer;
    }

    DWORD GetMaxBufferSize(const uint64_t maxFileSize)
    {
        constexpr uint64_t MAX_READ = 1024 * 1024 * 1024; // 1 GB
        DWORD startSize = static_cast<DWORD>(min(MAX_READ, maxFileSize));
        for (DWORD candidateSize = startSize; candidateSize > 0; candidateSize /= 2)
        {
            try
            {
                std::vector<uint8_t> buffer1;
                std::vector<uint8_t> buffer2;
                buffer1.resize(candidateSize);
                buffer2.resize(candidateSize);
                return candidateSize;
            }
            catch (const std::exception&)
            {
            }
        }
        return 4096;
    }

    DWORD GetReadSizeForFileSize(const uint64_t fileSize)
    {
        if (mMaxBufferSize.has_value())
        {
            return static_cast<DWORD>(min(uint64_t(*mMaxBufferSize), fileSize));
        }
        return 1024;
    }


    void CheckDuplicateCandidates(const FileDataList& candidates)
    {
        std::list<std::wstring> markedAsDuplicate;
        for (FileDataList::const_iterator originalIt = candidates.cbegin(); originalIt != candidates.cend(); ++originalIt)
        {
            ++mCandidatesProcessed;
            if (markedAsDuplicate.cend() != std::find(markedAsDuplicate.cbegin(), markedAsDuplicate.cend(), originalIt->fullPath))
            {
                continue;
            }
            DuplicateData duplicateData;
            duplicateData.files.push_back(*originalIt);
            const DWORD readSize = GetReadSizeForFileSize(originalIt->size);
            if (readSize != originalIt->size)
            {
                duplicateData.warningNotFullComparison = readSize;
            }
            std::vector<uint8_t> originalContentBeginning = FileRead(originalIt->fullPath, mClusterSize);

            FileDataList::const_iterator candidateIt = originalIt;
            ++candidateIt;
            for (; candidateIt != candidates.cend(); ++candidateIt)
            {
                bool isMatch = false;
                std::vector<uint8_t> candidateContentBeginning = FileRead(candidateIt->fullPath, mClusterSize);
                if (originalContentBeginning == candidateContentBeginning)
                {
                    if (originalIt->size == originalContentBeginning.size())
                    {
                        isMatch = true;
                    }
                    else
                    {
                        std::vector<uint8_t> originalContent = FileRead(originalIt->fullPath, readSize);
                        std::vector<uint8_t> candidateContent = FileRead(originalIt->fullPath, readSize);
                        if (originalContent == candidateContent)
                        {
                            isMatch = true;
                        }
                    }
                    if (isMatch)
                    {
                        std::wcout << L"[Stage 3] Match detected: " << originalIt->size << L" (candidate " << mCandidatesProcessed << L"/" << mCandidatesCount << ")\n";
                        std::wcout << L"    " << originalIt->fullPath << L"\n";
                        std::wcout << L"    " << candidateIt->fullPath << L"\n";
                        mHeartbeat.Reset();
                        markedAsDuplicate.push_back(candidateIt->fullPath);
                        duplicateData.files.push_back(*candidateIt);
                    }
                }

                if (!isMatch && mHeartbeat.CheckAndReset())
                {
                    std::wcout << L"    [Stage 3] Heartbeat: comparing files with size: " << originalIt->size << L" (candidate " << mCandidatesProcessed << L"/" << mCandidatesCount << L"\n";
                    std::wcout << L"        " << originalIt->fullPath << L"\n";
                    std::wcout << L"        " << candidateIt->fullPath << L"\n";
                }
            }

            if (duplicateData.files.size() > 1)
            {
                mTotalDuplicatesSize += duplicateData.files.begin()->size * (duplicateData.files.size() - 1);
                mDuplicates.push_back(std::move(duplicateData));
            }
        }
    }

public:
    std::list<std::wstring> mPaths;
    std::map<uint64_t, FileDataList> mCandidatesMap;
    size_t mCandidatesCount = 0;
    size_t mCandidatesProcessed = 0;
    std::list<DuplicateData> mDuplicates;
    uint64_t mTotalDuplicatesSize = 0;
    DWORD mClusterSize = 1024;
    std::optional<DWORD> mMaxBufferSize;
    Heartbeat mHeartbeat;
};


int main()
{
    if (-1 == _setmode(_fileno(stdout), _O_U16TEXT))
    {
        std::wcout << L"Warning: _setmode failed, errno=" << errno << L"\n";
    }
    LPWSTR cmdLine = GetCommandLineW();

    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(cmdLine, &argc);
    if (argc < 2)
    {
        std::wcout << L"Usage: it must be one argument or more arguments - full paths to the folders which has to be scanned for duplicate files\n";
        return 1;
    }

    std::list<std::wstring> pathsToScan;
    for (int i = 1; i < argc; ++i)
    {
        pathsToScan.push_back(std::wstring(argv[i]));
    }
    LocalFree(argv);

    Scanner scanner(pathsToScan);
    scanner.Scan();
    std::wcout << L"\n";
    std::wcout << L"\n";
    std::wcout << L"==========\n";
    std::wcout << L"\n";
    std::wcout << L"\n";
    std::wcout << L"Printing final results: " << scanner.mDuplicates.size() << L" files which have duplicates\n";
    for (const auto& entry : scanner.mDuplicates)
    {
        std::wcout << L"Duplicate with size: " << entry.files.begin()->size;
        if (entry.warningNotFullComparison.has_value())
        {
            std::wcout << L" (first " << *scanner.mMaxBufferSize << L" bytes match)\n";
        }
        else
        {
            std::wcout << L" (full match)\n";
        }
        for (const auto& fileData : entry.files)
        {
            std::wcout << L"    " << fileData.fullPath << L"\n";
        }
    }
    std::wcout << L"Printing final results finished.\n\n";
    std::wcout << L"All tasks finished. Summary:\n";
    std::wcout << scanner.mDuplicates.size() << L" duplicate files, " << scanner.mTotalDuplicatesSize << L" bytes can be saved after removing duplicates.\n";
    return 0;
}
