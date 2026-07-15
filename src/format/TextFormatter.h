#pragma once

#include <QList>
#include <QString>

// Turns raw Whisper output into clean written text. Whisper already emits
// punctuation and capitalisation; this layer strips non-speech artifacts,
// applies voice commands ("new line", "new paragraph", "delete last
// line/sentence" — each individually toggleable), inserts paragraph breaks
// based on how long the speaker paused between utterances, and normalises
// whitespace/punctuation spacing.
//
// Pure functions, no Qt GUI deps — unit-tested in tests/test_formatter.cpp.
class TextFormatter
{
public:
    struct Utterance {
        QString text;             // raw whisper text for one utterance
        qint64 pauseBeforeMs = 0; // silence between this and the previous utterance
    };

    struct Options {
        bool voiceCommands = true;         // master switch for all commands
        bool cmdNewLine = true;            // "new line"
        bool cmdNewParagraph = true;       // "new paragraph"
        bool cmdDeleteLastLine = true;     // "delete/remove/scratch last line"
        bool cmdDeleteLastSentence = true; // "delete/remove/scratch last sentence"
        double paragraphPauseSec = 2.0;    // pause length that starts a new paragraph
    };

    static QString format(const QList<Utterance> &utterances, const Options &opts);

    // Artifact stripping for a single raw transcript chunk ([MUSIC],
    // "(laughs)", ♪ …), whitespace-collapsed and trimmed.
    static QString cleanTranscript(QString text);

    // Replaces spoken break commands with literal breaks. The delete
    // commands are handled in format() because they edit text that has
    // already been committed. Exposed for tests.
    static QString applyVoiceCommands(QString text, const Options &opts);
    static QString applyVoiceCommands(QString text); // all commands enabled
};
