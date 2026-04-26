#ifndef OS1_KERNEL_UTIL_FIXED_STRING_H
#define OS1_KERNEL_UTIL_FIXED_STRING_H

#include <stddef.h>

inline void CopyFixedString(char *destination, size_t destination_size, const char *source)
{
	if((nullptr == destination) || (0 == destination_size))
	{
		return;
	}

	size_t index = 0;
	if(nullptr != source)
	{
		while(((index + 1) < destination_size) && source[index])
		{
			destination[index] = source[index];
			++index;
		}
	}
	while(index < destination_size)
	{
		destination[index++] = 0;
	}
}

#endif
