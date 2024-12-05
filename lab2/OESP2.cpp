// OESP2.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
// Обновляем файл с реализацией асинхронной обработки файлов

#include <windows.h>
#include <iostream>
#include <vector>
#include <chrono>
#include <thread>
#include <algorithm>
#include <numeric>

constexpr size_t BUFFER_SIZE = 1024 * 1024; 
constexpr size_t NUM_BUFFERS = 4; 
constexpr size_t FILE_SIZE = 100 * 1024 * 1024;

struct IOContext {
    OVERLAPPED overlapped;
    std::vector<double> buffer;
    bool isRead;
    HANDLE event;
};

struct Statistics {
    double sum = 0.0;
    double min = std::numeric_limits<double>::max();
    double max = std::numeric_limits<double>::lowest();
    size_t count = 0;
} globalStats;

VOID CALLBACK IoCompletionRoutine(
    DWORD dwErrorCode,
    DWORD dwNumberOfBytesTransfered,
    LPOVERLAPPED lpOverlapped
) {
    IOContext* context = CONTAINING_RECORD(lpOverlapped, IOContext, overlapped);
    SetEvent(context->event);
}

void ProcessDataSync(const std::wstring& filename) {
    auto start = std::chrono::high_resolution_clock::now();

    HANDLE hFile = CreateFile(
        filename.c_str(),
        GENERIC_READ,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        std::cerr << "Failed to open file\n";
        return;
    }

    std::vector<double> buffer(BUFFER_SIZE / sizeof(double));
    DWORD bytesRead;
    Statistics stats;

    while (ReadFile(hFile, buffer.data(), BUFFER_SIZE, &bytesRead, NULL) && bytesRead > 0) {
        size_t numElements = bytesRead / sizeof(double);
        for (size_t i = 0; i < numElements; ++i) {
            stats.sum += buffer[i];
            stats.min = std::min(stats.min, buffer[i]);
            stats.max = std::max(stats.max, buffer[i]);
        }
        stats.count += numElements;
    }

    CloseHandle(hFile);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "Synchronous processing completed in " << duration.count() << "ms\n";
    std::cout << "Average: " << (stats.count > 0 ? stats.sum / stats.count : 0)
              << "\nMin: " << stats.min
              << "\nMax: " << stats.max << "\n";
}

void ProcessDataAsync(const std::wstring& filename) {
    auto start = std::chrono::high_resolution_clock::now();

    HANDLE hFile = CreateFile(
        filename.c_str(),
        GENERIC_READ,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
        NULL
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        std::cerr << "Failed to open file\n";
        return;
    }

    std::vector<IOContext> contexts(NUM_BUFFERS);
    for (auto& ctx : contexts) {
        ctx.buffer.resize(BUFFER_SIZE / sizeof(double));
        ctx.isRead = true;
        ctx.event = CreateEvent(NULL, TRUE, FALSE, NULL);
        ZeroMemory(&ctx.overlapped, sizeof(OVERLAPPED));
    }

    for (size_t i = 0; i < NUM_BUFFERS; ++i) {
        contexts[i].overlapped.Offset = i * BUFFER_SIZE;
        ReadFileEx(
            hFile,
            contexts[i].buffer.data(),
            BUFFER_SIZE,
            &contexts[i].overlapped,
            IoCompletionRoutine
        );
    }

    Statistics stats;
    DWORD currentBuffer = 0;
    LARGE_INTEGER fileSize;
    GetFileSizeEx(hFile, &fileSize);
    DWORD remainingBytes = fileSize.QuadPart;

    while (remainingBytes > 0) {
        WaitForSingleObject(contexts[currentBuffer].event, INFINITE);
        ResetEvent(contexts[currentBuffer].event);

        size_t numElements = std::min(BUFFER_SIZE, (size_t)remainingBytes) / sizeof(double);
        for (size_t i = 0; i < numElements; ++i) {
            stats.sum += contexts[currentBuffer].buffer[i];
            stats.min = std::min(stats.min, contexts[currentBuffer].buffer[i]);
            stats.max = std::max(stats.max, contexts[currentBuffer].buffer[i]);
        }
        stats.count += numElements;

        DWORD nextOffset = contexts[currentBuffer].overlapped.Offset + NUM_BUFFERS * BUFFER_SIZE;
        if (nextOffset < fileSize.QuadPart) {
            contexts[currentBuffer].overlapped.Offset = nextOffset;
            ReadFileEx(
                hFile,
                contexts[currentBuffer].buffer.data(),
                BUFFER_SIZE,
                &contexts[currentBuffer].overlapped,
                IoCompletionRoutine
            );
        }

        remainingBytes -= std::min(BUFFER_SIZE, remainingBytes);
        currentBuffer = (currentBuffer + 1) % NUM_BUFFERS;
    }

    for (auto& ctx : contexts) {
        CloseHandle(ctx.event);
    }
    CloseHandle(hFile);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "Asynchronous processing completed in " << duration.count() << "ms\n";
    std::cout << "Average: " << (stats.count > 0 ? stats.sum / stats.count : 0)
              << "\nMin: " << stats.min
              << "\nMax: " << stats.max << "\n";
}

