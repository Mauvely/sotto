#pragma once

#include <QObject>

class App;
class QAction;
class QSystemTrayIcon;

// Status-notifier tray entry: quick access to dictation, notepad, settings.
class TrayIcon : public QObject
{
    Q_OBJECT
public:
    explicit TrayIcon(App *app, QObject *parent = nullptr);

public slots:
    void updateState();

private:
    App *m_app;
    QSystemTrayIcon *m_tray;
    QAction *m_toggleAction;
};
