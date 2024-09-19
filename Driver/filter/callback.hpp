#pragma once

bool filterProcTarget(PFLT_CALLBACK_DATA data)
{
	if (PsGetCurrentProcess() == PsInitialSystemProcess)
	{
		return false;
	}

	auto curProc = IoGetCurrentProcess();

	if (!curProc)
	{
		return false;
	}

	auto process = PsGetThreadProcess(data->Thread);

	if (!process)
	{
		return false;
	}

	PUNICODE_STRING imageFileName = 0;
	SeLocateProcessImageName(process, &imageFileName);

	if (!RtlEqualUnicodeString(imageFileName, &g_procPath, true))
	{	
		memory::freeKernelMemory(imageFileName);
		return false;
	}

	memory::freeKernelMemory(imageFileName);
	return true;
}

bool filterFiles(PFLT_FILE_NAME_INFORMATION fileName)
{
	if (!RtlEqualUnicodeString(&fileName->Name, &g_filePath, true))
	{
		return false;
	}

	return true;
}

NTSTATUS FLTAPI fsUnload(__in FLT_FILTER_UNLOAD_FLAGS flags)
{
	UNREFERENCED_PARAMETER(flags);

	if (g_filterHandle)
	{
		FltUnregisterFilter(g_filterHandle);
		ZwClose(g_filterHandle);
	}

	RtlFreeUnicodeString(&g_procPath);
	RtlFreeUnicodeString(&g_filePath);
	RtlFreeUnicodeString(&g_traceLog);

	InterlockedExchange(&g_loggerContext.stopThread, 1);
	KeSetEvent(&g_loggerContext.event, 0, false);

	ZwWaitForSingleObject(g_loggerContext.thread, false, nullptr);
	ZwClose(g_loggerContext.thread);

	return STATUS_SUCCESS;
}

FLT_POSTOP_CALLBACK_STATUS postFileCallBack(_Inout_ PFLT_CALLBACK_DATA data,
	_In_ PCFLT_RELATED_OBJECTS fltObjects,
	_In_opt_ PVOID completionContext,
	_In_ FLT_POST_OPERATION_FLAGS flags)
{
	UNREFERENCED_PARAMETER(completionContext);
	UNREFERENCED_PARAMETER(flags);

	if (!data || data->IoStatus.Status != STATUS_SUCCESS)
	{
		return FLT_POSTOP_FINISHED_PROCESSING;
	}
	
	const auto cur_irql = KeGetCurrentIrql();

	if (cur_irql > APC_LEVEL)
	{
		return FLT_POSTOP_FINISHED_PROCESSING;
	}

	const auto thId = (uint64_t)PsGetThreadId(data->Thread);
	const auto pocId = (uint64_t)PsGetThreadProcessId(data->Thread);
	const auto fileObject = (uint64_t)(fltObjects ? fltObjects->FileObject : nullptr);
	
	FileContext* context = nullptr;
	auto status = FltGetStreamHandleContext(fltObjects->Instance, fltObjects->FileObject, (PFLT_CONTEXT*)&context);

	if (NT_SUCCESS(status))
	{
		_LOG_CONTEXT log = { 0 };
		log.majorFunction = data->Iopb->MajorFunction;

		log.path = (wchar_t*)memory::commitKernelMemory(context->Len + 2);
		memcpy(log.path, context->Path, context->Len);

		log.procId = pocId;
		log.thId = thId;
		log.fileObject = fileObject;
		log.irql = cur_irql;
		log.fileFlag = data->IoStatus.Information;
		log.sizeWrite = data->Iopb->Parameters.Write.Length;
		log.fileInfoType = data->Iopb->Parameters.SetFileInformation.FileInformationClass;

		pushLog(&log, sizeof(log));

		FltReleaseContext(context);
	}
	else
	{
		
		if (data->Iopb->MajorFunction == IRP_MJ_CREATE)
		{
			if (!filterProcTarget(data))
			{
				return FLT_POSTOP_FINISHED_PROCESSING;
			}

			PFLT_FILE_NAME_INFORMATION fileNameInfo = nullptr;
			status = FltGetFileNameInformation(data, FLT_FILE_NAME_OPENED | FLT_FILE_NAME_QUERY_ALWAYS_ALLOW_CACHE_LOOKUP, &fileNameInfo);

			if (NT_SUCCESS(status))
			{
				if (!filterFiles(fileNameInfo))
				{
					return FLT_POSTOP_FINISHED_PROCESSING;
				}

				context = nullptr;
				status = FltAllocateContext(fltObjects->Filter, FLT_STREAMHANDLE_CONTEXT, sizeof(FileContext), PagedPool, (PFLT_CONTEXT*)&context);

				if (NT_SUCCESS(status))
				{
					memset(context, 0, sizeof(FileContext));

					context->Path = (wchar_t*)memory::commitKernelMemory(fileNameInfo->Name.Length + 2);
					context->Len = fileNameInfo->Name.Length;
					memcpy(context->Path, fileNameInfo->Name.Buffer, fileNameInfo->Name.Length);

					_LOG_CONTEXT log = { 0 };

					log.majorFunction = data->Iopb->MajorFunction;
					log.procId = pocId;
					log.thId = thId;
					log.fileObject = fileObject;
					log.fileFlag = data->IoStatus.Information;

					log.path = (wchar_t*)memory::commitKernelMemory(context->Len + 2);
					memcpy(log.path, context->Path, context->Len);

					pushLog(&log, sizeof(log));

					FltSetStreamHandleContext(fltObjects->Instance, fltObjects->FileObject, FLT_SET_CONTEXT_KEEP_IF_EXISTS, (PFLT_CONTEXT*)(context), nullptr);
					FltReleaseContext(context);
				}

				FltReleaseFileNameInformation(fileNameInfo);
			}
		}
	}

	return FLT_POSTOP_FINISHED_PROCESSING;
}
