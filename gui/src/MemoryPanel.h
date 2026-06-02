#pragma once

#include <QDockWidget>
#include <QTableWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QMenu>
#include <cstdint>
#include <deque>

// Number of bytes shown per page
static constexpr int MEMORY_PAGE_SIZE  = 256;
// Bytes per row in the hex view
static constexpr int MEMORY_BYTES_PER_ROW = 16;

class MemoryPanel : public QDockWidget
{
    Q_OBJECT

public:
    explicit MemoryPanel(QWidget *parent = nullptr);

    // Replace the backing memory shown (call after each step/run)
    void setMemory(const uint8_t *data, size_t size);

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

    // Dummy memory for now — replaced by setMemory() once core is wired up
    static constexpr size_t DUMMY_SIZE = 4096;
    uint8_t  m_dummyMem[DUMMY_SIZE] = {};
    const uint8_t *m_mem     = m_dummyMem;
    uint8_t       *m_memEdit = m_dummyMem;  // writable pointer for manual edits
    size_t         m_size    = DUMMY_SIZE;

    uint32_t m_baseAddress = 0;   // first address shown on current page
};
