#pragma once

#include <QDockWidget>
#include <QPlainTextEdit>
#include <QString>
#include <string>

class Console : public QDockWidget
{
    Q_OBJECT

public:
    explicit Console(QWidget *parent = nullptr);

    // Write a line with a trailing newline
    void println(const std::string &text);
    void println(const QString &text);

    // Write text without a newline
    void print(const std::string &text);
    void print(const QString &text);

    // Write a formatted error line (shown in red)
    void printError(const std::string &text);
    void printError(const QString &text);

    // Clear all output
    void clear();

private:
    QPlainTextEdit *m_output;

    void appendHtml(const QString &html);
};
