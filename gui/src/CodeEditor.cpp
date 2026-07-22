#include "CodeEditor.h"

#include <QPainter>
#include <QMouseEvent>
#include <QTextBlock>

void LineNumberArea::mousePressEvent(QMouseEvent *event) {
    // Delegate the click to the main editor which has access to the protected math
    codeEditor->lineNumberAreaMousePressEvent(event);
}


void CodeEditor::lineNumberAreaMousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        // Now this works because CodeEditor is a subclass of QPlainTextEdit!
        QTextBlock block = firstVisibleBlock();
        int blockNumber = block.blockNumber();
        int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
        int bottom = top + qRound(blockBoundingRect(block).height());

        while (block.isValid() && top <= event->y()) {
            if (block.isVisible() && bottom >= event->y()) {
                toggleBreakpoint(blockNumber);
                break;
            }
            block = block.next();
            top = bottom;
            bottom = top + qRound(blockBoundingRect(block).height());
            blockNumber++;
        }
    }
}
void CodeEditor::setExecutionLine(int blockNumber) {
    m_executionLine = blockNumber;
    updateHighlights();
    
    // Auto-scroll the editor so the executing line is always visible
    if (blockNumber >= 0) {
        QTextBlock block = document()->findBlockByNumber(blockNumber);
        if (block.isValid()) {
            // Move the viewport to ensure the block is visible, 
            // without necessarily stealing the user's text cursor
            QTextCursor cursor(block);
            setTextCursor(cursor);
            ensureCursorVisible();
        }
    }
}

void CodeEditor::updateHighlights() {
    QList<QTextEdit::ExtraSelection> extraSelections;

    // 1. Always draw the text cursor highlight (Dark Gray)
    if (!isReadOnly()) {
        QTextEdit::ExtraSelection cursorSelection;
        cursorSelection.format.setBackground(QColor(45, 45, 45));
        cursorSelection.format.setProperty(QTextFormat::FullWidthSelection, true);
        cursorSelection.cursor = textCursor();
        cursorSelection.cursor.clearSelection();
        
        // Unconditionally add the cursor highlight
        extraSelections.append(cursorSelection);
    }

    // 2. Draw the Execution Line highlight (Dark Green)
    if (m_executionLine >= 0) {
        QTextEdit::ExtraSelection execSelection;
        execSelection.format.setBackground(QColor(30, 70, 30)); 
        execSelection.format.setProperty(QTextFormat::FullWidthSelection, true);
        
        QTextBlock block = document()->findBlockByNumber(m_executionLine);
        execSelection.cursor = QTextCursor(block);
        execSelection.cursor.clearSelection();
        
        // Add this second so it draws on top if the cursor and execution line are the same
        extraSelections.append(execSelection);
    }

    setExtraSelections(extraSelections);
}

CodeEditor::CodeEditor(QWidget *parent, Emulator *emulator) : QPlainTextEdit(parent), m_emulator(emulator) {
    lineNumberArea = new LineNumberArea(this);

    // Connect signals to keep the margin width and line highlight updated
    connect(this, &CodeEditor::blockCountChanged, this, &CodeEditor::updateLineNumberAreaWidth);
    connect(this, &CodeEditor::updateRequest, this, &CodeEditor::updateLineNumberArea);
    connect(this, &CodeEditor::cursorPositionChanged, this, &CodeEditor::highlightCurrentLine);
    connect(this, &CodeEditor::cursorPositionChanged, this, &CodeEditor::updateHighlights);

    updateLineNumberAreaWidth(0);
    highlightCurrentLine();
}

int CodeEditor::lineNumberAreaWidth() {
    int digits = 1;
    int max = qMax(1, blockCount());
    while (max >= 10) {
        max /= 10;
        ++digits;
    }
    // Calculate width: space for digits + extra padding for the breakpoint dot
    int space = 3 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits + 15;
    return space;
}

void CodeEditor::updateLineNumberAreaWidth(int /* newBlockCount */) {
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}

void CodeEditor::updateLineNumberArea(const QRect &rect, int dy) {
    if (dy)
        lineNumberArea->scroll(0, dy);
    else
        lineNumberArea->update(0, rect.y(), lineNumberArea->width(), rect.height());

    if (rect.contains(viewport()->rect()))
        updateLineNumberAreaWidth(0);
}

void CodeEditor::resizeEvent(QResizeEvent *e) {
    QPlainTextEdit::resizeEvent(e);
    QRect cr = contentsRect();
    lineNumberArea->setGeometry(QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
}

void CodeEditor::highlightCurrentLine() {
    QList<QTextEdit::ExtraSelection> extraSelections;

    if (!isReadOnly()) {
        QTextEdit::ExtraSelection selection;
        
        // Use a subtle dark gray for the active line highlight 
        // (Slightly lighter than your QPalette::Base of 20,20,20)
        QColor lineColor = QColor(45, 45, 45); 
        
        selection.format.setBackground(lineColor);
        selection.format.setProperty(QTextFormat::FullWidthSelection, true);
        selection.cursor = textCursor();
        selection.cursor.clearSelection();
        extraSelections.append(selection);
    }
    setExtraSelections(extraSelections);
}

void CodeEditor::lineNumberAreaPaintEvent(QPaintEvent *event) {
    QPainter painter(lineNumberArea);
    
    // Fill margin background with the application's Window color (30, 30, 30)
    painter.fillRect(event->rect(), palette().color(QPalette::Window));

    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + qRound(blockBoundingRect(block).height());

    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            
            // 1. Draw Breakpoint Indicator
            if (m_breakpoints.contains(blockNumber)) {
                painter.setBrush(Qt::red);
                painter.setPen(Qt::NoPen);
                int radius = 10;
                painter.drawEllipse(2, top + (fontMetrics().height() - radius) / 2, radius, radius);
            }

            // 2. Draw Line Number
            QString number = QString::number(blockNumber + 1);
            
            // Use a muted gray for the line numbers so they don't distract from the code
            painter.setPen(QColor(120, 120, 120));
            painter.drawText(0, top, lineNumberArea->width() - 4, fontMetrics().height(),
                             Qt::AlignRight | Qt::AlignVCenter, number);
        }

        block = block.next();
        top = bottom;
        bottom = top + qRound(blockBoundingRect(block).height());
        ++blockNumber;
    }
}

void CodeEditor::toggleBreakpoint(int blockNumber) {
    if (m_breakpoints.contains(blockNumber)) {
        m_breakpoints.remove(blockNumber);
        m_emulator->setBreakpoint(blockNumber, false);
    } else {
        m_breakpoints.insert(blockNumber);
        m_emulator->setBreakpoint(blockNumber, true);
    }
    lineNumberArea->update(); // Request a redraw to show/hide the red dot
    
}

