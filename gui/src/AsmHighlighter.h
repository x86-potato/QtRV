#pragma once

#include <QSyntaxHighlighter>
#include <QRegularExpression>
#include <QTextCharFormat>
#include <vector>

class QTextDocument;

// Rule-based syntax highlighter for MIPS assembly source.
// Colors/styles are looked up per-line with plain regexes; instruction
// categorization (R/I/J/pseudo) is delegated to Assembler::classifyInstruction
// so the mnemonic lists stay defined in exactly one place (the assembler).
class AsmHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT

public:
    explicit AsmHighlighter(QTextDocument *parent = nullptr);

protected:
    void highlightBlock(const QString &text) override;

private:
    struct Rule {
        QRegularExpression pattern;
        QTextCharFormat    format;
    };

    void setupFormats();
    void setupRules();

    std::vector<Rule> m_rules; // generic rules applied in order (registers, numbers, directives, generic identifiers)

    QRegularExpression m_labelDefPattern;
    QRegularExpression m_mnemonicPattern;
    QRegularExpression m_stringPattern;
    QRegularExpression m_commentPattern;

    QTextCharFormat m_identifierFormat;   // fallback/label-reference color
    QTextCharFormat m_labelDefFormat;     // bold label definitions
    QTextCharFormat m_numberFormat;
    QTextCharFormat m_registerFormat;
    QTextCharFormat m_directiveFormat;
    QTextCharFormat m_stringFormat;
    QTextCharFormat m_commentFormat;
    QTextCharFormat m_rTypeFormat;
    QTextCharFormat m_iTypeFormat;
    QTextCharFormat m_jTypeFormat;
    QTextCharFormat m_pseudoFormat;
};
