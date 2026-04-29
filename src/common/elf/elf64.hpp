// Shared ELF64 layout and lightweight validation helpers used by both the
// Limine frontend and kernel-side ELF consumers.
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace elf
{
constexpr uint32_t kMagic = 0x464C457Fu;
constexpr uint16_t kTypeExec = 2;
constexpr uint16_t kMachineX86_64 = 62;
constexpr uint32_t kProgramTypeLoad = 1;
constexpr uint32_t kProgramFlagExecute = 0x1;
constexpr uint32_t kProgramFlagWrite = 0x2;

struct Elf64Header
{
    uint32_t magic;
    uint8_t ident[12];
    uint16_t type;
    uint16_t machine;
    uint32_t version;
    uint64_t entry;
    uint64_t phoff;
    uint64_t shoff;
    uint32_t flags;
    uint16_t ehsize;
    uint16_t phentsize;
    uint16_t phnum;
    uint16_t shentsize;
    uint16_t shnum;
    uint16_t shstrndx;
} __attribute__((packed));

struct Elf64ProgramHeader
{
    uint32_t type;
    uint32_t flags;
    uint64_t offset;
    uint64_t vaddr;
    uint64_t paddr;
    uint64_t filesz;
    uint64_t memsz;
    uint64_t align;
} __attribute__((packed));

[[nodiscard]] inline bool checked_add(uint64_t left, uint64_t right, uint64_t& result)
{
    if(left > (~0ull - right))
    {
        return false;
    }
    result = left + right;
    return true;
}

[[nodiscard]] inline const Elf64Header* header_from_image(const void* image, uint64_t image_size)
{
    if((nullptr == image) || (image_size < sizeof(Elf64Header)))
    {
        return nullptr;
    }

    const auto* header = reinterpret_cast<const Elf64Header*>(image);
    if((header->magic != kMagic) || (header->phoff >= image_size) ||
       (header->phentsize != sizeof(Elf64ProgramHeader)))
    {
        return nullptr;
    }

    return header;
}

[[nodiscard]] inline const Elf64ProgramHeader* program_header_from_image(
    const Elf64Header& header, const void* image, uint64_t image_size, uint16_t index)
{
    if(nullptr == image)
    {
        return nullptr;
    }

    const uint64_t index_offset = static_cast<uint64_t>(index) * header.phentsize;
    if((0 != header.phentsize) && (index_offset / header.phentsize != index))
    {
        return nullptr;
    }

    uint64_t program_offset = 0;
    if(!checked_add(header.phoff, index_offset, program_offset))
    {
        return nullptr;
    }

    uint64_t program_end = 0;
    if(!checked_add(program_offset, sizeof(Elf64ProgramHeader), program_end) ||
       (program_end > image_size))
    {
        return nullptr;
    }

    return reinterpret_cast<const Elf64ProgramHeader*>(
        static_cast<const uint8_t*>(image) + program_offset);
}

[[nodiscard]] inline bool loadable_segment_bounds_valid(const Elf64ProgramHeader& program,
                                                        uint64_t image_size)
{
    uint64_t file_end = 0;
    return (program.memsz >= program.filesz) && checked_add(program.offset, program.filesz, file_end) &&
           (file_end <= image_size);
}
}  // namespace elf
