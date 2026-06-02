#pragma once

#include <QDockWidget>
#include <QTableWidget>
#include <cstdint>
#include <array>

class RegisterPanel : public QDockWidget
{
    Q_OBJECT

public:
    explicit RegisterPanel(QWidget *parent = nullptr);

    // Update all registers at once (call after step/run)
    void setRegisters(const std::array<uint32_t, 32>& regs, uint32_t pc);

    // Highlight the register that changed most recently
    void highlightRegister(int index);

private:
    QTableWidget *m_table;

    void buildTable();
};
