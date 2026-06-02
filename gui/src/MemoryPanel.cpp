#include "MemoryPanel.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QFont>
#include <cstdio>

MemoryPanel::MemoryPanel(QWidget *parent)
    : QDockWidget("Memory", parent)
{
    setAllowedAreas(Qt::AllDockWidgetAreas);

    // ── Controls bar ──────────────────────────────────────────────────────────
    m_prevButton = new QPushButton("◀ Prev", this);
    m_nextButton = new QPushButton("Next ▶", this);
    m_gotoField  = new QLineEdit(this);
    m_gotoField->setPlaceholderText("Address (hex)");
    m_gotoField->setFixedWidth(130);
    m_gotoButton  = new QPushButton("Go", this);
    m_historyMenu = new QMenu(this);
    m_historyButton = new QPushButton("▾", this);
    m_historyButton->setFixedWidth(24);
    m_historyButton->setToolTip("Go-to history");
    m_pageLabel  = new QLabel(this);

    auto *controls = new QHBoxLayout;
    controls->addWidget(m_prevButton);
    controls->addWidget(m_nextButton);
    controls->addSpacing(12);
    controls->addWidget(new QLabel("Go to:", this));
    controls->addWidget(m_gotoField);
    controls->addWidget(m_gotoButton);
    controls->addWidget(m_historyButton);
    controls->addStretch();
    controls->addWidget(m_pageLabel);

    // ── Hex table ─────────────────────────────────────────────────────────────
    // Columns: Address | 16 hex bytes | ASCII
    m_table = new QTableWidget(this);
    m_table->setFont(QFont("Courier New", 10));
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setShowGrid(true);
    m_table->horizontalHeader()->setDefaultSectionSize(28);
    m_table->verticalHeader()->setVisible(false);

    buildTable();

    // ── Layout ────────────────────────────────────────────────────────────────
    auto *root = new QWidget(this);
    auto *vbox = new QVBoxLayout(root);
    vbox->setContentsMargins(4, 4, 4, 4);
    vbox->addLayout(controls);
    vbox->addWidget(m_table);
    setWidget(root);

    // ── Connections ───────────────────────────────────────────────────────────
    connect(m_gotoButton,   &QPushButton::clicked,       this, &MemoryPanel::onGoToClicked);
    connect(m_gotoField,    &QLineEdit::returnPressed,   this, &MemoryPanel::onGoToClicked);
    connect(m_nextButton,   &QPushButton::clicked,       this, &MemoryPanel::onNextPageClicked);
    connect(m_prevButton,   &QPushButton::clicked,       this, &MemoryPanel::onPrevPageClicked);
    connect(m_table,        &QTableWidget::cellChanged,  this, &MemoryPanel::onCellChanged);
    connect(m_historyButton,&QPushButton::clicked, this, [this]() {
        if (m_historyMenu->isEmpty()) return;
        m_historyMenu->popup(m_historyButton->mapToGlobal(
            m_historyButton->rect().bottomLeft()));
    });
    connect(m_historyMenu, &QMenu::triggered, this, &MemoryPanel::onHistoryTriggered);

    refreshView();
}

void MemoryPanel::buildTable()
{
    const int rows = MEMORY_PAGE_SIZE / MEMORY_BYTES_PER_ROW;

    // 1 address column + 16 byte columns + 1 ASCII column
    m_table->setColumnCount(1 + MEMORY_BYTES_PER_ROW + 1);
    m_table->setRowCount(rows);

    QStringList headers;
    headers << "Address";
    for (int i = 0; i < MEMORY_BYTES_PER_ROW; ++i)
        headers << QString("%1").arg(i, 2, 16, QChar('0')).toUpper();
    headers << "ASCII";
    m_table->setHorizontalHeaderLabels(headers);

    // Address column wider, ASCII column wider
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(
        1 + MEMORY_BYTES_PER_ROW, QHeaderView::ResizeToContents);
}

void MemoryPanel::refreshView()
{
    m_table->blockSignals(true);

    const int rows = MEMORY_PAGE_SIZE / MEMORY_BYTES_PER_ROW;
    char buf[16];

    for (int row = 0; row < rows; ++row) {
        uint32_t rowAddr = m_baseAddress + static_cast<uint32_t>(row * MEMORY_BYTES_PER_ROW);

        // Address cell (read-only)
        std::snprintf(buf, sizeof(buf), "0x%08X", rowAddr);
        auto *addrItem = new QTableWidgetItem(QString(buf));
        addrItem->setFlags(addrItem->flags() & ~Qt::ItemIsEditable);
        m_table->setItem(row, 0, addrItem);

        QString ascii;
        for (int col = 0; col < MEMORY_BYTES_PER_ROW; ++col) {
            size_t idx = rowAddr + static_cast<size_t>(col);
            uint8_t byte = (idx < m_size) ? m_mem[idx] : 0;

            std::snprintf(buf, sizeof(buf), "%02X", byte);
            auto *byteItem = new QTableWidgetItem(QString(buf));
            byteItem->setTextAlignment(Qt::AlignCenter);
            m_table->setItem(row, 1 + col, byteItem);

            ascii += (byte >= 0x20 && byte < 0x7F) ? QChar(byte) : QChar('.');
        }

        // ASCII cell (read-only)
        auto *asciiItem = new QTableWidgetItem(ascii);
        asciiItem->setFlags(asciiItem->flags() & ~Qt::ItemIsEditable);
        m_table->setItem(row, 1 + MEMORY_BYTES_PER_ROW, asciiItem);
    }

    m_table->blockSignals(false);

    // Update page label
    uint32_t pageEnd = m_baseAddress + MEMORY_PAGE_SIZE - 1;
    m_pageLabel->setText(QString("0x%1 — 0x%2")
        .arg(m_baseAddress, 8, 16, QChar('0')).toUpper()
        .arg(pageEnd,       8, 16, QChar('0')).toUpper());

    m_prevButton->setEnabled(m_baseAddress >= MEMORY_PAGE_SIZE);
    m_nextButton->setEnabled(m_baseAddress + MEMORY_PAGE_SIZE < m_size);
}

