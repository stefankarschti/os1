// Kernel-owned copy of the boot handoff graph. Boot frontend memory can be
// reclaimed after this module has copied pointed-to arrays and strings.
#include "handoff/boot_info.hpp"

#include "util/memory.h"

namespace
{
struct OwnedBootInfoStorage
{
	BootInfo info{};
	BootMemoryRegion memory_map[kBootInfoMaxMemoryRegions]{};
	BootModuleInfo modules[kBootInfoMaxModules]{};
	char bootloader_name[kBootInfoMaxBootloaderNameBytes]{};
	char command_line[kBootInfoMaxCommandLineBytes]{};
	char module_names[kBootInfoMaxModules][kBootInfoMaxModuleNameBytes]{};
};

constinit OwnedBootInfoStorage g_owned_boot_info{};

size_t copy_string(char *dest, size_t capacity, const char *source)
{
	if((nullptr == dest) || (0 == capacity))
	{
		return 0;
	}
	dest[0] = 0;
	if(nullptr == source)
	{
		return 0;
	}

	size_t source_length = 0;
	while(source[source_length])
	{
		++source_length;
	}
	const size_t copy_length = (source_length < (capacity - 1))
			? source_length
			: (capacity - 1);
	memcpy(dest, source, copy_length);
	dest[copy_length] = 0;
	return copy_length;
}
}

const BootInfo *own_boot_info(const BootInfo *source)
{
	if(nullptr == source)
	{
		return nullptr;
	}
	if((source->magic != kBootInfoMagic) || (source->version != kBootInfoVersion))
	{
		return nullptr;
	}
	if((source->memory_map_count > 0) && (nullptr == source->memory_map))
	{
		return nullptr;
	}
	if(source->memory_map_count > kBootInfoMaxMemoryRegions)
	{
		return nullptr;
	}
	if((source->module_count > 0) && (nullptr == source->modules))
	{
		return nullptr;
	}
	if(source->module_count > kBootInfoMaxModules)
	{
		return nullptr;
	}

	memset(&g_owned_boot_info, 0, sizeof(g_owned_boot_info));
	g_owned_boot_info.info = *source;

	// Bootloader-owned pointers are copied into kernel BSS immediately so later
	// milestones can reclaim or replace the original boot staging memory safely.
	if(source->memory_map_count > 0)
	{
		memcpy(g_owned_boot_info.memory_map, source->memory_map,
				source->memory_map_count * sizeof(BootMemoryRegion));
		g_owned_boot_info.info.memory_map = g_owned_boot_info.memory_map;
	}
	else
	{
		g_owned_boot_info.info.memory_map = nullptr;
	}

	if(source->module_count > 0)
	{
		memcpy(g_owned_boot_info.modules, source->modules,
				source->module_count * sizeof(BootModuleInfo));
		for(size_t i = 0; i < source->module_count; ++i)
		{
			if(source->modules[i].name)
			{
				copy_string(g_owned_boot_info.module_names[i],
						kBootInfoMaxModuleNameBytes,
						source->modules[i].name);
				g_owned_boot_info.modules[i].name = g_owned_boot_info.module_names[i];
			}
			else
			{
				g_owned_boot_info.modules[i].name = nullptr;
			}
		}
		g_owned_boot_info.info.modules = g_owned_boot_info.modules;
	}
	else
	{
		g_owned_boot_info.info.modules = nullptr;
	}

	if(source->bootloader_name)
	{
		copy_string(g_owned_boot_info.bootloader_name,
				kBootInfoMaxBootloaderNameBytes,
				source->bootloader_name);
		g_owned_boot_info.info.bootloader_name = g_owned_boot_info.bootloader_name;
	}
	else
	{
		g_owned_boot_info.info.bootloader_name = nullptr;
	}

	if(source->command_line)
	{
		copy_string(g_owned_boot_info.command_line,
				kBootInfoMaxCommandLineBytes,
				source->command_line);
		g_owned_boot_info.info.command_line = g_owned_boot_info.command_line;
	}
	else
	{
		g_owned_boot_info.info.command_line = nullptr;
	}

	return &g_owned_boot_info.info;
}
