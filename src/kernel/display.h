#ifndef _DISPLAY_H_
#define _DISPLAY_H_

#include <stdint.h>

#include "bootinfo.h"

enum class TextDisplayBackendKind : uint8_t
{
	None = 0,
	VgaText = 1,
	FramebufferText = 2,
};

struct TextDisplayBackend
{
	TextDisplayBackendKind kind = TextDisplayBackendKind::None;
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

bool InitializeFramebufferTextDisplay(FramebufferTextDisplay &display, const BootFramebufferInfo &framebuffer);
void PresentTextDisplay(const TextDisplayBackend *backend,
		const uint16_t *buffer,
		uint16_t columns,
		uint16_t rows,
		uint16_t cursor_x,
		uint16_t cursor_y);
void DetachTextDisplay(const TextDisplayBackend *backend);

#endif
