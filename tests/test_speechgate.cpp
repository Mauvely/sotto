#include "stt/SpeechGate.h"

#include <QtTest>

class TestSpeechGate : public QObject
{
    Q_OBJECT

private slots:
    void silenceStaysInactive()
    {
        SpeechGate gate;
        for (int64_t t = 0; t < 2000; t += 30)
            QVERIFY(!gate.update(0.001, t));
    }

    void speechActivates()
    {
        SpeechGate gate;
        for (int64_t t = 0; t < 500; t += 30)
            gate.update(0.001, t);
        QVERIFY(gate.update(0.08, 530));
        QCOMPARE(gate.lastVoiceMs(), int64_t(530));
    }

    void hangoverThenRelease()
    {
        SpeechGate gate;
        for (int64_t t = 0; t < 500; t += 30)
            gate.update(0.001, t);
        gate.update(0.08, 500);

        // Within the hangover window the gate stays open on silence…
        QVERIFY(gate.update(0.001, 600));
        // …and closes once the hangover has elapsed.
        QVERIFY(!gate.update(0.001, 900));
    }

    void noiseFloorAdaptsToLoudEnvironments()
    {
        SpeechGate gate;
        // A constant fan at RMS 0.02 must stop counting as speech once the
        // floor has adapted to it.
        bool active = true;
        for (int64_t t = 0; t < 60000; t += 30)
            active = gate.update(0.02, t);
        QVERIFY(!active);
        QVERIFY(gate.noiseFloor() > 0.015);
    }

    void resetKeepsNoiseFloor()
    {
        SpeechGate gate;
        for (int64_t t = 0; t < 3000; t += 30)
            gate.update(0.02, t);
        const double floor = gate.noiseFloor();
        gate.reset();
        QCOMPARE(gate.noiseFloor(), floor);
        QCOMPARE(gate.lastVoiceMs(), int64_t(-1));
    }
};

QTEST_APPLESS_MAIN(TestSpeechGate)
#include "test_speechgate.moc"
