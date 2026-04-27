// Text rendering backends for logical terminals. VGA text mode and framebuffer
// text rendering share the same 80x25 cell-buffer input contract.
#include "drivers/display/text_display.hpp"

#include "arch/x86_64/cpu/io_port.hpp"
#include "debug/debug.hpp"
#include "font8x8_basic.h"
#include "util/ctype.hpp"
#include "util/memory.h"

namespace
{
constexpr uint32_t kFramebufferCellWidth = 8;
constexpr uint32_t kFramebufferCellHeight = 16;
constexpr uint32_t kFramebufferBackground = 0x00000000u;
constexpr uint32_t kFramebufferForeground = 0x00FFFFFFu;
constexpr uint16_t kVgaBlankCell = 0x0720;

VgaTextDisplay g_vga_text_display;
FramebufferTextDisplay g_framebuffer_text_display;
TextDisplayBackend g_vga_backend{TextDisplayBackendKind::VgaText, &g_vga_text_display};
TextDisplayBackend g_framebuffer_backend{TextDisplayBackendKind::FramebufferText,
                                         &g_framebuffer_text_display};

[[nodiscard]] inline uint64_t BufferSizeBytes(uint16_t columns, uint16_t rows)
{
    return (uint64_t)columns * (uint64_t)rows * sizeof(uint16_t);
}

void PresentVgaText(
    const uint16_t* buffer, uint16_t columns, uint16_t rows, uint16_t cursor_x, uint16_t cursor_y)
{
    if(nullptr == buffer)
    {
        return;
    }

    uint16_t* screen = (uint16_t*)0xB8000;
    memcpy(screen, buffer, BufferSizeBytes(columns, rows));

    const uint16_t position = (uint16_t)(cursor_y * columns + cursor_x);
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(position & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((position >> 8) & 0xFF));
}

void DetachVgaText()
{
    uint16_t* screen = (uint16_t*)0xB8000;
    memsetw(screen, kVgaBlankCell, 80 * 25 * sizeof(uint16_t));
    outb(0x3D4, 0x0F);
    outb(0x3D5, 0);
    outb(0x3D4, 0x0E);
    outb(0x3D5, 0);
}

void FramebufferClear(FramebufferTextDisplay& display, uint32_t rgb)
{
    if(!display.available)
    {
        return;
    }

    if(0 == rgb)
    {
        memset(display.framebuffer, 0, (uint64_t)display.pitch_bytes * (uint64_t)display.height);
        return;
    }

    for(uint32_t y = 0; y < display.height; ++y)
    {
        for(uint32_t x = 0; x < display.width; ++x)
        {
            uint8_t* pixel = display.framebuffer + (uint64_t)y * display.pitch_bytes +
                             (uint64_t)x * ((uint64_t)display.bits_per_pixel / 8ull);
            if(24 == display.bits_per_pixel)
            {
                pixel[0] = (uint8_t)(rgb & 0xFFu);
                pixel[1] = (uint8_t)((rgb >> 8) & 0xFFu);
                pixel[2] = (uint8_t)((rgb >> 16) & 0xFFu);
            }
            else
            {
                *((uint32_t*)pixel) = rgb;
            }
        }
    }
}

void FramebufferSetPixel(FramebufferTextDisplay& display, uint32_t x, uint32_t y, uint32_t rgb)
{
    if(!display.available || (x >= display.width) || (y >= display.height))
    {
        return;
    }

    uint8_t* pixel = display.framebuffer + (uint64_t)y * display.pitch_bytes +
                     (uint64_t)x * ((uint64_t)display.bits_per_pixel / 8ull);
    if(24 == display.bits_per_pixel)
    {
        pixel[0] = (uint8_t)(rgb & 0xFFu);
        pixel[1] = (uint8_t)((rgb >> 8) & 0xFFu);
        pixel[2] = (uint8_t)((rgb >> 16) & 0xFFu);
        return;
    }

    *((uint32_t*)pixel) = rgb;
}

void FramebufferDrawCell(
    FramebufferTextDisplay& display, uint32_t cell_x, uint32_t cell_y, char character, bool inverse)
{
    const uint8_t glyph_index = (uint8_t)character;
    const uint32_t fg = inverse ? kFramebufferBackground : kFramebufferForeground;
    const uint32_t bg = inverse ? kFramebufferForeground : kFramebufferBackground;

    for(uint32_t glyph_row = 0; glyph_row < 8; ++glyph_row)
    {
        const uint8_t bits = font8x8_basic[glyph_index][glyph_row];
        for(uint32_t repeat = 0; repeat < 2; ++repeat)
        {
            const uint32_t y = cell_y + glyph_row * 2 + repeat;
            for(uint32_t glyph_col = 0; glyph_col < 8; ++glyph_col)
            {
                const bool set = ((bits >> glyph_col) & 1u) != 0;
                FramebufferSetPixel(display, cell_x + glyph_col, y, set ? fg : bg);
            }
        }
    }
}

void PresentFramebufferText(FramebufferTextDisplay& display,
                            const uint16_t* buffer,
                            uint16_t columns,
                            uint16_t rows,
                            uint16_t cursor_x,
                            uint16_t cursor_y)
{
    if(!display.available || (nullptr == buffer))
    {
        return;
    }

    // The first framebuffer backend intentionally repaints the whole terminal
    // every time. That keeps the M3 presentation path easy to reason about while
    // serial remains the authoritative debug channel.
    FramebufferClear(display, kFramebufferBackground);

    const uint32_t text_width = (uint32_t)columns * kFramebufferCellWidth;
    const uint32_t text_height = (uint32_t)rows * kFramebufferCellHeight;
    const uint32_t origin_x = (display.width > text_width) ? ((display.width - text_width) / 2) : 0;
    const uint32_t origin_y =
        (display.height > text_height) ? ((display.height - text_height) / 2) : 0;

    for(uint32_t row = 0; row < rows; ++row)
    {
        for(uint32_t col = 0; col < columns; ++col)
        {
            char character = (char)(buffer[row * columns + col] & 0xFFu);
            if(!isprint(character))
            {
                character = ' ';
            }
            const bool inverse = (cursor_x == col) && (cursor_y == row);
            FramebufferDrawCell(display,
                                origin_x + col * kFramebufferCellWidth,
                                origin_y + row * kFramebufferCellHeight,
                                character,
                                inverse);
        }
    }
}
}  // namespace

