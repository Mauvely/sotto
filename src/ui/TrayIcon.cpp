#include "ui/TrayIcon.h"

#include "core/App.h"

#include <QIcon>
#include <QMenu>
#include <QSystemTrayIcon>

TrayIcon::TrayIcon(App *app, QObject *parent)
    : QObject(parent)
    , m_app(app)
    , m_tray(new QSystemTrayIcon(this))
{
    auto *menu = new QMenu;

    m_toggleAction = menu->addAction(tr("Start dictation"), m_app, &App::toggle);
    menu->addAction(tr("Notepad"), m_app, &App::showNotepad);
    menu->addAction(tr("Settings…"), m_app, &App::showSettings);
    menu->addSeparator();
    auto *local = menu->addAction(tr("100% local — nothing leaves this device"));
    local->setEnabled(false);
    menu->addSeparator();
    menu->addAction(tr("Quit"), m_app, &App::quit);

    m_tray->setContextMenu(menu);
    m_tray->setIcon(QIcon(QStringLiteral(":/icons/sotto.svg")));
    m_tray->setToolTip(QStringLiteral("Sotto — local dictation"));
    m_tray->show();

    connect(m_tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason r) {
        if (r == QSystemTrayIcon::Trigger)
            m_app->showSettings();
    });
    connect(m_app, &App::stateChanged, this, &TrayIcon::updateState);
}

void TrayIcon::updateState()
{
    const bool busy = m_app->stateName() != QStringLiteral("idle");
    m_toggleAction->setText(busy ? tr("Stop dictation") : tr("Start dictation"));
}
