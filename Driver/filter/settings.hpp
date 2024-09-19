#pragma once

FLT_POSTOP_CALLBACK_STATUS postFileCallBack(_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_opt_ PVOID CompletionContext,
	_In_ FLT_POST_OPERATION_FLAGS Flags);

NTSTATUS FLTAPI fsUnload(__in FLT_FILTER_UNLOAD_FLAGS Flags);

struct FileContext
{
	wchar_t* Path;
	int Len;
};

inline UNICODE_STRING g_procPath = { 0 };
inline UNICODE_STRING g_filePath = { 0 };
inline UNICODE_STRING g_traceLog = { 0 };
inline PFLT_FILTER g_filterHandle = nullptr;

void cleanup(PFLT_CONTEXT context, FLT_CONTEXT_TYPE contextType)
{
	if (contextType == FLT_STREAMHANDLE_CONTEXT)
	{
		memory::freeKernelMemory(((FileContext*)context)->Path);
	}
}

NTSTATUS ñonvertDosToNtPath(UNICODE_STRING* dosPath) //this mem
{
	OBJECT_ATTRIBUTES objectAttributes;
    InitializeObjectAttributes(&objectAttributes, dosPath, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, nullptr, nullptr);

	HANDLE hFile = 0;
	IO_STATUS_BLOCK ioStatusBlock = { 0 };
    auto status = ZwOpenFile(&hFile, GENERIC_READ, &objectAttributes, &ioStatusBlock, FILE_SHARE_READ, FILE_SYNCHRONOUS_IO_NONALERT);

    if (!NT_SUCCESS(status))
	{
        return status;
    }

	PFILE_OBJECT fileObject;
    status = ObReferenceObjectByHandle(hFile, FILE_READ_DATA, *IoFileObjectType, KernelMode, (PVOID*)&fileObject, nullptr);

    if (!NT_SUCCESS(status)) 
	{
        ZwClose(hFile);
        return status;
    }

	PFLT_FILE_NAME_INFORMATION fileNameInfo = nullptr;
	FltGetFileNameInformationUnsafe(fileObject, nullptr, FLT_FILE_NAME_OPENED | FLT_FILE_NAME_QUERY_ALWAYS_ALLOW_CACHE_LOOKUP, &fileNameInfo);

	if (fileNameInfo)
	{
		RtlFreeUnicodeString(dosPath);
		RtlDuplicateUnicodeString(RTL_DUPLICATE_UNICODE_STRING_NULL_TERMINATE, &fileNameInfo->Name, dosPath);
	}
	
	FltReleaseFileNameInformation(fileNameInfo);
    ObDereferenceObject(fileObject);
    ZwClose(hFile);
	
    return status;
}

NTSTATUS ñonvertNtToDosPath(UNICODE_STRING* ntPath) //this mem v2
{
	OBJECT_ATTRIBUTES objectAttributes;
	InitializeObjectAttributes(&objectAttributes, ntPath, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, nullptr, nullptr);

	HANDLE hFile = 0;
	IO_STATUS_BLOCK ioStatusBlock = { 0 };

	auto status = ZwCreateFile(
		&hFile,
		GENERIC_READ | SYNCHRONIZE,
		&objectAttributes,
		&ioStatusBlock,
		nullptr,
		FILE_ATTRIBUTE_NORMAL,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		FILE_OPEN,
		FILE_SYNCHRONOUS_IO_NONALERT,
		nullptr,
		0
	);

	if (!NT_SUCCESS(status))
	{
		return status;
	}

	PFILE_OBJECT fileOfbject;
	status = ObReferenceObjectByHandle(hFile, FILE_READ_DATA, *IoFileObjectType, KernelMode, (PVOID*)&fileOfbject, nullptr);

	if (!NT_SUCCESS(status))
	{
		ZwClose(hFile);
		return status;
	}

	POBJECT_NAME_INFORMATION fileNameInfo = nullptr;
	IoQueryFileDosDeviceName(fileOfbject, &fileNameInfo);

	if (fileNameInfo)
	{
		RtlFreeUnicodeString(ntPath);

		RtlDuplicateUnicodeString(RTL_DUPLICATE_UNICODE_STRING_NULL_TERMINATE, &fileNameInfo->Name, ntPath);
	}

	memory::freeKernelMemory(fileNameInfo);
	ObDereferenceObject(fileOfbject);
	ZwClose(hFile);

	return status;
}

NTSTATUS setupSettings(PUNICODE_STRING registryPath)
{
	//\REGISTRY\MACHINE\SYSTEM\ControlSet001\Services\DriverTest\

	auto registryCopy = (wchar_t*)memory::commitKernelMemory(registryPath->Length);
	memcpy(registryCopy, registryPath->Buffer, registryPath->Length);

	if (!NT_SUCCESS(registry::readStr(registryCopy, L"ProcPath", &g_procPath)))
	{
		memory::freeKernelMemory(registryCopy);
		return STATUS_ACCESS_DENIED;
	}

	if (!NT_SUCCESS(registry::readStr(registryCopy, L"FilePath", &g_filePath)))
	{
		memory::freeKernelMemory(registryCopy);
		return STATUS_ACCESS_DENIED;
	}

	if (!NT_SUCCESS(registry::readStr(registryPath->Buffer, L"TraceLog", &g_traceLog)))
	{
		memory::freeKernelMemory(registryCopy);
		return STATUS_ACCESS_DENIED;
	}

	memory::freeKernelMemory(registryCopy);

	ñonvertDosToNtPath(&g_procPath);
	ñonvertDosToNtPath(&g_filePath);

	return STATUS_SUCCESS;
}