#include "DisplayWindow.h"
#include "Memory.h"

#include <QPainter>

DisplayWindow::DisplayWindow(Memory *memory, QWidget *parent)
    : QWidget(parent, Qt::Window)
    , m_memory(memory)
    , m_baseAddress(0x10040000u)
    , m_width(64)
    , m_height(32)
{
    setWindowTitle("Bitmap Display");

    m_framebuffer = QImage(m_width, m_height, QImage::Format_RGB32);
    m_framebuffer.fill(Qt::black);

    // Start at a readable zoom so individual pixels are visible immediately;
    // the window can still be freely resized afterward.
    const int zoom = 8;
    resize(m_width * zoom, m_height * zoom);
}

void DisplayWindow::configure(uint32_t baseAddress, int widthPixels, int heightPixels)
{
    m_baseAddress = baseAddress;
    m_width  = widthPixels  > 0 ? widthPixels  : 1;
    m_height = heightPixels > 0 ? heightPixels : 1;

    m_framebuffer = QImage(m_width, m_height, QImage::Format_RGB32);
    m_framebuffer.fill(Qt::black);

    // Re-apply the same fixed zoom used at construction, so the window's
    // aspect ratio matches the new pixel dimensions instead of stretching
    // whatever shape it happened to be before (e.g. going from 64x32 to a
    // square 128x128 previously stayed in the old wide window and looked
    // squashed). paintEvent() also now letterboxes defensively in case the
    // user manually resizes afterward.
    const int zoom = 8;
    resize(m_width * zoom, m_height * zoom);

    refresh();
}

void DisplayWindow::refresh()
{
    if (!m_memory) return;

    for (int row = 0; row < m_height; ++row) {
        for (int col = 0; col < m_width; ++col) {
            uint32_t addr = m_baseAddress + static_cast<uint32_t>((row * m_width + col) * 4);
            uint32_t *word = m_memory->readWord(addr);
            uint32_t pixel = word ? *word : 0; // unallocated page reads as black

            QRgb rgb = qRgb(static_cast<int>((pixel >> 16) & 0xFF),
                            static_cast<int>((pixel >>  8) & 0xFF),
                            static_cast<int>( pixel        & 0xFF));
            m_framebuffer.setPixel(col, row, rgb);
        }
    }

    update();
}

void DisplayWindow::paintEvent(QPaintEvent * /*event*/)
{
    QPainter painter(this);
    painter.fillRect(rect(), Qt::black); // letterbox bars where the image doesn't reach

    if (m_framebuffer.isNull() || m_width <= 0 || m_height <= 0) return;

    // Preserve the framebuffer's own aspect ratio instead of stretching it
    // to fill whatever shape the window currently is.
    double imageAspect  = static_cast<double>(m_width) / m_height;
    double widgetAspect = height() > 0 ? static_cast<double>(width()) / height() : imageAspect;

    QRect target;
    if (widgetAspect > imageAspect) {
        int drawWidth = static_cast<int>(height() * imageAspect);
        target = QRect((width() - drawWidth) / 2, 0, drawWidth, height());
    } else {
        int drawHeight = static_cast<int>(width() / imageAspect);
        target = QRect(0, (height() - drawHeight) / 2, width(), drawHeight);
    }

    painter.setRenderHint(QPainter::SmoothPixmapTransform, false); // crisp pixels, not blurry
    painter.drawImage(target, m_framebuffer);
}
