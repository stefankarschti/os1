// Text presentation backends. Terminals own cell buffers; this layer renders
// those cells through VGA text mode or a Limine-provided framebuffer.
#ifndef OS1_KERNEL_DRIVERS_DISPLAY_TEXT_DISPLAY_H
#define OS1_KERNEL_DRIVERS_DISPLAY_TEXT_DISPLAY_H

#include <stdint.h>

#include "handoff/bootinfo.h"

enum class TextDisplayBackendKind : uint8_t
{
	None = 0,
	VgaText = 1,
	FramebufferText = 2,
};

struct TextDisplayBackend
{
	// Active rendering strategy.
	TextDisplayBackendKind kind = TextDisplayBackendKind::None;
	// Backend-specific object pointer.
	void *instance = nullptr;
};

struct VgaTextDisplay
{
};

struct FramebufferTextDisplay
{
	bool available = false;
	uint8_t *framebuffer = nullptr;
	uint32_t width = 0;
	uint32_t height = 0;
	uint32_t pitch_bytes = 0;
	uint16_t bits_per_pixel = 0;
	BootFramebufferPixelFormat pixel_format = BootFramebufferPixelFormat::Unknown;
};

// Choose the terminal presentation backend from the normalized boot display
// metadata. BIOS keeps VGA text; Limine prefers a compatible framebuffer.
TextDisplayBackend *SelectTextDisplay(const BootInfo &boot_info);

// Validate and bind a boot framebuffer for text rendering.
bool InitializeFramebufferTextDisplay(FramebufferTextDisplay &display, const BootFramebufferInfo &framebuffer);
// Present one full text grid plus cursor state through the selected backend.
void PresentTextDisplay(const TextDisplayBackend *backend,
		const uint16_t *buffer,
		uint16_t columns,
		uint16_t rows,
		uint16_t cursor_x,
		uint16_t cursor_y);
// Stop presenting a terminal through this backend.
void DetachTextDisplay(const TextDisplayBackend *backend);

#endif // OS1_KERNEL_DRIVERS_DISPLAY_TEXT_DISPLAY_H
