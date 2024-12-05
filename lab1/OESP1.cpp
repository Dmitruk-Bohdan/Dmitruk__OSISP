//#include <windows.h>
//#include <iostream>
//#include <vector>
//#include <string>
//
//struct ThreadParams {
//    HANDLE hFile;
//    DWORD startOffset;
//    DWORD bytesToRead;
//    std::vector<char>* buffer;
//};
//
//
//DWORD WINAPI ReadFileChunk(LPVOID lpParam) {
//    ThreadParams* params = static_cast<ThreadParams*>(lpParam);
//
//    SetFilePointer(params->hFile, params->startOffset, NULL, FILE_BEGIN);
//
//    DWORD bytesRead;
//    params->buffer->resize(params->bytesToRead);
//    ReadFile(params->hFile, params->buffer->data(), params->bytesToRead, &bytesRead, NULL);
//
//    return 0;
//}
//
//bool fileExists(const std::string& fileName) {
//
//    std::wstring wideFileName(fileName.begin(), fileName.end());
//    DWORD fileAttributes = GetFileAttributes(wideFileName.c_str());
//
//    if (fileAttributes == INVALID_FILE_ATTRIBUTES) {
//
//        return false;
//    }
//    return true;
//}
//
//void read_file_multithread(std::string fileName, int numThreads)
//{
//    HANDLE hFile = CreateFileA(fileName.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
//    if (hFile == INVALID_HANDLE_VALUE) {
//        std::cerr << "Error opening file.\n";
//        return;
//    }
//
//    LARGE_INTEGER fileSize;
//    GetFileSizeEx(hFile, &fileSize);
//
//    DWORD chunkSize = static_cast<DWORD>(fileSize.QuadPart / numThreads);
//    DWORD lastChunkSize = chunkSize + static_cast<DWORD>(fileSize.QuadPart % numThreads);
//
//    std::vector<HANDLE> threadHandles(numThreads);
//    std::vector<std::vector<char>> buffers(numThreads);
//
//    LARGE_INTEGER frequency, start, end;
//    QueryPerformanceFrequency(&frequency);
//    QueryPerformanceCounter(&start);
//
//    for (int i = 0; i < numThreads; ++i) {
//        DWORD currentChunkSize = (i == numThreads - 1) ? lastChunkSize : chunkSize;
//        ThreadParams* params = new ThreadParams{ hFile, i * chunkSize, currentChunkSize, &buffers[i] };
//
//        threadHandles[i] = CreateThread(NULL, 0, ReadFileChunk, params, 0, NULL);
//        if (threadHandles[i] == NULL) {
//            std::cerr << "Error while creating thread.\n";
//            return;
//        }
//    }
//
//    WaitForMultipleObjects(numThreads, threadHandles.data(), TRUE, INFINITE);
//
//    for (HANDLE threadHandle : threadHandles) {
//        CloseHandle(threadHandle);
//    }
//    CloseHandle(hFile);
//
//    QueryPerformanceCounter(&end);
//    double elapsedTime = static_cast<double>(end.QuadPart - start.QuadPart) / frequency.QuadPart;
//
//    std::vector<char> finalData;
//    for (const auto& buffer : buffers) {
//        finalData.insert(finalData.end(), buffer.begin(), buffer.end());
//    }
//
//    //std::string output(finalData.begin(), finalData.end());
//    //std::cout << output << std::endl;
//
//    std::cout << "File reading time: " << elapsedTime << " seconds\n";
//    std::cout << "Reading file size: " << finalData.size() << " bytes\n";
//}
//
//int main() {
//    std::string fileName;
//    int numThreads;
//
//    while (true)
//    {
//        std::cout << "Enter file name to read: ";
//        std::cin >> fileName;
//        std::cout << "Enter thread count: ";
//        std::cin >> numThreads;
//        read_file_multithread(fileName, numThreads);
//    }
// 
//    return 0;
//}
