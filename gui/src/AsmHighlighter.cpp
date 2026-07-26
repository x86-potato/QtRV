#include "AsmHighlighter.h"

#include <Assembler.h>
#include <QTextDocument>

AsmHighlighter::AsmHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
{
    setupFormats();
    setupRules();
}

void AsmHighlighter::setupFormats()
{
    m_identifierFormat.setForeground(QColor("#9CDCFE"));

    m_labelDefFormat.setForeground(QColor("#9CDCFE"));
    m_labelDefFormat.setFontWeight(QFont::Bold);

    m_numberFormat.setForeground(QColor("#B5CEA8"));

    m_registerFormat.setForeground(QColor("#DCDCAA"));

    m_directiveFormat.setForeground(QColor("#CE9178"));
    m_directiveFormat.setFontWeight(QFont::Bold);

    m_stringFormat.setForeground(QColor("#D69D85"));

    m_commentFormat.setForeground(QColor("#6A9955"));
    m_commentFormat.setFontItalic(true);

    // R/I/J-type and pseudo mnemonics each get a distinct color *and* a
    // distinct style, so the pipeline stage a given instruction belongs to
    // is recognizable at a glance even without reading the text.
    m_rTypeFormat.setForeground(QColor("#4EC9B0"));
    m_rTypeFormat.setFontWeight(QFont::Bold);

    m_iTypeFormat.setForeground(QColor("#569CD6"));
    m_iTypeFormat.setFontWeight(QFont::Bold);

    m_jTypeFormat.setForeground(QColor("#C586C0"));
    m_jTypeFormat.setFontWeight(QFont::Bold);
    m_jTypeFormat.setFontUnderline(true);

    m_pseudoFormat.setForeground(QColor("#9CDCFE"));
    m_pseudoFormat.setFontWeight(QFont::Bold);
    m_pseudoFormat.setFontItalic(true);
}

void AsmHighlighter::setupRules()
{
    // Applied first, in order; each later rule in this list (and the
    // dedicated label/mnemonic/string/comment passes below) overrides the
    // format of any earlier match on the same range.
    m_rules.push_back({ QRegularExpression(R"(\b[A-Za-z_]\w*\b)"), m_identifierFormat });
    m_rules.push_back({ QRegularExpression(R"(-?\b(?:0[xX][0-9A-Fa-f]+|\d+)\b)"), m_numberFormat });
    m_rules.push_back({ QRegularExpression(R"(\$[A-Za-z0-9]+\b)"), m_registerFormat });
    m_rules.push_back({ QRegularExpression(R"(\.[A-Za-z]+\b)"), m_directiveFormat });

    m_labelDefPattern = QRegularExpression(R"(^\s*([A-Za-z_]\w*)\s*:)");
    m_mnemonicPattern = QRegularExpression(R"(^\s*(?:[A-Za-z_]\w*\s*:\s*)?([A-Za-z]+)\b)");
    m_stringPattern   = QRegularExpression(R"("(?:\\.|[^"\\])*")");
    m_commentPattern  = QRegularExpression(R"(#[^\n]*)");
}

void AsmHighlighter::highlightBlock(const QString &text)
{
    // 1. Generic rules: identifiers, numbers, registers, directives.
    for (const auto &rule : m_rules) {
        auto it = rule.pattern.globalMatch(text);
        while (it.hasNext()) {
            auto match = it.next();
            setFormat(match.capturedStart(), match.capturedLength(), rule.format);
        }
    }

    // 2. Label definitions (identifier immediately followed by ':') override
    //    the generic identifier color with a bold label style.
    auto labelMatch = m_labelDefPattern.match(text);
    if (labelMatch.hasMatch()) {
        setFormat(labelMatch.capturedStart(1), labelMatch.capturedLength(1), m_labelDefFormat);
    }

    // 3. The mnemonic occupying the instruction position (after an optional
    //    leading label) gets colored/styled by its R/I/J/pseudo class.
    auto mnemonicMatch = m_mnemonicPattern.match(text);
    if (mnemonicMatch.hasMatch()) {
        QString mnemonic = mnemonicMatch.captured(1);
        InstrClass cls = classifyInstruction(mnemonic.toStdString());

        const QTextCharFormat *fmt = nullptr;
        switch (cls) {
            case InstrClass::R_TYPE: fmt = &m_rTypeFormat; break;
            case InstrClass::I_TYPE: fmt = &m_iTypeFormat; break;
            case InstrClass::J_TYPE: fmt = &m_jTypeFormat; break;
            case InstrClass::PSEUDO: fmt = &m_pseudoFormat; break;
            case InstrClass::UNKNOWN: fmt = nullptr; break;
        }

        if (fmt) {
            setFormat(mnemonicMatch.capturedStart(1), mnemonicMatch.capturedLength(1), *fmt);
        }
    }

    // 4. Strings override anything matched inside their quotes.
    auto stringIt = m_stringPattern.globalMatch(text);
    while (stringIt.hasNext()) {
        auto match = stringIt.next();
        setFormat(match.capturedStart(), match.capturedLength(), m_stringFormat);
    }

    // 5. Comments are applied last so they override everything from '#' to EOL.
    auto commentMatch = m_commentPattern.match(text);
    if (commentMatch.hasMatch()) {
        setFormat(commentMatch.capturedStart(), commentMatch.capturedLength(), m_commentFormat);
    }
}
