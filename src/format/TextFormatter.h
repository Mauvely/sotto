#pragma once

#include <QList>
#include <QString>

// Turns raw Whisper output into clean written text. Whisper already emits
// punctuation and capitalisation; this layer strips non-speech artifacts,
// applies "new line"/"new paragraph" voice commands, inserts paragraph
// breaks based on how long the speaker paused between utterances, and
// normalises whitespace/punctuation spacing.
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
        bool voiceCommands = true;      // "new line" / "new paragraph"
        double paragraphPauseSec = 2.0; // pause length that starts a new paragraph
    };

    static QString format(const QList<Utterance> &utterances, const Options &opts);

    // Artifact stripping for a single raw transcript chunk ([MUSIC],
    // "(laughs)", ♪ …), whitespace-collapsed and trimmed.
    static QString cleanTranscript(QString text);

    // Replaces spoken commands with literal breaks. Exposed for tests.
    static QString applyVoiceCommands(QString text);
};
