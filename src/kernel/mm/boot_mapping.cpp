// Identity-mapping policy for boot-critical physical ranges.
#include "mm/boot_mapping.h"

#include "handoff/memory_layout.h"
#include "mm/virtual_memory.h"
#include "util/align.h"

bool MapIdentityRange(VirtualMemory &vm, uint64_t physical_start, uint64_t length)
{
	if((0 == physical_start) || (0 == length))
	{
		return true;
	}

	const uint64_t start = AlignDown(physical_start, kPageSize);
	const uint64_t end = AlignUp(physical_start + length, kPageSize);
	return vm.MapPhysical(start,
			start,
			(end - start) / kPageSize,
			PageFlags::Present | PageFlags::Write);
}