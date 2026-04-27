// Reservation policy for bootloader-owned ranges that the kernel still needs
// after taking over its own page tables.
#include "mm/boot_reserve.hpp"

#include "mm/page_frame.hpp"

uint64_t BootFramebufferLengthBytes(const BootFramebufferInfo &framebuffer)
{
	return (uint64_t)framebuffer.pitch_bytes * (uint64_t)framebuffer.height;
}

void reserve_tracked_physical_range(PageFrameContainer &frames, uint64_t physical_start, uint64_t length)
{
	if((0 == length) || (physical_start >= frames.memory_end()))
	{
		return;
	}

	const uint64_t clamped_end = ((physical_start + length) < frames.memory_end())
		? (physical_start + length)
		: frames.memory_end();
	if(clamped_end > physical_start)
	{
		frames.reserve_range(physical_start, clamped_end - physical_start);
	}
}