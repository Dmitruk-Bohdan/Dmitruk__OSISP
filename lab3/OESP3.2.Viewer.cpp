#include <windows.h>
#include <iostream>
#include <vector>

#define PIPE_NAME L"\\\\.\\pipe\\SortedDataPipe"

int main() {
    HANDLE hPipe;
    hPipe = CreateFile(
        PIPE_NAME,
        GENERIC_READ,
        0,
        NULL,
        OPEN_EXISTING,
        0,
        NULL);

    if (hPipe == INVALID_HANDLE_VALUE) {
        std::cerr << "Failed to open pipe. Error: " << GetLastError() << std::endl;
        return 1;
    }

    DWORD bytesRead;
    int value;

    std::cout << "Sorted data:\n";
    while (ReadFile(hPipe, &value, sizeof(value), &bytesRead, NULL) && value != -1) {
        std::cout << value << " ";
    }

    std::cout << "\n";
    CloseHandle(hPipe);

    system("pause");
    return 0;
}
