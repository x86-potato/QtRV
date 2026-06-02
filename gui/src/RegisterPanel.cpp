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
    m_table->setColumnCount(3);
    m_table->setRowCount(33);
    m_table->setHorizontalHeaderLabels({ "Reg", "Num", "Value (hex)" });

    for (int i = 0; i < 32; ++i) {
        m_table->setItem(i, 0, new QTableWidgetItem(kRegNames[i]));
        m_table->setItem(i, 1, new QTableWidgetItem(kRegNumbers[i]));
        m_table->setItem(i, 2, new QTableWidgetItem("0x00000000"));
    }

    // PC row
    m_table->setItem(32, 0, new QTableWidgetItem("PC"));
    m_table->setItem(32, 1, new QTableWidgetItem(""));
    m_table->setItem(32, 2, new QTableWidgetItem("0x00000000"));

    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
}

void RegisterPanel::setRegisters(const std::array<uint32_t, 32>& regs, uint32_t pc)
{
    for (int i = 0; i < 32; ++i) {
        auto *item = m_table->item(i, 2);
        if (item)
            item->setText(QString("0x%1").arg(regs[i], 8, 16, QChar('0')).toUpper());
    }
    auto *pcItem = m_table->item(32, 2);
    if (pcItem)
        pcItem->setText(QString("0x%1").arg(pc, 8, 16, QChar('0')).toUpper());
}

void RegisterPanel::highlightRegister(int index)
{
    if (index < 0 || index >= 32) return;
    for (int col = 0; col < 3; ++col) {
        auto *item = m_table->item(index, col);
        if (item)
            item->setBackground(QColor(42, 130, 218, 80));
    }
}
