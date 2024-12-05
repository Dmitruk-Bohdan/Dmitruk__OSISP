#include "QueueSystem.h"
#include <stdio.h>

// Глобальные переменные для статистики
CRITICAL_SECTION statsCriticalSection;
StageStats* globalStats = NULL;
SystemParameters globalParams = {0};
BOOL isSystemRunning = TRUE;

// Функция генерации случайного времени обработки
DWORD GetRandomProcessingTime(DWORD min, DWORD max) {
    return min + (rand() % (max - min + 1));
}

// Функция потока-генератора заявок
DWORD WINAPI RequestGenerator(LPVOID lpParam) {
    HANDLE bufferSemaphore = ((ChannelParams*)lpParam)->bufferSemaphore;
    HANDLE bufferMutex = ((ChannelParams*)lpParam)->bufferMutex;
    Request** buffer = ((ChannelParams*)lpParam)->buffer;
    DWORD* currentBufferSize = ((ChannelParams*)lpParam)->currentBufferSize;
    DWORD bufferSize = *((ChannelParams*)lpParam)->bufferSize;
    StageStats* stats = ((ChannelParams*)lpParam)->stats;
    
    DWORD requestId = 0;
    
    while (isSystemRunning) {
        Sleep(globalParams.requestGenerationRate);
        
        Request* newRequest = (Request*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(Request));
        if (!newRequest) continue;
        
        newRequest->id = requestId++;
        newRequest->creationTime = GetTickCount();
        newRequest->processingTime = GetRandomProcessingTime(
            globalParams.minProcessingTime,
            globalParams.maxProcessingTime
        );

        // Ожидание места в буфере
        if (WaitForSingleObject(bufferSemaphore, 100) == WAIT_OBJECT_0) {
            WaitForSingleObject(bufferMutex, INFINITE);
            
            buffer[*currentBufferSize] = newRequest;
            (*currentBufferSize)++;
            
            EnterCriticalSection(&statsCriticalSection);
            stats->totalRequests++;
            LeaveCriticalSection(&statsCriticalSection);
            
            ReleaseMutex(bufferMutex);
        } else {
            EnterCriticalSection(&statsCriticalSection);
            stats->droppedRequests++;
            LeaveCriticalSection(&statsCriticalSection);
            HeapFree(GetProcessHeap(), 0, newRequest);
        }
    }
    return 0;
}

// Функция потока-обработчика (канала)
DWORD WINAPI ChannelProcessor(LPVOID lpParam) {
    ChannelParams* params = (ChannelParams*)lpParam;
    DWORD stageId = params->stageId;
    DWORD channelId = params->channelId;
    
    while (isSystemRunning) {
        if (WaitForSingleObject(params->bufferSemaphore, 100) == WAIT_OBJECT_0) {
            WaitForSingleObject(params->bufferMutex, INFINITE);
            
            Request* request = NULL;
            if (*params->currentBufferSize > 0) {
                request = params->buffer[0];
                
                for (DWORD i = 0; i < *params->currentBufferSize - 1; i++) {
                    params->buffer[i] = params->buffer[i + 1];
                }
                (*params->currentBufferSize)--;
            }
            
            ReleaseMutex(params->bufferMutex);
            
            if (request) {
                DWORD startTime = GetTickCount();
                Sleep(request->processingTime);
                DWORD endTime = GetTickCount();
                
                EnterCriticalSection(&statsCriticalSection);
                params->stats->channelStats[channelId].processedRequests++;
                params->stats->channelStats[channelId].totalProcessingTime += 
                    endTime - startTime;
                LeaveCriticalSection(&statsCriticalSection);
                
                if (stageId < globalParams.stageCount - 1) {
                    // Логика передачи в следующую ступень
                }
                
                HeapFree(GetProcessHeap(), 0, request);
            }
        } else {
            EnterCriticalSection(&statsCriticalSection);
            params->stats->channelStats[channelId].idleTime += 100;
            LeaveCriticalSection(&statsCriticalSection);
        }
    }
    return 0;
}

// Функция вывода статистики
void PrintStatistics() {
    printf("\nSystem Statistics:\n");
    for (DWORD i = 0; i < globalParams.stageCount; i++) {
        printf("\nStage %d:\n", i + 1);
        printf("Total Requests: %d\n", globalStats[i].totalRequests);
        printf("Dropped Requests: %d\n", globalStats[i].droppedRequests);
        
        for (DWORD j = 0; j < globalParams.channelsPerStage[i]; j++) {
            printf("Channel %d:\n", j + 1);
            printf("  Processed Requests: %d\n", 
                globalStats[i].channelStats[j].processedRequests);
            printf("  Average Processing Time: %.2f ms\n",
                globalStats[i].channelStats[j].processedRequests ?
                (float)globalStats[i].channelStats[j].totalProcessingTime / 
                globalStats[i].channelStats[j].processedRequests : 0);
            printf("  Idle Time: %d ms\n", globalStats[i].channelStats[j].idleTime);
        }
    }
}

