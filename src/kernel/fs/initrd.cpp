// CPIO newc initrd parser. It exposes a tiny archive traversal API until the
// future VFS can provide path lookup and filesystem-backed exec.
#include "fs/initrd.hpp"

namespace
{
// The boot path still publishes the initrd as a boot module rather than a
// filesystem-backed device, so the parser holds only a borrowed BootInfo view.
const BootInfo *g_boot_info = nullptr;

constexpr uint32_t kCpioModeTypeMask = 0170000u;
constexpr uint32_t kCpioModeRegular = 0100000u;

struct CpioNewcHeader
{
	char magic[6];
	char inode[8];
	char mode[8];
	char uid[8];
	char gid[8];
	char nlink[8];
	char mtime[8];
	char filesize[8];
	char devmajor[8];
	char devminor[8];
	char rdevmajor[8];
	char rdevminor[8];
	char namesize[8];
	char check[8];
} __attribute__((packed));

struct InitrdLookupContext
{
	const char *path;
	const uint8_t *data;
	uint64_t size;
};

[[nodiscard]] uint64_t AlignUp(uint64_t value, uint64_t alignment)
{
	return (value + alignment - 1) & ~(alignment - 1);
}


const char *NormalizeArchivePath(const char *path)
{
	if(nullptr == path)
	{
		return nullptr;
	}
	while('.' == path[0] && '/' == path[1])
	{
		path += 2;
	}
	while('/' == *path)
	{
		++path;
	}
	return path;
}

bool PathsEqual(const char *archive_name, const char *wanted)
{
	const char *normalized_archive = NormalizeArchivePath(archive_name);
	const char *normalized_wanted = NormalizeArchivePath(wanted);
	if((nullptr == normalized_archive) || (nullptr == normalized_wanted))
	{
		return false;
	}
	for(size_t index = 0;; ++index)
	{
		if(normalized_archive[index] != normalized_wanted[index])
		{
			return false;
		}
		if(0 == normalized_archive[index])
		{
			return true;
		}
	}
}

[[nodiscard]] bool IsRegularInitrdEntry(uint32_t mode)
{
	return (mode & kCpioModeTypeMask) == kCpioModeRegular;
}

uint32_t ParseHex(const char *text, size_t digits)
{
	uint32_t value = 0;
	for(size_t i = 0; i < digits; ++i)
	{
		value <<= 4;
		if((text[i] >= '0') && (text[i] <= '9'))
		{
			value |= static_cast<uint32_t>(text[i] - '0');
		}
		else if((text[i] >= 'a') && (text[i] <= 'f'))
		{
			value |= static_cast<uint32_t>(text[i] - 'a' + 10);
		}
		else if((text[i] >= 'A') && (text[i] <= 'F'))
		{
			value |= static_cast<uint32_t>(text[i] - 'A' + 10);
		}
	}
	return value;
}

bool MatchInitrdFile(const char *archive_name, const uint8_t *file_data, uint64_t file_size, void *context)
{
	auto *lookup = static_cast<InitrdLookupContext *>(context);
	if((nullptr == lookup) || !PathsEqual(archive_name, lookup->path))
	{
		return true;
	}

	lookup->data = file_data;
	lookup->size = file_size;
	return false;
}
}

void bind_initrd_boot_info(const BootInfo *boot_info)
{
	g_boot_info = boot_info;
}

bool for_each_initrd_file(InitrdFileVisitor visitor, void *context)
{
	if((nullptr == visitor) || (nullptr == g_boot_info) || (0 == g_boot_info->module_count))
	{
		return false;
	}

	const BootModuleInfo &module = g_boot_info->modules[0];
	const uint8_t *cursor = reinterpret_cast<const uint8_t *>(module.physical_start);
	const uint8_t *end = cursor + module.length;
	while((cursor + sizeof(CpioNewcHeader)) <= end)
	{
		const CpioNewcHeader *header = reinterpret_cast<const CpioNewcHeader *>(cursor);
		bool magic_ok = true;
		for(size_t i = 0; i < 6; ++i)
		{
			if(header->magic[i] != "070701"[i])
			{
				magic_ok = false;
				break;
			}
		}
		if(!magic_ok)
		{
			return false;
		}

		const uint32_t mode = ParseHex(header->mode, 8);
		const uint32_t name_size = ParseHex(header->namesize, 8);
		const uint32_t file_size = ParseHex(header->filesize, 8);
		const char *name = reinterpret_cast<const char *>(cursor + sizeof(CpioNewcHeader));
		const uint8_t *file_data = reinterpret_cast<const uint8_t *>(AlignUp(
			reinterpret_cast<uint64_t>(cursor + sizeof(CpioNewcHeader) + name_size), 4));
		if((reinterpret_cast<const uint8_t *>(name) > end) || ((file_data + file_size) > end))
		{
			return false;
		}

		if(PathsEqual(name, "TRAILER!!!"))
		{
			return true;
		}

		if(IsRegularInitrdEntry(mode) && !visitor(name, file_data, file_size, context))
		{
			return false;
		}

		cursor = reinterpret_cast<const uint8_t *>(AlignUp(reinterpret_cast<uint64_t>(file_data + file_size), 4));
	}

	return false;
}

bool find_initrd_file(const char *path, const uint8_t *&data, uint64_t &size)
{
	InitrdLookupContext lookup{path, nullptr, 0};
	(void)for_each_initrd_file(MatchInitrdFile, &lookup);
	data = lookup.data;
	size = lookup.size;
	return (nullptr != data);
}

void copy_initrd_path(char *destination, size_t destination_size, const char *archive_name)
{
	if((nullptr == destination) || (0 == destination_size))
	{
		return;
	}

	const char *normalized = NormalizeArchivePath(archive_name);
	size_t index = 0;
	if((nullptr != normalized) && (0 != normalized[0]))
	{
		if(index < (destination_size - 1))
		{
			destination[index++] = '/';
		}
		size_t source_index = 0;
		while(((index + 1) < destination_size) && normalized[source_index])
		{
			destination[index++] = normalized[source_index++];
		}
	}
	while(index < destination_size)
	{
		destination[index++] = 0;
	}
}