// Reservation policy for bootloader-owned ranges that the kernel still needs
// after taking over its own page tables.
#include "mm/boot_reserve.h"

#include "mm/page_frame.h"

uint64_t BootFramebufferLengthBytes(const BootFramebufferInfo &framebuffer)
{
	return (uint64_t)framebuffer.pitch_bytes * (uint64_t)framebuffer.height;
}

void ReserveTrackedPhysicalRange(PageFrameContainer &frames, uint64_t physical_start, uint64_t length)
{
	if((0 == length) || (physical_start >= frames.MemoryEnd()))
	{
		return;
	}

	const uint64_t clamped_end = ((physical_start + length) < frames.MemoryEnd())
		? (physical_start + length)
		: frames.MemoryEnd();
	if(clamped_end > physical_start)
	{
		frames.ReserveRange(physical_start, clamped_end - physical_start);
	}
}