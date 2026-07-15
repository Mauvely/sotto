#include "ui/OverlayController.h"

#include "core/Settings.h"

#include <QCursor>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QScreen>

#ifdef SOTTO_HAVE_LAYER_SHELL
#include <LayerShellQt/window.h>
#endif

#ifdef SOTTO_HAVE_KWINDOWSYSTEM
#include <KWindowEffects>
#endif

namespace {
constexpr int kBottomMargin = 32;

QScreen *screenByName(const QString &name)
{
    const auto screens = QGuiApplication::screens();
    for (QScreen *s : screens) {
        if (s->name() == name)
            return s;
    }
    return nullptr;
}
} // namespace

OverlayController::OverlayController(QQmlEngine *engine, Settings *settings, QObject *parent)
    : QObject(parent)
    , m_engine(engine)
    , m_settings(settings)
{
}

bool OverlayController::initialize()
{
    QQmlComponent component(m_engine, QUrl(QStringLiteral("qrc:/qt/qml/Sotto/qml/Overlay.qml")));
    QObject *obj = component.create();
    if (!obj) {
        qWarning() << "Overlay.qml failed to load:" << component.errorString();
        return false;
    }
    m_window = qobject_cast<QQuickWindow *>(obj);
    if (!m_window) {
        qWarning() << "Overlay.qml root item is not a Window";
        delete obj;
        return false;
    }

    const bool wayland =
        QGuiApplication::platformName().startsWith(QStringLiteral("wayland"), Qt::CaseInsensitive);

#ifdef SOTTO_HAVE_LAYER_SHELL
    if (wayland) {
        configureLayerShell();
        m_usingLayerShell = true;
    }
#endif

    if (!m_usingLayerShell) {
        m_window->setFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool
                           | Qt::WindowDoesNotAcceptFocus);
        connect(m_window, &QWindow::visibleChanged, this, [this](bool visible) {
            if (visible)
                positionFallback();
        });
    }

    connect(m_settings, &Settings::overlayScreenChanged, this, &OverlayController::applyScreenSetting);
    applyScreenSetting();

    // Hiding destroys the wl surface, so the blur region has to be
    // re-requested on every show.
    connect(m_window, &QWindow::visibleChanged, this, [this](bool visible) {
        if (visible)
            applyBlurBehind();
    });
    connect(m_settings, &Settings::overlayTranslucentChanged, this,
            &OverlayController::applyBlurBehind);
    return true;
}

void OverlayController::applyBlurBehind()
{
#ifdef SOTTO_HAVE_KWINDOWSYSTEM
    if (!m_window)
        return;
    // Blur only the pill's capsule shape; a full-window region would show
    // blurred rectangles poking out of the rounded corners.
    QRegion region;
    if (m_settings->overlayTranslucent()) {
        const int w = m_window->width();
        const int h = m_window->height();
        const int r = h / 2;
        region = QRegion(r, 0, w - 2 * r, h);
        region += QRegion(0, 0, 2 * r, h, QRegion::Ellipse);
        region += QRegion(w - 2 * r, 0, 2 * r, h, QRegion::Ellipse);
    }
    KWindowEffects::enableBlurBehind(m_window, m_settings->overlayTranslucent(), region);
#endif
}

void OverlayController::configureLayerShell()
{
#ifdef SOTTO_HAVE_LAYER_SHELL
    // Must run before the window is first shown so the surface is created
    // as a layer surface.
    auto *ls = LayerShellQt::Window::get(m_window);
    ls->setScope(QStringLiteral("sotto-hud"));
    ls->setLayer(LayerShellQt::Window::LayerOverlay);
    ls->setAnchors(LayerShellQt::Window::Anchors(LayerShellQt::Window::AnchorBottom));
    ls->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);
    ls->setExclusiveZone(0); // float above content, don't reserve space
    ls->setMargins(QMargins(0, 0, 0, kBottomMargin));
#endif
}

void OverlayController::applyScreenSetting()
{
    if (!m_window)
        return;
    const QString wanted = m_settings->overlayScreen();

#ifdef SOTTO_HAVE_LAYER_SHELL
    if (m_usingLayerShell) {
        auto *ls = LayerShellQt::Window::get(m_window);
        if (wanted == QStringLiteral("auto")) {
            // No fixed screen: KWin and Hyprland place the surface on the
            // active monitor. Re-evaluated on every show since hiding
            // destroys the wl surface.
            ls->setWantsToBeOnActiveScreen(true);
        } else if (QScreen *s = screenByName(wanted)) {
            ls->setWantsToBeOnActiveScreen(false);
            m_window->setScreen(s);
        }
        return;
    }
#endif

    if (m_window->isVisible())
        positionFallback();
}

void OverlayController::positionFallback()
{
    const QString wanted = m_settings->overlayScreen();
    QScreen *screen = wanted == QStringLiteral("auto") ? nullptr : screenByName(wanted);
    if (!screen)
        screen = QGuiApplication::screenAt(QCursor::pos());
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    if (!screen)
        return;

    const QRect geo = screen->availableGeometry();
    m_window->setPosition(geo.center().x() - m_window->width() / 2,
                          geo.bottom() - m_window->height() - kBottomMargin);
}
