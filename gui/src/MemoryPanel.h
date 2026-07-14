#pragma once

#include <QDockWidget>
#include <QTableWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QMenu>
#include <cstdint>
#include <deque>
#include "Memory.h"

// Number of bytes shown per page
static constexpr int MEMORY_PAGE_SIZE  = 256;
// Bytes per row in the hex view
static constexpr int MEMORY_BYTES_PER_ROW = 16;

class MemoryPanel : public QDockWidget
{
    Q_OBJECT

public:
    explicit MemoryPanel(QWidget *parent = nullptr);

    // Set the sparse Memory object to display; jumps view to jumpTo address.
    void setMemory(Memory *mem, uint32_t jumpTo = 0);

    // Redraw the current page without changing the base address.
    void refresh() { refreshView(); }

    // Jump to a specific byte address
    void goToAddress(uint32_t address);

private slots:
    void onGoToClicked();
    void onNextPageClicked();
    void onPrevPageClicked();
    void onCellChanged(int row, int col);
    void onHistoryTriggered(QAction *action);

private:
    void buildTable();
    void refreshView();

    QTableWidget *m_table;
    QLineEdit    *m_gotoField;
    QPushButton  *m_gotoButton;
    QPushButton  *m_historyButton;
    QPushButton  *m_prevButton;
    QPushButton  *m_nextButton;
    QLabel       *m_pageLabel;
    QMenu        *m_historyMenu;

    static constexpr int MAX_HISTORY = 20;
    std::deque<uint32_t> m_gotoHistory;  // front = most recent

    void pushHistory(uint32_t address);
    void rebuildHistoryMenu();

    // Sparse Memory pointer (owned by Emulator)
    Memory  *m_coreMemory  = nullptr;

    uint32_t m_baseAddress = 0;   // first address shown on current page
};
