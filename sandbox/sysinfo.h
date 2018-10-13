#ifndef _SYSINFO_H_
#define _SYSINFO_H_

#pragma pack(1)
struct memory_block
{
	uint64_t start;
	uint64_t length;
	uint32_t type;
	uint32_t unused;
};
struct system_info
{
	uint8_t cursorx;
	uint8_t cursory;
	uint16_t num_memory_blocks;
	struct memory_block* memory_blocks;
};
#pragma pack()


#endif

