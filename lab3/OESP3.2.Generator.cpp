#include <windows.h>
#include <iostream>
#include <vector>
#include <ctime>

#define PIPE_NAME L"\\\\.\\pipe\\DataPipe"

int main() {
    HANDLE hPipe;
    std::vector<int> data = { 23, 5, 89, 1, 42, 37 };

    hPipe = CreateNamedPipe(
        PIPE_NAME,
        PIPE_ACCESS_OUTBOUND,
        PIPE_TYPE_MESSAGE | PIPE_WAIT,
        1,
        1024,
        1024,
        0,
        NULL);

    if (hPipe == INVALID_HANDLE_VALUE) {
        std::cerr << "Failed to create pipe. Error: " << GetLastError() << std::endl;
        return 1;
    }

    std::cout << "Waiting for sorter to connect...\n";
    ConnectNamedPipe(hPipe, NULL);

    std::cout << "Sending data to sorter...\n";
    DWORD bytesWritten;
    for (int num : data) {
        WriteFile(hPipe, &num, sizeof(num), &bytesWritten, NULL);
    }

    int endSignal = -1;
    WriteFile(hPipe, &endSignal, sizeof(endSignal), &bytesWritten, NULL);

    CloseHandle(hPipe);
    std::cout << "Data sent.\n";

    system("pause");
    return 0;
}
