#pragma once

struct _LOG_CONTEXT
{
    UCHAR majorFunction;
    wchar_t* path;
    uint64_t procId;
    uint64_t thId;
    uint64_t fileObject;
    KIRQL irql;
    ULONG_PTR fileFlag;
    ULONG sizeWrite;
    FILE_INFORMATION_CLASS fileInfoType;
};

typedef struct _LIST_ENTRY_NODE
{
   LIST_ENTRY listEntry;
    _LOG_CONTEXT context;
} LIST_ENTRY_NODE, * PLIST_ENTRY_NODE;

struct _LOGGER_CONTEXT
{
    LIST_ENTRY listHead;
    HANDLE thread;
    LONG stopThread;
    KEVENT event;
    KSPIN_LOCK lock;
} g_loggerContext;

void pushLog(_LOG_CONTEXT* data, int size)
{
    PLIST_ENTRY_NODE logData = (PLIST_ENTRY_NODE)memory::commitKernelMemory(sizeof(LIST_ENTRY_NODE));

    if (logData)
    {
        memcpy(&logData->context, data, size);

        ExInterlockedInsertTailList(&g_loggerContext.listHead, &logData->listEntry, &g_loggerContext.lock);
        KeSetEvent(&g_loggerContext.event, 0, FALSE);
    }
}

NTSTATUS writeFileFromBuffer(UNICODE_STRING* filePath, PVOID buffer, ULONG bufferSize)
{
 
    OBJECT_ATTRIBUTES objectAttributes;
    InitializeObjectAttributes(&objectAttributes, filePath, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, nullptr, nullptr);

    IO_STATUS_BLOCK ioStatusBlock;
    HANDLE hFile;

    auto status = ZwCreateFile(
        &hFile,
        GENERIC_WRITE | SYNCHRONIZE,
        &objectAttributes,
        &ioStatusBlock,
        nullptr,
        FILE_ATTRIBUTE_NORMAL,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        FILE_OPEN_IF,
        FILE_SYNCHRONOUS_IO_NONALERT,
        nullptr,
        0
    );

    if (!NT_SUCCESS(status)) {
        TRACE("Error opening file: %08X\n", status);
        return status;
    }

    LARGE_INTEGER byteOffset;
    byteOffset.HighPart = -1;
    byteOffset.LowPart = FILE_WRITE_TO_END_OF_FILE;

    status = ZwWriteFile(
        hFile,
        nullptr,
        nullptr,
        nullptr,
        &ioStatusBlock,
        buffer,
        bufferSize,
        &byteOffset,
        nullptr
    );
 
    if (!NT_SUCCESS(status)) {
        TRACE("Error writing to file: %08X\n", status);
    }

    status = ZwClose(hFile);

    if (!NT_SUCCESS(status)) {
        TRACE("Error closing file: %08X\n", status);
    }

    return status;
}

void loggerManager()
{
    PLIST_ENTRY entry;

    while (!g_loggerContext.stopThread)
    {
        KeWaitForSingleObject(&g_loggerContext.event, Executive, KernelMode, FALSE, nullptr);

        while ((entry = ExInterlockedRemoveHeadList(&g_loggerContext.listHead, &g_loggerContext.lock)) != nullptr)
        { 
            PLIST_ENTRY_NODE node = CONTAINING_RECORD(entry, LIST_ENTRY_NODE, listEntry);

            auto bufferSize = wcslen(node->context.path) + 164;//default size str
            auto buffer = (char*)memory::commitKernelMemory(bufferSize);

            if (buffer)
            {
        
                UNICODE_STRING filePath = { 0 };
                RtlCreateUnicodeString(&filePath, node->context.path);
                ñonvertNtToDosPath(&filePath);

                auto len = 0;

                if (node->context.majorFunction == IRP_MJ_CREATE)
                {
                    auto createOptions = node->context.fileFlag;

                    if (createOptions & FILE_CREATE)
                    {
                        len = sprintf_s(buffer, bufferSize, "[FILE CREATE] - %wZ / Proccess id 0x%llx / Thread id 0x%llx / File object 0x%llx\n", filePath, node->context.procId, node->context.thId, node->context.fileObject);
                    }
                    else if (createOptions & FILE_OPEN)
                    {
                        len = sprintf_s(buffer, bufferSize, "[FILE OPEN] - %wZ / Proccess id 0x%llx / Thread id 0x%llx / File object 0x%llx\n", filePath, node->context.procId, node->context.thId, node->context.fileObject);
                    }
                    else 
                    {
                        len = sprintf_s(buffer, bufferSize, "[FILE OVER WRITE] - %wZ / Proccess id 0x%llx / Thread id 0x%llx / File object 0x%llx\n", filePath, node->context.procId, node->context.thId, node->context.fileObject);
                    }
                }
                else if (node->context.majorFunction == IRP_MJ_CLOSE || node->context.majorFunction == IRP_MJ_CLEANUP)
                {
                    len = sprintf_s(buffer, bufferSize, "[FILE CLOSE] - %wZ / Proccess id 0x%llx / Thread id 0x%llx / File object 0x%llx\n", filePath, node->context.procId, node->context.thId, node->context.fileObject);
                }
                else if (node->context.majorFunction == IRP_MJ_WRITE)
                {
                    len = sprintf_s(buffer, bufferSize, "[FILE WRITE] - %wZ / Proccess id 0x%llx / Thread id 0x%llx / File object 0x%llx / Irql %u / Write size %u\n", filePath, node->context.procId, node->context.thId, node->context.fileObject, node->context.irql, node->context.sizeWrite);
                }
                else if (node->context.majorFunction == IRP_MJ_SET_INFORMATION)
                {
                    len = sprintf_s(buffer, bufferSize, "[FILE SET INFORMATION] - %wZ / Proccess id 0x%llx / Thread id 0x%llx / File object 0x%llx / Irql %u / Type info %i\n", filePath, node->context.procId, node->context.thId, node->context.fileObject, node->context.irql, node->context.fileInfoType);
                }

                if (len)
                {
                    writeFileFromBuffer(&g_traceLog, buffer, len);
                }

                RtlFreeUnicodeString(&filePath);
                memory::freeKernelMemory(buffer);
            }

            memory::freeKernelMemory(node->context.path);
            memory::freeKernelMemory(node);

            if (g_loggerContext.stopThread)
            {
                break;
            }
        }
    }

   PsTerminateSystemThread(STATUS_SUCCESS);
}