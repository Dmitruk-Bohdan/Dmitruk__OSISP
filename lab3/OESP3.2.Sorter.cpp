#include <windows.h>
#include <iostream>
#include <vector>
#include <algorithm>

#define PIPE_NAME_IN L"\\\\.\\pipe\\DataPipe"
#define PIPE_NAME_OUT L"\\\\.\\pipe\\SortedDataPipe"

int main() {
    HANDLE hPipeIn, hPipeOut;
    hPipeIn = CreateFile(
        PIPE_NAME_IN,
        GENERIC_READ,
        0,
        NULL,
        OPEN_EXISTING,
        0,
        NULL);

    if (hPipeIn == INVALID_HANDLE_VALUE) {
        std::cerr << "Failed to open input pipe. Error: " << GetLastError() << std::endl;
        return 1;
    }

    hPipeOut = CreateNamedPipe(
        PIPE_NAME_OUT,
        PIPE_ACCESS_OUTBOUND,
        PIPE_TYPE_MESSAGE | PIPE_WAIT,
        1,
        1024,
        1024,
        0,
        NULL);

    if (hPipeOut == INVALID_HANDLE_VALUE) {
        std::cerr << "Failed to create output pipe. Error: " << GetLastError() << std::endl;
        return 1;
    }

    ConnectNamedPipe(hPipeOut, NULL);

    std::vector<int> data;
    DWORD bytesRead;
    int value;

    std::cout << "Reading data from generator...\n";
    while (ReadFile(hPipeIn, &value, sizeof(value), &bytesRead, NULL) && value != -1) {
        data.push_back(value);
    }

    std::cout << "Sorting data...\n";
    std::sort(data.begin(), data.end());

    std::cout << "Sending sorted data to viewer...\n";
    DWORD bytesWritten;
    for (int num : data) {
        WriteFile(hPipeOut, &num, sizeof(num), &bytesWritten, NULL);
    }

    int endSignal = -1;
    WriteFile(hPipeOut, &endSignal, sizeof(endSignal), &bytesWritten, NULL);

    CloseHandle(hPipeIn);
    CloseHandle(hPipeOut);

    system("pause");
    return 0;
}
