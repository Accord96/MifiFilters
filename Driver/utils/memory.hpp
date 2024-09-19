#pragma once
#define Tag 'looP'

namespace memory
{
	void* commitKernelMemory(SIZE_T size)
	{
		if (!size)
		{
			return nullptr;
		}

		auto mem = ExAllocatePool2(POOL_FLAG_NON_PAGED, size, Tag);

		if (mem)
		{
			memset(mem, 0, size);
		}

		return mem;

	}

	void freeKernelMemory(void* addr)
	{
		if (addr)
		{
			ExFreePoolWithTag(addr, Tag);
		}
	}
}