void MemoryPanel::setMemory(const uint8_t *data, size_t size)
{
    m_mem      = data;
    m_memEdit  = nullptr;   // read-only source; manual edits won't write back
    m_size     = size;
    m_baseAddress = 0;
    refreshView();
}

void MemoryPanel::goToAddress(uint32_t address)
{
    // Align down to page boundary
    m_baseAddress = (address / MEMORY_PAGE_SIZE) * MEMORY_PAGE_SIZE;
    if (m_baseAddress >= m_size)
        m_baseAddress = 0;
    refreshView();
}

void MemoryPanel::onGoToClicked()
{
    bool ok = false;
    uint32_t addr = m_gotoField->text().toUInt(&ok, 16);
    if (ok) {
        pushHistory(addr);
        goToAddress(addr);
    }
    m_gotoField->clear();
}

void MemoryPanel::onNextPageClicked()
{
    uint32_t next = m_baseAddress + MEMORY_PAGE_SIZE;
    if (next < m_size) {
        m_baseAddress = next;
        refreshView();
    }
}

void MemoryPanel::onPrevPageClicked()
{
    if (m_baseAddress >= MEMORY_PAGE_SIZE) {
        m_baseAddress -= MEMORY_PAGE_SIZE;
        refreshView();
    }
}

void MemoryPanel::onCellChanged(int row, int col)
{
    // Ignore address column (0) and ASCII column (last)
    if (col == 0 || col == 1 + MEMORY_BYTES_PER_ROW) return;

    auto *item = m_table->item(row, col);
    if (!item) return;

    bool ok = false;
    uint8_t val = static_cast<uint8_t>(item->text().toUInt(&ok, 16));
    if (!ok) {
        // Revert to current memory value
        refreshView();
        return;
    }

    // Clamp to two hex digits display
    char buf[4];
    std::snprintf(buf, sizeof(buf), "%02X", val);
    // Block signal to avoid recursion while correcting text
    m_table->blockSignals(true);
    item->setText(QString(buf));
    m_table->blockSignals(false);

    // Write to writable backing memory if available
    size_t byteCol = static_cast<size_t>(col - 1);
    size_t idx = m_baseAddress + static_cast<size_t>(row) * MEMORY_BYTES_PER_ROW + byteCol;
    if (m_memEdit && idx < m_size)
        m_memEdit[idx] = val;
    else
        m_dummyMem[idx < DUMMY_SIZE ? idx : 0] = val;

    // Refresh ASCII column for this row only
    QString ascii;
    for (int c = 0; c < MEMORY_BYTES_PER_ROW; ++c) {
        auto *it = m_table->item(row, 1 + c);
        uint8_t b = it ? static_cast<uint8_t>(it->text().toUInt(nullptr, 16)) : 0;
        ascii += (b >= 0x20 && b < 0x7F) ? QChar(b) : QChar('.');
    }
    m_table->blockSignals(true);
    m_table->item(row, 1 + MEMORY_BYTES_PER_ROW)->setText(ascii);
    m_table->blockSignals(false);
}

void MemoryPanel::pushHistory(uint32_t address)
{
    // Remove duplicate if already present
    for (auto it = m_gotoHistory.begin(); it != m_gotoHistory.end(); ++it) {
        if (*it == address) { m_gotoHistory.erase(it); break; }
    }
    m_gotoHistory.push_front(address);
    if (m_gotoHistory.size() > MAX_HISTORY)
        m_gotoHistory.pop_back();
    rebuildHistoryMenu();
}

void MemoryPanel::rebuildHistoryMenu()
{
    m_historyMenu->clear();
    for (uint32_t addr : m_gotoHistory) {
        QString label = QString("0x%1").arg(addr, 8, 16, QChar('0')).toUpper();
        m_historyMenu->addAction(label)->setData(addr);
    }
    m_historyButton->setEnabled(!m_historyMenu->isEmpty());
}

void MemoryPanel::onHistoryTriggered(QAction *action)
{
    uint32_t addr = action->data().toUInt();
    goToAddress(addr);
    // Bring it to the front without re-adding a duplicate
    pushHistory(addr);
}
