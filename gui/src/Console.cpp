#include "Console.h"

#include <QVBoxLayout>
#include <QScrollBar>
#include <QTextCursor>

Console::Console(QWidget *parent)
    : QDockWidget("Console", parent)
{
    setAllowedAreas(Qt::BottomDockWidgetArea | Qt::TopDockWidgetArea);

    m_output = new QPlainTextEdit(this);
    m_output->setReadOnly(true);
    m_output->setFont(QFont("Courier New", 10));
    m_output->setPlaceholderText("Program output will appear here...");
    m_output->setMaximumBlockCount(5000); // cap scrollback

    setWidget(m_output);
}

void Console::print(const QString &text)
{
    m_output->moveCursor(QTextCursor::End);
    
    // Normalize any weird Windows/Unix line endings to standard Qt newlines
    QString normalizedText = text;
    normalizedText.replace("\r\n", "\n"); 
    
    m_output->insertPlainText(normalizedText);
    m_output->verticalScrollBar()->setValue(m_output->verticalScrollBar()->maximum());
}

void Console::print(const std::string &text)
{
    print(QString::fromStdString(text));
}

void Console::println(const QString &text)
{
    print(text + "\n");
}

void Console::println(const std::string &text)
{
    println(QString::fromStdString(text));
}

void Console::printError(const QString &text)
{
    // 1. Escape the HTML to prevent injection
    QString escaped = text.toHtmlEscaped();
    
    // 2. Convert standard \n to HTML <br> tags so they actually drop down a line!
    escaped.replace("\n", "<br>");
    
    appendHtml("<span style=\"color:#f44;\">Error: " + escaped + "</span>");
}

void Console::printError(const std::string &text)
{
    printError(QString::fromStdString(text));
}

void Console::clear()
{
    m_output->clear();
}

void Console::appendHtml(const QString &html)
{
    m_output->moveCursor(QTextCursor::End);
    
    // appendHtml automatically inserts a paragraph break. 
    // To insert HTML WITHOUT a paragraph break, we use the text cursor:
    QTextCursor cursor = m_output->textCursor();
    cursor.movePosition(QTextCursor::End);
    cursor.insertHtml(html);
    
    m_output->setTextCursor(cursor);
    m_output->verticalScrollBar()->setValue(m_output->verticalScrollBar()->maximum());
}