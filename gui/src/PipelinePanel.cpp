#include "PipelinePanel.h"
#include <QVBoxLayout>
#include <QHeaderView>
#include <QScrollBar>

PipelinePanel::PipelinePanel(QWidget *parent)
    : QDockWidget("Pipeline Timeline", parent)
{
    setAllowedAreas(Qt::AllDockWidgetAreas);
    setupUI();
}

void PipelinePanel::setupUI()
{
    auto *root = new QWidget(this);
    auto *layout = new QVBoxLayout(root);
    layout->setContentsMargins(4, 4, 4, 4);

    m_table = new QTableWidget(this);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->setFocusPolicy(Qt::NoFocus);
    m_table->verticalHeader()->setVisible(false);
    m_table->setShowGrid(true);
    m_table->setGridStyle(Qt::DotLine);

    layout->addWidget(m_table);
    setWidget(root);
}

void PipelinePanel::updatePipeline(std::vector<PipelineRow>& rows, int currentCycle, const QStringList& sourceLines)
{
    // Limit to the last 40 cycles so Qt doesn't crash on long loops
    const int MAX_VISIBLE_CYCLES = 10; 
    
    int startCycle = std::max(1, currentCycle - MAX_VISIBLE_CYCLES + 1);
    int endCycle = std::max(5, currentCycle);
    int numCols = endCycle - startCycle + 1;

    // Filter rows to only those visible in our window
    std::vector<size_t> visibleRows;
    for (size_t i = 0; i < rows.size(); ++i) {
        int rowEndCycle = rows[i].startCycle + static_cast<int>(rows[i].stages.size()) - 1;
        if (rowEndCycle >= startCycle) {
            visibleRows.push_back(i);
        }
    }

    m_table->clearContents();
    m_table->setRowCount(static_cast<int>(visibleRows.size()));
    m_table->setColumnCount(numCols + 1); 

    QStringList headers;
    headers << "Instruction";
    for (int c = startCycle; c <= endCycle; ++c) {
        headers << QString("C%1").arg(c);
    }
    m_table->setHorizontalHeaderLabels(headers);

    // --- UI SCALING LOGIC ---
    // The instruction text column stretches to fill empty space
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->horizontalHeader()->setMinimumSectionSize(50); 

    for (int i = 1; i <= numCols; ++i) {
        if (numCols < 15) {
            // If few columns, let them stretch dynamically to fill the window
            m_table->horizontalHeader()->setSectionResizeMode(i, QHeaderView::Stretch);
        } else {
            // If many columns, lock them to a readable width and enable scrolling
            m_table->horizontalHeader()->setSectionResizeMode(i, QHeaderView::Fixed);
            m_table->setColumnWidth(i, 45);
        }
    }
    // ------------------------

    for (int rowIdx = 0; rowIdx < visibleRows.size(); ++rowIdx) {
        auto& row = rows[visibleRows[rowIdx]];

        if (row.instructionText.empty()) {
            bool ok;
            int lineNum = QString::fromStdString(row.lineStr).toInt(&ok);
            if (ok && lineNum > 0 && lineNum <= sourceLines.size()) {
                row.instructionText = sourceLines[lineNum - 1].trimmed().toStdString();
            } else {
                // It's out-of-bounds memory fetched blindly by the pipeline
                row.instructionText = "<speculative fetch>"; 
            }
        }

        auto *instItem = new QTableWidgetItem(QString::fromStdString(row.instructionText));
        instItem->setFont(QFont("Courier New", 10));
        m_table->setItem(rowIdx, 0, instItem);

        int cyclePos = row.startCycle;
        for (const std::string& stage : row.stages) {
            if (cyclePos >= startCycle && cyclePos <= endCycle) {
                int colIdx = cyclePos - startCycle + 1;
                
                QColor stageColor = Qt::darkGray;
                if (stage == "IF")       stageColor = QColor(42, 75, 124);
                else if (stage == "ID")  stageColor = QColor(46, 111, 64);
                else if (stage == "EX")  stageColor = QColor(122, 96, 33);
                else if (stage == "MEM") stageColor = QColor(122, 62, 33);
                else if (stage == "WB")  stageColor = QColor(91, 42, 124);
                else if (stage == "FLUSH") stageColor = QColor(150, 40, 40);

                setStageCell(rowIdx, colIdx, QString::fromStdString(stage), stageColor);
            }
            cyclePos++;
        }
    }

    if (numCols > 0) {
        m_table->scrollToItem(m_table->item(0, numCols));
    }
}

void PipelinePanel::setStageCell(int row, int col, const QString& stage, const QColor& color)
{
    auto *item = new QTableWidgetItem(stage);
    item->setTextAlignment(Qt::AlignCenter);
    item->setBackground(color);
    item->setForeground(Qt::white);
    
    QFont font;
    font.setBold(true);
    item->setFont(font);

    m_table->setItem(row, col, item);
}