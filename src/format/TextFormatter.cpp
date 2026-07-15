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

// "delete last line": drop everything after the last line break, keeping
// the break itself so the next words land where the deleted line was.
// Without any line break the whole text is the last line.
void chopLastLine(QString &text)
{
    int end = text.size();
    while (end > 0 && text.at(end - 1).isSpace())
        --end;
    if (end == 0) {
        text.clear();
        return;
    }
    const int idx = text.lastIndexOf(u'\n', end - 1);
    if (idx < 0)
        text.clear();
    else
        text.truncate(idx + 1);
}

// "delete last sentence": cut back to the end of the previous sentence on
// the same line (or the line break, so paragraph structure survives).
void chopLastSentence(QString &text)
{
    int end = text.size();
    while (end > 0 && text.at(end - 1).isSpace())
        --end;
    // The last sentence's own terminator/closing quotes belong to it.
    while (end > 0) {
        const QChar c = text.at(end - 1);
        if (c == u'.' || c == u'!' || c == u'?' || c == u'…' || c == u':'
            || c == u'"' || c == u'”' || c == u'\'' || c == u'’' || c == u')')
            --end;
        else
            break;
    }
    int cut = 0;
    for (int i = end - 1; i >= 0; --i) {
        const QChar c = text.at(i);
        if (c == u'\n' || c == u'.' || c == u'!' || c == u'?' || c == u'…') {
            cut = i + 1;
            break;
        }
    }
    text.truncate(cut);
    while (text.endsWith(u' '))
        text.chop(1);
}

// Matches the edit commands; capture 1 says which ("line"/"sentence").
const QRegularExpression &editCommandRe()
{
    static const QRegularExpression re(
        QStringLiteral(
            R"(\s*\b(?:delete|remove|scratch|erase)\s+(?:the\s+)?last\s+(line|sentence)\b[,.;:!?]*\s*)"),
        QRegularExpression::CaseInsensitiveOption);
    return re;
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
    return applyVoiceCommands(std::move(text), Options{});
}

QString TextFormatter::applyVoiceCommands(QString text, const Options &opts)
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
    if (opts.cmdNewParagraph)
        text.replace(newParagraph, QStringLiteral("\n\n"));
    if (opts.cmdNewLine)
        text.replace(newLine, QStringLiteral("\n"));
    return text;
}

QString TextFormatter::format(const QList<Utterance> &utterances, const Options &opts)
{
    const bool anyEditCommand =
        opts.voiceCommands && (opts.cmdDeleteLastLine || opts.cmdDeleteLastSentence);

    QString result;
    for (const Utterance &u : utterances) {
        QString text = cleanTranscript(u.text);
        if (opts.voiceCommands)
            text = applyVoiceCommands(text, opts);

        // Join rules: the first appended segment of an utterance gets the
        // pause-based paragraph treatment; anything after an edit command
        // continues with a plain space. Text resumed right after a line
        // break (e.g. after "delete last line") gets no separator at all.
        bool firstSegment = true;
        auto appendSegment = [&](QString seg) {
            if (seg.trimmed().isEmpty())
                return;
            if (!result.isEmpty() && !result.endsWith(u'\n')) {
                const bool paragraph = firstSegment
                    && u.pauseBeforeMs >= qint64(opts.paragraphPauseSec * 1000.0);
                if (paragraph) {
                    // A paragraph break implies the previous sentence ended.
                    if (!endsSentence(result))
                        result += u'.';
                    result += QStringLiteral("\n\n");
                } else {
                    result += u' ';
                    // Utterances decode independently, so re-capitalise after
                    // a completed sentence in case whisper started lowercase.
                    if (endsSentence(result) && !seg.isEmpty() && seg.at(0).isLetter())
                        seg[0] = seg.at(0).toUpper();
                }
            }
            result += seg;
            firstSegment = false;
        };

        if (!anyEditCommand) {
            appendSegment(text);
            continue;
        }

        // Split around edit commands, applying each to everything committed
        // so far ("this is bad. delete last sentence" first appends, then
        // deletes). A disabled command is kept as literal text.
        int last = 0;
        auto it = editCommandRe().globalMatch(text);
        while (it.hasNext()) {
            const auto m = it.next();
            const bool isLine = m.captured(1).compare(QStringLiteral("line"),
                                                      Qt::CaseInsensitive) == 0;
            if ((isLine && !opts.cmdDeleteLastLine)
                || (!isLine && !opts.cmdDeleteLastSentence))
                continue;
            appendSegment(text.mid(last, m.capturedStart() - last));
            last = m.capturedEnd();
            if (isLine)
                chopLastLine(result);
            else
                chopLastSentence(result);
            firstSegment = false; // the deletion consumed the utterance join
        }
        appendSegment(text.mid(last));
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
