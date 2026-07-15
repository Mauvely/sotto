#include "format/TextFormatter.h"

#include <QtTest>

using Utterance = TextFormatter::Utterance;
using Options = TextFormatter::Options;

class TestFormatter : public QObject
{
    Q_OBJECT

private slots:
    void stripsBracketArtifacts()
    {
        QCOMPARE(TextFormatter::cleanTranscript(QStringLiteral("[BLANK_AUDIO] Hello world. [MUSIC]")),
                 QStringLiteral("Hello world."));
    }

    void stripsNoiseParentheticals()
    {
        QCOMPARE(TextFormatter::cleanTranscript(QStringLiteral("So (laughs) that went well.")),
                 QStringLiteral("So that went well."));
    }

    void keepsMeaningfulParentheticals()
    {
        QCOMPARE(TextFormatter::cleanTranscript(QStringLiteral("The result (about 40%) was fine.")),
                 QStringLiteral("The result (about 40%) was fine."));
    }

    void collapsesWhitespace()
    {
        QCOMPARE(TextFormatter::cleanTranscript(QStringLiteral("  Hello   there \n world ")),
                 QStringLiteral("Hello there world"));
    }

    void newLineCommand()
    {
        QCOMPARE(TextFormatter::applyVoiceCommands(QStringLiteral("Hi John, new line I am writing to you")),
                 QStringLiteral("Hi John,\nI am writing to you"));
    }

    void newLineCommandSwallowsItsOwnPunctuation()
    {
        QCOMPARE(TextFormatter::applyVoiceCommands(QStringLiteral("Best regards new line. John")),
                 QStringLiteral("Best regards\nJohn"));
    }

    void newParagraphCommand()
    {
        QCOMPARE(TextFormatter::applyVoiceCommands(QStringLiteral("done. New paragraph, moving on")),
                 QStringLiteral("done.\n\nmoving on"));
    }

    void paragraphBreakOnLongPause()
    {
        const QString out = TextFormatter::format(
            {{QStringLiteral("Good morning."), 0},
             {QStringLiteral("I am writing regarding the invoice."), 2500}},
            Options{});
        QCOMPARE(out, QStringLiteral("Good morning.\n\nI am writing regarding the invoice."));
    }

    void shortPauseJoinsWithSpace()
    {
        const QString out = TextFormatter::format(
            {{QStringLiteral("Good morning."), 0},
             {QStringLiteral("how are you?"), 500}},
            Options{});
        QCOMPARE(out, QStringLiteral("Good morning. How are you?"));
    }

    void paragraphBreakAddsMissingPeriod()
    {
        const QString out = TextFormatter::format(
            {{QStringLiteral("this is one thought"), 0},
             {QStringLiteral("and a new topic"), 3000}},
            Options{});
        QCOMPARE(out, QStringLiteral("This is one thought.\n\nAnd a new topic"));
    }

    void capitalizesFirstLetter()
    {
        QCOMPARE(TextFormatter::format({{QStringLiteral("hello there"), 0}}, Options{}),
                 QStringLiteral("Hello there"));
    }

    void voiceCommandsCanBeDisabled()
    {
        Options opts;
        opts.voiceCommands = false;
        QCOMPARE(TextFormatter::format({{QStringLiteral("a new line of products"), 0}}, opts),
                 QStringLiteral("A new line of products"));
    }

    void deleteLastSentenceCommand()
    {
        QCOMPARE(TextFormatter::format(
                     {{QStringLiteral("This is great. This is terrible. Delete last sentence."), 0}},
                     Options{}),
                 QStringLiteral("This is great."));
    }

    void deleteLastSentenceAcrossUtterances()
    {
        const QString out = TextFormatter::format(
            {{QStringLiteral("This is great."), 0},
             {QStringLiteral("This is terrible."), 500},
             {QStringLiteral("remove the last sentence"), 800},
             {QStringLiteral("this is better."), 500}},
            Options{});
        QCOMPARE(out, QStringLiteral("This is great. This is better."));
    }

    void deleteLastLineCommand()
    {
        QCOMPARE(TextFormatter::format(
                     {{QStringLiteral("Shopping list new line apples new line oranges delete last line"), 0}},
                     Options{}),
                 QStringLiteral("Shopping list\napples"));
    }

    void deleteLastLineKeepsTheBreak()
    {
        // Redoing a line: the deleted line's break survives so the next
        // words land where the bad line was.
        const QString out = TextFormatter::format(
            {{QStringLiteral("Dear John new line I am riding to you"), 0},
             {QStringLiteral("delete last line I am writing to you"), 600}},
            Options{});
        QCOMPARE(out, QStringLiteral("Dear John\nI am writing to you"));
    }

    void deleteLastLineWithoutBreaksClearsAll()
    {
        QCOMPARE(TextFormatter::format(
                     {{QStringLiteral("just one line of text"), 0},
                      {QStringLiteral("scratch the last line"), 500}},
                     Options{}),
                 QString());
    }

    void deleteOnEmptyIsSafe()
    {
        QCOMPARE(TextFormatter::format({{QStringLiteral("delete last sentence"), 0}}, Options{}),
                 QString());
    }

    void deleteCommandsCanBeDisabledIndividually()
    {
        Options opts;
        opts.cmdDeleteLastSentence = false;
        QCOMPARE(TextFormatter::format({{QStringLiteral("please delete the last sentence"), 0}}, opts),
                 QStringLiteral("Please delete the last sentence"));
    }

    void masterSwitchDisablesEditCommandsToo()
    {
        Options opts;
        opts.voiceCommands = false;
        QCOMPARE(TextFormatter::format({{QStringLiteral("erase last line"), 0}}, opts),
                 QStringLiteral("Erase last line"));
    }

    void breakCommandsCanBeDisabledIndividually()
    {
        Options opts;
        opts.cmdNewLine = false;
        QCOMPARE(TextFormatter::applyVoiceCommands(
                     QStringLiteral("a new line here new paragraph there"), opts),
                 QStringLiteral("a new line here\n\nthere"));
    }

    void fixesSpaceBeforePunctuation()
    {
        QCOMPARE(TextFormatter::format({{QStringLiteral("Hello , world ."), 0}}, Options{}),
                 QStringLiteral("Hello, world."));
    }

    void noiseOnlyUtterancesDisappear()
    {
        const QString out = TextFormatter::format(
            {{QStringLiteral("[BLANK_AUDIO]"), 0},
             {QStringLiteral("Actual words."), 400},
             {QStringLiteral("(wind blowing)"), 400}},
            Options{});
        QCOMPARE(out, QStringLiteral("Actual words."));
    }

    void emptyInputGivesEmptyOutput()
    {
        QCOMPARE(TextFormatter::format({}, Options{}), QString());
    }

    void wholeFlowExample()
    {
        // "Good morning. I am writing regarding..." — the Whispr Flow pitch.
        const QString out = TextFormatter::format(
            {{QStringLiteral("Good morning."), 0},
             {QStringLiteral("I am writing regarding your email from Monday."), 2400},
             {QStringLiteral("best regards new line Tim"), 2600}},
            Options{});
        QCOMPARE(out,
                 QStringLiteral("Good morning.\n\nI am writing regarding your email from Monday.\n\n"
                                "Best regards\nTim"));
    }
};

QTEST_APPLESS_MAIN(TestFormatter)
#include "test_formatter.moc"
