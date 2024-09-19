#include "include.hpp"

NTSTATUS DriverEntry(
	PDRIVER_OBJECT  driverObject,
	PUNICODE_STRING registryPath
)
{
	auto status = setupSettings(registryPath);

	if (!NT_SUCCESS(status))
	{
		TRACE("setupSettings error - %08x\n", status);
		return status;
	}

	InitializeListHead(&g_loggerContext.listHead);
	KeInitializeSpinLock(&g_loggerContext.lock);
	KeInitializeEvent(&g_loggerContext.event, SynchronizationEvent, FALSE);

	const FLT_OPERATION_REGISTRATION callbackList[] = 
	{
		{ IRP_MJ_CREATE, 0, 0, postFileCallBack, 0 },
		{ IRP_MJ_WRITE, 0, 0, postFileCallBack, 0  },
		{ IRP_MJ_SET_INFORMATION, 0, 0, postFileCallBack, 0  },
		{ IRP_MJ_CLOSE, NULL, 0, postFileCallBack, 0  },
		{ IRP_MJ_CLEANUP, NULL, 0, postFileCallBack, 0  },
		{ IRP_MJ_OPERATION_END }
	};

	const FLT_CONTEXT_REGISTRATION contextRegistration[] = 
	{
		{ FLT_STREAMHANDLE_CONTEXT, 0, cleanup, sizeof(FileContext), Tag},
		{ FLT_CONTEXT_END }
	};

	const FLT_REGISTRATION filterRegistration =
	{
		sizeof(FLT_REGISTRATION),
		FLT_REGISTRATION_VERSION,
		0,
		contextRegistration,
		callbackList,
		fsUnload,
		0,
		0,
		0,
		0,
		0,
		0,
		0
	};

	status = FltRegisterFilter(driverObject, &filterRegistration, &g_filterHandle);

	if (!NT_SUCCESS(status))
	{
		TRACE("FltRegisterFilter error - %08x\n", status);
		return status;
	}

	OBJECT_ATTRIBUTES objectAttributes;
	InitializeObjectAttributes(&objectAttributes, NULL, OBJ_KERNEL_HANDLE, nullptr, nullptr);

	status = PsCreateSystemThread((PHANDLE)&g_loggerContext.thread, (ULONG)GENERIC_ALL, (POBJECT_ATTRIBUTES)&objectAttributes, (HANDLE)nullptr, (PCLIENT_ID)nullptr, (PKSTART_ROUTINE)loggerManager, (PVOID)nullptr);
	
	if (!NT_SUCCESS(status))
	{
		TRACE("PsCreateSystemThread error - %08x\n", status);
		return status;
	}

	status = FltStartFiltering(g_filterHandle);

	if (!NT_SUCCESS(status))
	{
		TRACE("FltStartFiltering error - %08x\n", status);
		return status;
	}

	return status;
}