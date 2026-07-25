#pragma once

#include <QDockWidget>
#include <QTableWidget>
#include <QColor>
#include <QStringList>
#include <vector>

// Make sure this matches the file where you defined PipelineRow.
// If you renamed the file, update this include!
#include "InstructionState.h" 

class PipelinePanel : public QDockWidget
{
    Q_OBJECT

public:
    explicit PipelinePanel(QWidget *parent = nullptr);

    // New 3-argument signature using PipelineRow
    void updatePipeline(std::vector<PipelineRow>& rows, int currentCycle, const QStringList& sourceLines);

private:
    void setupUI();
    void setStageCell(int row, int col, const QString& stage, const QColor& color);

    QTableWidget *m_table;
};