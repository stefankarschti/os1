#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "memory.h"
#include "console.h"
#include "sysinfo.h"

#include "stdlib.h"

struct system_info sysinfo;

void kernel_main(struct system_info* pinfo)
{
	sysinfo = *pinfo;
	
	/* Initialize terminal interface */
	terminal_initialize();
 	terminal_row = pinfo->cursory;
 	terminal_col = pinfo->cursorx;
	update_cursor(terminal_col, terminal_row);
 	
	/* Newline support is left as an exercise. */
	puts("[elf_kernel64] hello\n");

	char temp[16];
	itoa((int)(pinfo), temp, 16);
	puts("[elf_kernel64] system_info=0x");
	puts(temp);
	puts("\n");

	if(pinfo != 0x4000)
	{
		puts("[elf_kernel] unexpected system info pointer!\n");
		return;
	}

	uint64_t total_mem = 0;
	puts("Memory blocks (start, length, type):\n");
	for(int i = 0; i < pinfo->num_memory_blocks; ++i)
	{
		char start[16], len[16], type[16];
		itoa(pinfo->memory_blocks[i].start, start, 16);
		itoa(pinfo->memory_blocks[i].length, len, 16);
		itoa(pinfo->memory_blocks[i].type, type, 16);

		puts(start);
		puts(" ");
		puts(len);
		puts(" ");
		puts(type);
		puts("\n");
		
		if(1 == pinfo->memory_blocks[i].type)
		{
			total_mem += pinfo->memory_blocks[i].length;
		}
	}
	total_mem >>= 20;
	itoa((int)(total_mem), temp, 10);
	puts("[elf_kernel64] ");
	puts(temp);
	puts(" MB RAM detected\n");	
}