int main() {
    // Инициализация параметров системы
    globalParams.stageCount = 3;
    globalParams.channelsPerStage = (DWORD*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
        sizeof(DWORD) * globalParams.stageCount);
    globalParams.bufferSizes = (DWORD*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
        sizeof(DWORD) * globalParams.stageCount);
    
    for (DWORD i = 0; i < globalParams.stageCount; i++) {
        globalParams.channelsPerStage[i] = 2;  
        globalParams.bufferSizes[i] = 5;       // Размер буфера - 5 заявок
    }
    
    globalParams.requestGenerationRate = 1000;  // 1 заявка в секунду
    globalParams.minProcessingTime = 500;       // Минимальное время обработки
    globalParams.maxProcessingTime = 2000;      // Максимальное время обработки
    globalParams.simulationTime = 30000;        // 30 секунд симуляции
    
    // Инициализация критической секции
    InitializeCriticalSection(&statsCriticalSection);
    
    // Создание статистики
    globalStats = (StageStats*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
        sizeof(StageStats) * globalParams.stageCount);
    
    // Инициализация семафоров, мьютексов и буферов для каждой ступени
    HANDLE* stageSemaphores = (HANDLE*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
        sizeof(HANDLE) * globalParams.stageCount);
    HANDLE* stageMutexes = (HANDLE*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
        sizeof(HANDLE) * globalParams.stageCount);
    Request*** stageBuffers = (Request***)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
        sizeof(Request**) * globalParams.stageCount);
    DWORD** currentBufferSizes = (DWORD**)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
        sizeof(DWORD*) * globalParams.stageCount);
    
    // Создание потоков для каждого канала
    HANDLE** channelThreads = (HANDLE**)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
        sizeof(HANDLE*) * globalParams.stageCount);
    ChannelParams** channelParams = (ChannelParams**)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
        sizeof(ChannelParams*) * globalParams.stageCount);
    
    for (DWORD i = 0; i < globalParams.stageCount; i++) {
        stageSemaphores[i] = CreateSemaphore(NULL, globalParams.bufferSizes[i],
            globalParams.bufferSizes[i], NULL);
        stageMutexes[i] = CreateMutex(NULL, FALSE, NULL);
        
        stageBuffers[i] = (Request**)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
            sizeof(Request*) * globalParams.bufferSizes[i]);
        currentBufferSizes[i] = (DWORD*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
            sizeof(DWORD));
        
        channelThreads[i] = (HANDLE*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
            sizeof(HANDLE) * globalParams.channelsPerStage[i]);
        channelParams[i] = (ChannelParams*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
            sizeof(ChannelParams) * globalParams.channelsPerStage[i]);
        
        globalStats[i].channelStats = (ChannelStats*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
            sizeof(ChannelStats) * globalParams.channelsPerStage[i]);
        
        for (DWORD j = 0; j < globalParams.channelsPerStage[i]; j++) {
            channelParams[i][j].stageId = i;
            channelParams[i][j].channelId = j;
            channelParams[i][j].bufferSemaphore = stageSemaphores[i];
            channelParams[i][j].bufferMutex = stageMutexes[i];
            channelParams[i][j].buffer = stageBuffers[i];
            channelParams[i][j].bufferSize = &globalParams.bufferSizes[i];
            channelParams[i][j].currentBufferSize = currentBufferSizes[i];
            channelParams[i][j].stats = &globalStats[i];
            channelParams[i][j].params = &globalParams;
            channelParams[i][j].isRunning = &isSystemRunning;
            
            channelThreads[i][j] = CreateThread(NULL, 0, ChannelProcessor,
                &channelParams[i][j], 0, NULL);
        }
    }
    
    // Создание генератора заявок
    HANDLE generatorThread = CreateThread(NULL, 0, RequestGenerator,
        &channelParams[0][0], 0, NULL);
    
    // Ожидание завершения симуляции
    Sleep(globalParams.simulationTime);
    isSystemRunning = FALSE;
    
    // Ожидание завершения всех потоков
    WaitForSingleObject(generatorThread, INFINITE);
    for (DWORD i = 0; i < globalParams.stageCount; i++) {
        for (DWORD j = 0; j < globalParams.channelsPerStage[i]; j++) {
            WaitForSingleObject(channelThreads[i][j], INFINITE);
            CloseHandle(channelThreads[i][j]);
        }
    }
    
    PrintStatistics();
    
    DeleteCriticalSection(&statsCriticalSection);
    
    for (DWORD i = 0; i < globalParams.stageCount; i++) {
        CloseHandle(stageSemaphores[i]);
        CloseHandle(stageMutexes[i]);
        
        HeapFree(GetProcessHeap(), 0, stageBuffers[i]);
        HeapFree(GetProcessHeap(), 0, currentBufferSizes[i]);
        HeapFree(GetProcessHeap(), 0, channelThreads[i]);
        HeapFree(GetProcessHeap(), 0, channelParams[i]);
        HeapFree(GetProcessHeap(), 0, globalStats[i].channelStats);
    }
    
    HeapFree(GetProcessHeap(), 0, stageSemaphores);
    HeapFree(GetProcessHeap(), 0, stageMutexes);
    HeapFree(GetProcessHeap(), 0, stageBuffers);
    HeapFree(GetProcessHeap(), 0, currentBufferSizes);
    HeapFree(GetProcessHeap(), 0, channelThreads);
    HeapFree(GetProcessHeap(), 0, channelParams);
    HeapFree(GetProcessHeap(), 0, globalStats);
    HeapFree(GetProcessHeap(), 0, globalParams.channelsPerStage);
    HeapFree(GetProcessHeap(), 0, globalParams.bufferSizes);
    
    return 0;
}
