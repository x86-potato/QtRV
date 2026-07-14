#include "RegisterPanel.h"

#include <QVBoxLayout>
#include <QHeaderView>
#include <QFont>
#include <QColor>

// MIPS integer register names (ABI names)
static const char* kRegNames[32] = {
    "$zero", "$at", "$v0", "$v1",
    "$a0",   "$a1", "$a2", "$a3",
    "$t0",   "$t1", "$t2", "$t3", "$t4", "$t5", "$t6", "$t7",
    "$s0",   "$s1", "$s2", "$s3", "$s4", "$s5", "$s6", "$s7",
    "$t8",   "$t9",
    "$k0",   "$k1",
    "$gp",   "$sp", "$fp", "$ra"
};

static const char* kRegNumbers[32] = {
    "$0",  "$1",  "$2",  "$3",  "$4",  "$5",  "$6",  "$7",
    "$8",  "$9",  "$10", "$11", "$12", "$13", "$14", "$15",
    "$16", "$17", "$18", "$19", "$20", "$21", "$22", "$23",
    "$24", "$25", "$26", "$27", "$28", "$29", "$30", "$31"
};

RegisterPanel::RegisterPanel(QWidget *parent)
    : QDockWidget("Registers", parent)
{
    setAllowedAreas(Qt::AllDockWidgetAreas);

    m_table = new QTableWidget(this);
    m_table->setFont(QFont("Courier New", 10));
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setShowGrid(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setStretchLastSection(true);

    buildTable();

    auto *root = new QWidget(this);
    auto *vbox = new QVBoxLayout(root);
    vbox->setContentsMargins(4, 4, 4, 4);
    vbox->addWidget(m_table);
    setWidget(root);
}

void RegisterPanel::buildTable()
{
    // Rows: 32 GP registers + 1 PC row
    m_table->setColumnCount(5);
    m_table->setRowCount(33);
    m_table->setHorizontalHeaderLabels({ "Reg", "Num", "Hex", "Decimal", "ASCII" });

    for (int i = 0; i < 32; ++i) {
        m_table->setItem(i, 0, new QTableWidgetItem(kRegNames[i]));
        m_table->setItem(i, 1, new QTableWidgetItem(kRegNumbers[i]));
        m_table->setItem(i, 2, new QTableWidgetItem("0x00000000"));
        m_table->setItem(i, 3, new QTableWidgetItem("0"));
        m_table->setItem(i, 4, new QTableWidgetItem(""));
    }

    // PC row
    m_table->setItem(32, 0, new QTableWidgetItem("PC"));
    m_table->setItem(32, 1, new QTableWidgetItem(""));
    m_table->setItem(32, 2, new QTableWidgetItem("0x00000000"));
    m_table->setItem(32, 3, new QTableWidgetItem("0"));
    m_table->setItem(32, 4, new QTableWidgetItem(""));

    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
}

void RegisterPanel::setRegisters(const std::array<uint32_t, 32>& regs, uint32_t pc)
{
    auto fillRow = [&](int row, uint32_t val, bool showAscii) {
        // Hex
        if (auto *it = m_table->item(row, 2))
            it->setText(QString("0x%1").arg(val, 8, 16, QChar('0')).toUpper());
        // Decimal (signed)
        if (auto *it = m_table->item(row, 3))
            it->setText(QString::number(static_cast<int32_t>(val)));
        // ASCII — show up to 4 printable chars packed in the word (big-endian byte order)
        if (auto *it = m_table->item(row, 4)) {
            if (!showAscii) { it->setText(""); return; }
            QString ascii;
            for (int shift = 24; shift >= 0; shift -= 8) {
                uint8_t b = static_cast<uint8_t>((val >> shift) & 0xFF);
                ascii += (b >= 0x20 && b < 0x7F) ? QChar(b) : QChar('.');
            }
            it->setText(ascii);
        }
    };

    for (int i = 0; i < 32; ++i)
        fillRow(i, regs[i], true);

    fillRow(32, pc, false);   // PC: decimal/hex only, no ASCII
}

void RegisterPanel::highlightRegister(int index)
{
    if (index < 0 || index >= 32) return;
    for (int col = 0; col < 5; ++col) {
        auto *item = m_table->item(index, col);
        if (item)
            item->setBackground(QColor(42, 130, 218, 80));
    }
}
