// ─────────────────────────────────────────────────────────────────────────────
//  autostart_migration — an entry written under the old app id is moved to the
//  new one, and "Launch at login" stays on.
//
//  The ids moved from net.mauvely.sotto.app to com.mauvely.sotto on 2026-10-14.
//  Anyone who had the switch on has an entry in ~/.config/autostart named by the
//  old id. It still starts Sotto, but the window no longer matches it, and
//  launchAtLogin() looks for the new name and reads "off".
//  Settings::migrateAutostart() moves it; this pins that with the app's real
//  APP_ID and APP_PREVIOUS_ID, so losing the PREVIOUS_APP_ID argument in
//  CMakeLists.txt fails here.
//
//  XDG_CONFIG_HOME is pointed at a temporary directory first, so nothing here
//  touches the real autostart directory. Linux only, like the migration.
// ─────────────────────────────────────────────────────────────────────────────

#include "core/Settings.h"

#include <QGuiApplication>
#include <QtTest>

class TestAutostartMigration : public QObject
{
    Q_OBJECT

    QString m_dir;
    QString m_oldPath;
    QString m_newPath;
    QByteArray m_old;
    QByteArray m_expected;

    static QByteArray read(const QString &path)
    {
        QFile f(path);
        return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
    }
    static void write(const QString &path, const QByteArray &text)
    {
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
        QCOMPARE(f.write(text), qint64(text.size()));
    }

private slots:
    void initTestCase()
    {
        QCoreApplication::setOrganizationName(QStringLiteral(APP_ORGANISATION));
        QCoreApplication::setApplicationName(QStringLiteral(APP_DISPLAY_NAME));

        const QString previousId = QStringLiteral(APP_PREVIOUS_ID);
        QVERIFY2(!previousId.isEmpty() && previousId != QStringLiteral(APP_ID),
                 "APP_PREVIOUS_ID names an old id");

        m_dir = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
                + QStringLiteral("/autostart");
        m_oldPath = m_dir + QLatin1Char('/') + previousId + QStringLiteral(".desktop");
        m_newPath = m_dir + QStringLiteral("/" APP_ID ".desktop");

        // What an older build wrote.
        m_old = "[Desktop Entry]\n"
                "Type=Application\n"
                "Name=Sotto\n"
                "Comment=Voice dictation\n"
                "Exec=/opt/mauvely/bin/MauvelySotto\n"
                "Icon=" + previousId.toUtf8() + "\n"
                "X-KDE-autostart-phase=2\n";
        m_expected = QByteArray(m_old).replace(previousId.toUtf8(), APP_ID);
    }

    // Each test starts with an empty autostart directory.
    void init()
    {
        QDir(m_dir).removeRecursively();
        QVERIFY(QDir().mkpath(m_dir));
    }

    void oldEntryMovesAndAutostartStaysOn()
    {
        write(m_oldPath, m_old);
        Settings settings;
        QVERIFY2(!settings.launchAtLogin(), "before: the old entry is not under this app's name");

        Settings::migrateAutostart(QStringLiteral(APP_PREVIOUS_ID));
        QVERIFY2(settings.launchAtLogin(), "after: it is still on");
        QCOMPARE(read(m_newPath), m_expected);
        QCOMPARE(QDir(m_dir).entryList(QDir::Files), QStringList{QStringLiteral(APP_ID ".desktop")});
    }

    void aSecondStartChangesNothing()
    {
        write(m_oldPath, m_old);
        Settings::migrateAutostart(QStringLiteral(APP_PREVIOUS_ID));
        Settings::migrateAutostart(QStringLiteral(APP_PREVIOUS_ID));
        QCOMPARE(read(m_newPath), m_expected);
        QCOMPARE(QDir(m_dir).entryList(QDir::Files).size(), 1);
    }

    // An older build ran again after a newer one. The new entry wins, and the
    // old one goes, or it would start Sotto twice.
    void withBothTheNewOneStaysAndTheOldOneGoes()
    {
        write(m_newPath, m_expected);
        write(m_oldPath, m_old);
        Settings::migrateAutostart(QStringLiteral(APP_PREVIOUS_ID));
        QVERIFY(!QFile::exists(m_oldPath));
        QCOMPARE(read(m_newPath), m_expected);
    }

    void withNoOldEntryNothingIsCreated()
    {
        Settings settings;
        Settings::migrateAutostart(QStringLiteral(APP_PREVIOUS_ID));
        QVERIFY(!settings.launchAtLogin());
        Settings::migrateAutostart(QString());
        QVERIFY(!settings.launchAtLogin());
        QVERIFY(QDir(m_dir).entryList(QDir::Files).isEmpty());
    }

    // A path that contains the old id is a path, not the id.
    void anExecPathContainingTheOldIdIsLeftAlone()
    {
        const QByteArray exec = "Exec=/home/me/" + QStringLiteral(APP_PREVIOUS_ID).toUtf8() + "/sotto\n";
        write(m_oldPath, "[Desktop Entry]\n" + exec);
        Settings::migrateAutostart(QStringLiteral(APP_PREVIOUS_ID));
        QVERIFY(read(m_newPath).contains(exec));
    }

    // The switch itself writes, and removes, the new name with the new Icon=.
    void theSwitchUsesTheNewName()
    {
        Settings settings;
        settings.setLaunchAtLogin(true);
        QCOMPARE(QDir(m_dir).entryList(QDir::Files), QStringList{QStringLiteral(APP_ID ".desktop")});
        QVERIFY(read(m_newPath).contains("\nIcon=" APP_ID "\n"));
        settings.setLaunchAtLogin(false);
        QVERIFY(QDir(m_dir).entryList(QDir::Files).isEmpty());
    }
};

int main(int argc, char *argv[])
{
    // Before the application exists, so nothing here reads the real config.
    QTemporaryDir config;
    qputenv("XDG_CONFIG_HOME", config.path().toUtf8());
    QGuiApplication app(argc, argv);
    TestAutostartMigration test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_autostart_migration.moc"
