#pragma once

#include <QPlainTextEdit>
#include <QWidget>
#include <QSet>
#include "Emulator.h"


class CodeEditor : public QPlainTextEdit
{
    Q_OBJECT

public:
    explicit CodeEditor(QWidget *parent = nullptr, Emulator *emulator = nullptr);

    QString filePath() const { return m_filePath; }
    void setFilePath(const QString &path) { m_filePath = path; }
    
    void setExecutionLine(int blockNumber);
    void lineNumberAreaPaintEvent(QPaintEvent *event);
    void lineNumberAreaMousePressEvent(QMouseEvent *event);
    int lineNumberAreaWidth();
    void toggleBreakpoint(int blockNumber);

protected:
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void updateLineNumberAreaWidth(int newBlockCount);
    void highlightCurrentLine();
    void updateHighlights();
    void updateLineNumberArea(const QRect &rect, int dy);

private:
    QWidget *lineNumberArea;
    int m_executionLine = -1;
    QSet<int> m_breakpoints; // Stores 0-indexed line numbers where breakpoints are set
    Emulator *m_emulator = nullptr; // Pointer to the emulator for breakpoint management
    QString m_filePath;
};

// Companion widget strictly for drawing the line numbers and catching clicks
class LineNumberArea : public QWidget
{
public:
    LineNumberArea(CodeEditor *editor) : QWidget(editor), codeEditor(editor) {}

    QSize sizeHint() const override {
        return QSize(codeEditor->lineNumberAreaWidth(), 0);
    }

protected:
    void paintEvent(QPaintEvent *event) override {
        codeEditor->lineNumberAreaPaintEvent(event);
    }
    void mousePressEvent(QMouseEvent *event) override;

private:
    CodeEditor *codeEditor;
};