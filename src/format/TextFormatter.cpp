#include "format/TextFormatter.h"

#include <QRegularExpression>

namespace {

const QRegularExpression &bracketTagRe()
{
    // [BLANK_AUDIO], [MUSIC], [inaudible] ... — whisper uses brackets for meta only
    static const QRegularExpression re(QStringLiteral(R"(\[[^\[\]]{0,60}\])"));
    return re;
}

const QRegularExpression &parentheticalRe()
{
    static const QRegularExpression re(QStringLiteral(R"(\(([^()]{0,60})\))"));
    return re;
}

bool isNoiseDescription(const QString &content)
{
    static const QRegularExpression noiseWords(
        QStringLiteral("\\b(laugh|chuckl|music|applause|clap|cough|sigh|inaudible|indistinct|"
                       "silence|silent|noise|static|typing|keyboard|clears? throat|wind|breath|"
                       "beep|click|hum|buzz|murmur|speaking|speaks|singing|sings|whistl|footsteps|"
                       "door|engine|birds?|dog|barking)\\w*\\b"),
        QRegularExpression::CaseInsensitiveOption);
    return noiseWords.match(content).hasMatch();
}

const QRegularExpression &whitespaceRe()
{
    static const QRegularExpression re(QStringLiteral(R"(\s+)"));
    return re;
}

bool endsSentence(const QString &text)
{
    for (int i = text.size() - 1; i >= 0; --i) {
        const QChar c = text.at(i);
        if (c.isSpace() || c == u'"' || c == u'”' || c == u')' || c == u'’')
            continue;
        return c == u'.' || c == u'!' || c == u'?' || c == u':' || c == u'…';
    }
    return true; // empty counts as "nothing to terminate"
}

void capitalizeParagraphStarts(QString &text)
{
    bool atStart = true;
    for (int i = 0; i < text.size(); ++i) {
        if (atStart && text.at(i).isLetter()) {
            text[i] = text.at(i).toUpper();
            atStart = false;
        } else if (!text.at(i).isSpace() && text.at(i) != u'\n') {
            atStart = false;
        }
        if (i + 1 < text.size() && text.at(i) == u'\n' && text.at(i + 1) == u'\n')
            atStart = true;
    }
    if (!text.isEmpty() && text.at(0).isLetter())
        text[0] = text.at(0).toUpper();
}

} // namespace

QString TextFormatter::cleanTranscript(QString text)
{
    text.remove(bracketTagRe());
    text.remove(QChar(u'♪')); // ♪

    // Remove parentheticals only when they describe noise, not real speech.
    QString out;
    out.reserve(text.size());
    int last = 0;
    auto it = parentheticalRe().globalMatch(text);
    while (it.hasNext()) {
        const auto m = it.next();
        if (isNoiseDescription(m.captured(1))) {
            out += text.mid(last, m.capturedStart() - last);
            last = m.capturedEnd();
        }
    }
    out += text.mid(last);

    out.replace(whitespaceRe(), QStringLiteral(" "));
    return out.trimmed();
}

QString TextFormatter::applyVoiceCommands(QString text)
{
    // Consume punctuation the recognizer wrapped around the command itself,
    // but keep anything before it — a comma preceding "new line" is usually
    // intentional ("Hi John, new line I am writing…").
    static const QRegularExpression newParagraph(
        QStringLiteral(R"(\s*\b(?:new|next)[ -]?paragraph\b[,.;:!?]*\s*)"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression newLine(
        QStringLiteral(R"(\s*\b(?:new|next)[ -]?line\b[,.;:!?]*\s*)"),
        QRegularExpression::CaseInsensitiveOption);
    text.replace(newParagraph, QStringLiteral("\n\n"));
    text.replace(newLine, QStringLiteral("\n"));
    return text;
}

QString TextFormatter::format(const QList<Utterance> &utterances, const Options &opts)
{
    QString result;
    for (const Utterance &u : utterances) {
        QString text = cleanTranscript(u.text);
        if (opts.voiceCommands)
            text = applyVoiceCommands(text);
        if (text.trimmed().isEmpty())
            continue;

        if (!result.isEmpty()) {
            const bool paragraph = u.pauseBeforeMs >= qint64(opts.paragraphPauseSec * 1000.0);
            if (paragraph) {
                // A paragraph break implies the previous sentence ended.
                if (!endsSentence(result))
                    result += u'.';
                result += QStringLiteral("\n\n");
            } else {
                result += u' ';
                // Utterances decode independently, so re-capitalise after a
                // completed sentence in case whisper started lowercase.
                if (endsSentence(result) && !text.isEmpty() && text.at(0).isLetter())
                    text[0] = text.at(0).toUpper();
            }
        }
        result += text;
    }

    // Normalise whitespace produced by joins and voice commands.
    static const QRegularExpression spaceAroundNewline(QStringLiteral(R"([ \t]*\n[ \t]*)"));
    static const QRegularExpression tooManyNewlines(QStringLiteral(R"(\n{3,})"));
    static const QRegularExpression multiSpace(QStringLiteral(R"([ \t]{2,})"));
    static const QRegularExpression spaceBeforePunct(QStringLiteral(R"([ \t]+([,.;:!?]))"));
    result.replace(spaceAroundNewline, QStringLiteral("\n"));
    result.replace(tooManyNewlines, QStringLiteral("\n\n"));
    result.replace(multiSpace, QStringLiteral(" "));
    result.replace(spaceBeforePunct, QStringLiteral("\\1"));

    capitalizeParagraphStarts(result);
    return result.trimmed();
}
