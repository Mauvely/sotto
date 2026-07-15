#pragma once

#include <QObject>
#include <QPointer>
#include <QQuickWindow>

class QQmlEngine;
class Settings;

// Owns the HUD window (qml/Overlay.qml). On Wayland with LayerShellQt the
// window is a layer-shell surface on the overlay layer: bottom-anchored,
// never focusable, never tiled, and — with the "auto" screen setting —
// placed by the compositor on the currently active monitor only (KWin and
// Hyprland both resolve a null output to the active one). Without
// LayerShellQt it degrades to a frameless always-on-top window positioned
// on the screen under the cursor.
class OverlayController : public QObject
{
    Q_OBJECT
public:
    OverlayController(QQmlEngine *engine, Settings *settings, QObject *parent = nullptr);

    bool initialize();
    QQuickWindow *window() const { return m_window; }

    // Tear the window down while the QML engine is still alive, so
    // bindings don't fire against dead context properties on shutdown.
    void destroyWindow() { delete m_window.data(); }

public slots:
    void applyScreenSetting();

private:
    void configureLayerShell();
    void positionFallback();
    void applyBlurBehind();

    QQmlEngine *m_engine;
    Settings *m_settings;
    QPointer<QQuickWindow> m_window;
    bool m_usingLayerShell = false;
};
