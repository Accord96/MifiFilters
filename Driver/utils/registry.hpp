#pragma once

namespace registry
{
    NTSTATUS open(const wchar_t* path, PHANDLE handle)
    {
        UNICODE_STRING destinationString;
        RtlInitUnicodeString(&destinationString, path);

        OBJECT_ATTRIBUTES objectAttributes;
        InitializeObjectAttributes(&objectAttributes, &destinationString, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);

        return ZwOpenKey(handle, 0xF003Fu, &objectAttributes);
    }

    NTSTATUS setValue(const wchar_t* path, const wchar_t* valName, void* value, uint32_t size, int32_t type)
    {
        HANDLE regKey = 0;
        auto status = open(path, &regKey);

        if (!NT_SUCCESS(status))
        {
            return status;
        }

        UNICODE_STRING atribName = { 0 };
        RtlInitUnicodeString(&atribName, valName);

        status = ZwSetValueKey(regKey, &atribName, 0, type, value, size);

        ZwClose(regKey);

        return status;
    }

    NTSTATUS readStr(const wchar_t* path, const wchar_t* valName, UNICODE_STRING* readStr)
    {
        HANDLE regKey = 0;
        auto status = open(path, &regKey);

        if (!NT_SUCCESS(status))
        {
            return status;
        }

        UNICODE_STRING atribName = { 0 };
        RtlInitUnicodeString(&atribName, valName);

        ULONG sizeStr = 0;
        status = ZwQueryValueKey(regKey, &atribName, KeyValuePartialInformation, 0, 0, &sizeStr);

        if (status != STATUS_BUFFER_TOO_SMALL)
        {
            return status;
        }

        auto buff = (PKEY_VALUE_PARTIAL_INFORMATION)memory::commitKernelMemory(sizeStr + 10);

        if (!buff)
        {
            return STATUS_MEMORY_NOT_ALLOCATED;
        }

        status = ZwQueryValueKey(regKey, &atribName, KeyValuePartialInformation, buff, sizeStr, &sizeStr);

        if (!NT_SUCCESS(status))
        {
            return status;
        }

        memmove(buff->Data + 8, buff->Data, sizeStr);
        memmove(buff->Data, L"\\??\\", 8);

        RtlCreateUnicodeString(readStr, (wchar_t*)buff->Data);

        memory::freeKernelMemory(buff);

        ZwClose(regKey);

        return status;
    }
}