TextDisplayBackend* SelectTextDisplay(const BootInfo& boot_info)
{
    if(BootSource::BiosLegacy == boot_info.source)
    {
        debug("console backend: vga")();
        return &g_vga_backend;
    }

    if(initialize_framebuffer_text_display(g_framebuffer_text_display, boot_info.framebuffer))
    {
        debug("framebuffer console active")();
        debug("framebuffer ")(boot_info.framebuffer.width)("x")(boot_info.framebuffer.height)(
            " pitch ")(boot_info.framebuffer.pitch_bytes)(" bpp ")(
            boot_info.framebuffer.bits_per_pixel)(" format ")(
            boot_framebuffer_pixel_format_name(boot_info.framebuffer.pixel_format))();
        return &g_framebuffer_backend;
    }

    if(0 != boot_info.framebuffer.physical_address)
    {
        debug("framebuffer console unavailable")();
    }
    else
    {
        debug("console backend: serial-only")();
    }

    return nullptr;
}

bool initialize_framebuffer_text_display(FramebufferTextDisplay& display,
                                         const BootFramebufferInfo& framebuffer)
{
    display.available = false;
    display.framebuffer = nullptr;
    display.width = framebuffer.width;
    display.height = framebuffer.height;
    display.pitch_bytes = framebuffer.pitch_bytes;
    display.bits_per_pixel = framebuffer.bits_per_pixel;
    display.pixel_format = framebuffer.pixel_format;

    if((0 == framebuffer.physical_address) || (0 == display.width) || (0 == display.height) ||
       (0 == display.pitch_bytes))
    {
        return false;
    }

    if((BootFramebufferPixelFormat::Rgb != display.pixel_format) &&
       (BootFramebufferPixelFormat::Bgr != display.pixel_format))
    {
        return false;
    }

    if((24 != display.bits_per_pixel) && (32 != display.bits_per_pixel))
    {
        return false;
    }

    display.framebuffer = (uint8_t*)framebuffer.physical_address;
    display.available = true;
    FramebufferClear(display, kFramebufferBackground);
    return true;
}

void present_text_display(const TextDisplayBackend* backend,
                          const uint16_t* buffer,
                          uint16_t columns,
                          uint16_t rows,
                          uint16_t cursor_x,
                          uint16_t cursor_y)
{
    if(nullptr == backend)
    {
        return;
    }

    switch(backend->kind)
    {
        case TextDisplayBackendKind::VgaText:
            PresentVgaText(buffer, columns, rows, cursor_x, cursor_y);
            break;
        case TextDisplayBackendKind::FramebufferText:
            PresentFramebufferText(*(FramebufferTextDisplay*)backend->instance,
                                   buffer,
                                   columns,
                                   rows,
                                   cursor_x,
                                   cursor_y);
            break;
        default:
            break;
    }
}

void detach_text_display(const TextDisplayBackend* backend)
{
    if(nullptr == backend)
    {
        return;
    }

    switch(backend->kind)
    {
        case TextDisplayBackendKind::VgaText:
            DetachVgaText();
            break;
        case TextDisplayBackendKind::FramebufferText:
            FramebufferClear(*(FramebufferTextDisplay*)backend->instance, kFramebufferBackground);
            break;
        default:
            break;
    }
}