void ProcessDataMultithreaded(const std::wstring& filename) {
    auto start = std::chrono::high_resolution_clock::now();

    HANDLE hFile = CreateFile(
        filename.c_str(),
        GENERIC_READ,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        std::cerr << "Failed to open file\n";
        return;
    }

    const size_t numThreads = std::thread::hardware_concurrency();
    std::vector<std::thread> threads;
    std::vector<Statistics> threadStats(numThreads);
    
    LARGE_INTEGER fileSize;
    GetFileSizeEx(hFile, &fileSize);
    size_t chunkSize = (fileSize.QuadPart + numThreads - 1) / numThreads;

    for (size_t i = 0; i < numThreads; ++i) {
        threads.emplace_back([&, i]() {
            std::vector<double> buffer(BUFFER_SIZE / sizeof(double));
            DWORD bytesRead;
            LARGE_INTEGER offset;
            offset.QuadPart = i * chunkSize;

            SetFilePointerEx(hFile, offset, NULL, FILE_BEGIN);
            size_t remainingBytes = std::min(chunkSize, (size_t)(fileSize.QuadPart - offset.QuadPart));

            while (remainingBytes > 0 && ReadFile(hFile, buffer.data(), 
                   std::min(BUFFER_SIZE, remainingBytes), &bytesRead, NULL)) {
                size_t numElements = bytesRead / sizeof(double);
                for (size_t j = 0; j < numElements; ++j) {
                    threadStats[i].sum += buffer[j];
                    threadStats[i].min = std::min(threadStats[i].min, buffer[j]);
                    threadStats[i].max = std::max(threadStats[i].max, buffer[j]);
                }
                threadStats[i].count += numElements;
                remainingBytes -= bytesRead;
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    Statistics finalStats;
    for (const auto& stats : threadStats) {
        finalStats.sum += stats.sum;
        finalStats.min = std::min(finalStats.min, stats.min);
        finalStats.max = std::max(finalStats.max, stats.max);
        finalStats.count += stats.count;
    }

    CloseHandle(hFile);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "Multithreaded processing completed in " << duration.count() << "ms\n";
    std::cout << "Average: " << (finalStats.count > 0 ? finalStats.sum / finalStats.count : 0)
              << "\nMin: " << finalStats.min
              << "\nMax: " << finalStats.max << "\n";
}

void GenerateTestFile(const std::wstring& filename) {
    HANDLE hFile = CreateFile(
        filename.c_str(),
        GENERIC_WRITE,
        0,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        std::cerr << "Failed to create test file\n";
        return;
    }

    std::vector<double> buffer(BUFFER_SIZE / sizeof(double));
    size_t remainingBytes = FILE_SIZE;
    DWORD bytesWritten;

    while (remainingBytes > 0) {
        // Generate random doubles
        for (auto& val : buffer) {
            val = (double)rand() / RAND_MAX;
        }

        WriteFile(
            hFile,
            buffer.data(),
            std::min(BUFFER_SIZE, remainingBytes),
            &bytesWritten,
            NULL
        );

        remainingBytes -= bytesWritten;
    }

    CloseHandle(hFile);
}

int main() {
    const std::wstring filename = L"testdata.bin";

    std::cout << "Generating test file...\n";
    GenerateTestFile(filename);

    std::cout << "\nTesting synchronous processing:\n";
    ProcessDataSync(filename);

    std::cout << "\nTesting asynchronous processing:\n";
    ProcessDataAsync(filename);

    std::cout << "\nTesting multithreaded processing:\n";
    ProcessDataMultithreaded(filename);

    DeleteFile(filename.c_str());
    return 0;
}
