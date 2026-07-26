#pragma once

#include <QWidget>
#include <QImage>
#include <cstdint>

class Memory;

// Standalone top-level window rendering a memory-mapped bitmap display.
// One pixel = one 32-bit word (0x00RRGGBB) at:
//     address = base + (row * width + col) * 4
// The device itself is "dumb" — it just polls Memory and paints; all drawing
// (fonts, shapes, etc.) is done by the running MIPS program.
class DisplayWindow : public QWidget
{
    Q_OBJECT

public:
    explicit DisplayWindow(Memory *memory, QWidget *parent = nullptr);

    // Re-reads the configured framebuffer region from memory and repaints.
    void refresh();

    // Changes the base address / pixel dimensions, resizing the framebuffer.
    void configure(uint32_t baseAddress, int widthPixels, int heightPixels);

    uint32_t baseAddress() const { return m_baseAddress; }
    int      widthPixels()  const { return m_width; }
    int      heightPixels() const { return m_height; }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    Memory  *m_memory = nullptr;
    uint32_t m_baseAddress;
    int      m_width;
    int      m_height;
    QImage   m_framebuffer;
};
