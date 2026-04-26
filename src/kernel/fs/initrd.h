#ifndef OS1_KERNEL_FS_INITRD_H
#define OS1_KERNEL_FS_INITRD_H

#include <stddef.h>
#include <stdint.h>

#include "bootinfo.h"

// The initrd module is still a boot-time cpio archive owned by BootInfo.
// Keep the parsing surface small so later filesystem work can replace it
// without dragging CPIO details through unrelated kernel subsystems.

using InitrdFileVisitor = bool (*)(const char *archive_name, const uint8_t *file_data, uint64_t file_size, void *context);

void BindInitrdBootInfo(const BootInfo *boot_info);
bool ForEachInitrdFile(InitrdFileVisitor visitor, void *context);
bool FindInitrdFile(const char *path, const uint8_t *&data, uint64_t &size);
void CopyInitrdPath(char *destination, size_t destination_size, const char *archive_name);

#endif
