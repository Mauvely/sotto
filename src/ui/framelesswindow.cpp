#include "ui/framelesswindow.h"

#include <QGuiApplication>
#include <QPlatformSurfaceEvent>
#include <QQuickItem>
#include <QScreen>

#ifdef Q_OS_WIN
// The window's corners and edge are DWM's — see applyWindowsChrome().
// The frame's *pixels* are ours — see nativeEvent().
#include <qt_windows.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <windowsx.h>
#endif

namespace {

#ifdef Q_OS_WIN
// The Windows 11 attributes, by value: the enums need a Windows 11 SDK, and
// the numbers are what an older SDK's dwmapi.h lacks. Windows 10 answers
// E_INVALIDARG to both and the window stays square and edgeless — accepted.
constexpr DWORD kDwmWindowCornerPreference = 33; // DWMWA_WINDOW_CORNER_PREFERENCE
constexpr DWORD kDwmBorderColor = 34;            // DWMWA_BORDER_COLOR
constexpr int kDwmCornerRound = 2;               // DWMWCP_ROUND

/** What an ordinary top-level is made of, and what Qt's frameless hint takes
 *  away. WS_THICKFRAME is the one that buys Aero Snap and a maximise that stops
 *  at the work area; WS_CAPTION buys the minimise/restore animations and the
 *  taskbar's window thumbnail; the box styles let the shell offer Snap Layouts
 *  at all. None of them is ever *drawn*, because WM_NCCALCSIZE gives the client
 *  area the whole window. */
constexpr LONG_PTR kFrameStyles =
    WS_CAPTION | WS_THICKFRAME | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;

/** How far Windows oversizes a maximised window: it hangs the frame off every
 *  edge of the work area, expecting the frame not to be part of the client.
 *  Ours is, so the same amount has to come back off. */
int zoomedFrameX()
{
    return GetSystemMetrics(SM_CXSIZEFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
}
int zoomedFrameY()
{
    return GetSystemMetrics(SM_CYSIZEFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
}

/**
 * The screen edge holding an auto-hidden taskbar on this window's monitor, or 0.
 *
 * A problem a custom frame has and an ordinary window does not. An ordinary
 * maximised window keeps a non-client frame, so its *client* never reaches the
 * screen edge; ours takes the whole window as client area (WM_NCCALCSIZE), and
 * when the taskbar auto-hides the work area is the whole monitor — so maximising
 * covers the monitor exactly, pixel for pixel. That is the condition the desktop
 * compositor reads as full-screen, and it then hands the window a presentation
 * path meant for games: on a variable-refresh display the refresh rate drops to
 * a flat 60 Hz the moment the window is maximised.
 *
 * The same pixel is what lets an auto-hidden taskbar come back over us — the
 * shell needs an uncovered edge to notice the pointer. The visible cost is a
 * hairline of desktop along that edge while the bar is hidden.
 */
UINT autoHideEdge(HWND hwnd)
{
    APPBARDATA state{};
    state.cbSize = sizeof state;
    if (!(SHAppBarMessage(ABM_GETSTATE, &state) & ABS_AUTOHIDE))
        return 0;

    MONITORINFO mi{};
    mi.cbSize = sizeof mi;
    if (!GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &mi))
        return 0;

    // Per monitor, not per desktop: the bar is on one screen and the other
    // screens must not lose a pixel for it.
    for (const UINT edge : {UINT(ABE_BOTTOM), UINT(ABE_TOP), UINT(ABE_LEFT), UINT(ABE_RIGHT)}) {
        APPBARDATA bar{};
        bar.cbSize = sizeof bar;
        bar.uEdge = edge;
        bar.rc = mi.rcMonitor;
        if (SHAppBarMessage(ABM_GETAUTOHIDEBAREX, &bar))
            return edge;
    }
    return 0;
}
#endif // Q_OS_WIN

/** Whether the window handle is really an HWND.
 *
 *  The offscreen platform hands out a synthetic window id, and passing that to
 *  SetWindowLongPtr/SetWindowPos is a Win32 call on a handle that names no
 *  window. The screenshot harness runs under it on this very platform, so this
 *  is not a theoretical branch. */
bool onNativeWindows()
{
#ifdef Q_OS_WIN
    return QGuiApplication::platformName() == QLatin1String("windows");
#else
    return false;
#endif
}

} // namespace

SottoWindow::SottoWindow(QWindow *parent)
    : QQuickWindow(parent)
{
    // The OS title bar goes; ours is a 44px QML row. On Windows the *styles*
    // come straight back in ensureWindowsFrame() — see the header.
    setFlag(Qt::FramelessWindowHint, true);

    connect(this, &QWindow::windowStateChanged, this, [this] { emit maximizedChanged(); });
}

bool SottoWindow::platformDrawsFrame()
{
#ifdef Q_OS_WIN
    return true;
#else
    return false;
#endif
}

void SottoWindow::setCaptionHeight(int h)
{
    if (m_captionHeight == h)
        return;
    m_captionHeight = h;
    emit captionHeightChanged();
}

void SottoWindow::setCaptionExclusions(const QVariantList &items)
{
    m_exclusions = items;
    emit captionExclusionsChanged();
}

void SottoWindow::setMaximizeButton(QQuickItem *item)
{
    if (m_maxButton == item)
        return;
    m_maxButton = item;
    emit maximizeButtonChanged();
}

void SottoWindow::setBorderColor(const QColor &c)
{
    if (m_borderColor == c)
        return;
    m_borderColor = c;
    emit borderColorChanged();
    applyWindowsChrome();
}

void SottoWindow::setMaxHovered(bool on)
{
    if (m_maxHovered == on)
        return;
    m_maxHovered = on;
    emit maximizeButtonHoveredChanged();
}

bool SottoWindow::isMaximized() const
{
#ifdef Q_OS_WIN
    // IsZoomed, not Qt's window state: SW_MAXIMIZE is what actually maximises
    // this window (see toggleMaximized) and the shell can do it too — a drag to
    // the top edge, Win+Up, a Snap Layouts zone — without Qt being asked first.
    if (onNativeWindows() && m_hwnd)
        return IsZoomed(static_cast<HWND>(m_hwnd)) != FALSE;
#endif
    return windowStates().testFlag(Qt::WindowMaximized)
        || windowStates().testFlag(Qt::WindowFullScreen);
}

void SottoWindow::toggleMaximized()
{
#ifdef Q_OS_WIN
    // Measured in app-base on 2026-09-09: after showMaximized() the window sat
    // at 0,0 2560×1440 with IsZoomed false; after SW_MAXIMIZE it sat at -8,-8
    // 2576×1456 with IsZoomed true — the shape the WM_NCCALCSIZE code expects,
    // and the one a drag to the top edge produces. Qt learns the state from
    // WM_SIZE either way.
    if (onNativeWindows() && m_hwnd) {
        const HWND hwnd = static_cast<HWND>(m_hwnd);
        ShowWindow(hwnd, IsZoomed(hwnd) ? SW_RESTORE : SW_MAXIMIZE);
        emit maximizedChanged();
        return;
    }
#endif
    if (isMaximized())
        showNormal();
    else
        showMaximized();
    emit maximizedChanged();
}

void SottoWindow::beginSystemMove()
{
    if (isMaximized())
        return;
    startSystemMove();
}

void SottoWindow::beginSystemResize(int edges)
{
    if (isMaximized() || edges == 0)
        return;
    startSystemResize(Qt::Edges(edges));
}

void SottoWindow::minimise()
{
    showMinimized();
}

Qt::Edges SottoWindow::edgeAtLocal(const QPointF &p) const
{
    Qt::Edges e;
    const int M = kResizeMargin;
    if (p.x() < 0 || p.y() < 0 || p.x() > width() || p.y() > height())
        return e;
    if (p.x() <= M)
        e |= Qt::LeftEdge;
    else if (p.x() >= width() - M)
        e |= Qt::RightEdge;
    if (p.y() <= M)
        e |= Qt::TopEdge;
    else if (p.y() >= height() - M)
        e |= Qt::BottomEdge;
    return e;
}

bool SottoWindow::inMaximizeButton(const QPointF &p) const
{
    QQuickItem *b = m_maxButton;
    if (!b || !b->isVisible() || b->width() <= 0 || b->height() <= 0)
        return false;
    return b->mapRectToScene(QRectF(0, 0, b->width(), b->height())).contains(p);
}

bool SottoWindow::inCaption(const QPointF &p) const
{
    if (m_captionHeight <= 0 || p.y() >= m_captionHeight)
        return false;
    for (const QVariant &v : m_exclusions) {
        auto *item = v.value<QQuickItem *>();
        if (!item || !item->isVisible() || item->width() <= 0 || item->height() <= 0)
            continue;
        if (item->mapRectToScene(QRectF(0, 0, item->width(), item->height())).contains(p))
            return false;
    }
    return true;
}

bool SottoWindow::event(QEvent *ev)
{
    if (ev->type() == QEvent::PlatformSurface) {
        // The moment the platform window behind this one is created or replaced.
        // Qt rebuilds it on some state changes and both the frame styles and the
        // DWM attributes go with the old one, so both are re-applied here.
        //
        // SurfaceCreated only. onNativeWindowChanged() asks for winId(), and
        // asking for it while the surface is being destroyed makes Qt create a
        // new one on the way out.
        auto *pse = static_cast<QPlatformSurfaceEvent *>(ev);
        if (pse->surfaceEventType() == QPlatformSurfaceEvent::SurfaceCreated)
            onNativeWindowChanged();
        else
            m_hwnd = nullptr;
    }
    return QQuickWindow::event(ev);
}

void SottoWindow::onNativeWindowChanged()
{
#ifdef Q_OS_WIN
    m_hwnd = onNativeWindows() ? reinterpret_cast<void *>(winId()) : nullptr;
#endif
    ensureWindowsFrame();
    applyWindowsChrome();
}

void SottoWindow::ensureWindowsFrame()
{
#ifdef Q_OS_WIN
    if (!m_hwnd)
        return;
    const HWND hwnd = static_cast<HWND>(m_hwnd);
    const LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);
    // WS_POPUP is what Qt leaves behind for a frameless window, and what stops
    // the shell treating this as a window at all.
    const LONG_PTR wanted = (style & ~LONG_PTR(WS_POPUP)) | kFrameStyles;
    if (style == wanted)
        return;
    SetWindowLongPtr(hwnd, GWL_STYLE, wanted);
    // Nothing takes effect until the frame is recalculated, and that is the call
    // that sends the first WM_NCCALCSIZE.
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                 SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE
                     | SWP_NOOWNERZORDER);
#endif
}

void SottoWindow::applyWindowsChrome()
{
#ifdef Q_OS_WIN
    if (!m_hwnd)
        return;
    const HWND hwnd = static_cast<HWND>(m_hwnd);
    const int corner = kDwmCornerRound;
    DwmSetWindowAttribute(hwnd, kDwmWindowCornerPreference, &corner, sizeof corner);
    if (m_borderColor.isValid()) {
        const COLORREF edge = RGB(m_borderColor.red(), m_borderColor.green(), m_borderColor.blue());
        DwmSetWindowAttribute(hwnd, kDwmBorderColor, &edge, sizeof edge);
    }
#endif
}

bool SottoWindow::nativeEvent(const QByteArray &type, void *message, qintptr *result)
{
#ifdef Q_OS_WIN
    if (type != QByteArrayLiteral("windows_generic_MSG"))
        return QQuickWindow::nativeEvent(type, message, result);

    auto *msg = static_cast<MSG *>(message);
    if (!m_hwnd || msg->hwnd != static_cast<HWND>(m_hwnd))
        return QQuickWindow::nativeEvent(type, message, result);

    switch (msg->message) {
    case WM_NCCALCSIZE: {
        // The whole window becomes the client area, so the frame styles exist
        // without a single frame pixel being drawn. wParam FALSE is the
        // rectangle-only form and needs no answer from us.
        if (!msg->wParam)
            break;
        auto *params = reinterpret_cast<NCCALCSIZE_PARAMS *>(msg->lParam);
        RECT &rc = params->rgrc[0];
        if (IsZoomed(msg->hwnd)) {
            // A maximised window is positioned frame-width outside the work area
            // on every side. Left alone, that much of the content would sit
            // off-screen — and over the taskbar.
            const int fx = zoomedFrameX();
            const int fy = zoomedFrameY();
            rc.left += fx;
            rc.right -= fx;
            rc.top += fy;
            rc.bottom -= fy;

            // Leave the auto-hidden taskbar its pixel — see autoHideEdge.
            switch (autoHideEdge(msg->hwnd)) {
            case ABE_LEFT: rc.left += 1; break;
            case ABE_TOP: rc.top += 1; break;
            case ABE_RIGHT: rc.right -= 1; break;
            case ABE_BOTTOM: rc.bottom -= 1; break;
            default: break;
            }
        }
        *result = 0;
        return true;
    }
    case WM_SETTINGCHANGE:
    case WM_DISPLAYCHANGE:
        // The taskbar can be moved, switched to auto-hide, or taken off this
        // monitor while the window sits maximised, and nothing re-asks for the
        // frame on its own.
        if (IsZoomed(msg->hwnd))
            SetWindowPos(msg->hwnd, nullptr, 0, 0, 0, 0,
                         SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER
                             | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
        break;
    case WM_NCHITTEST: {
        // Screen pixels → client pixels → this window's own coordinates. The
        // client rect is the window's rect (WM_NCCALCSIZE above), so the only
        // conversion left is the device pixel ratio.
        POINT pt{GET_X_LPARAM(msg->lParam), GET_Y_LPARAM(msg->lParam)};
        ScreenToClient(msg->hwnd, &pt);
        const qreal dpr = devicePixelRatio() > 0 ? devicePixelRatio() : 1.0;
        const QPointF local(pt.x / dpr, pt.y / dpr);

        if (!isMaximized()) {
            const Qt::Edges e = edgeAtLocal(local);
            const bool l = e.testFlag(Qt::LeftEdge), r = e.testFlag(Qt::RightEdge);
            const bool t = e.testFlag(Qt::TopEdge), b = e.testFlag(Qt::BottomEdge);
            int ht = 0;
            if (l && t) ht = HTTOPLEFT;
            else if (r && t) ht = HTTOPRIGHT;
            else if (l && b) ht = HTBOTTOMLEFT;
            else if (r && b) ht = HTBOTTOMRIGHT;
            else if (l) ht = HTLEFT;
            else if (r) ht = HTRIGHT;
            else if (t) ht = HTTOP;
            else if (b) ht = HTBOTTOM;
            if (ht) {
                *result = ht;
                return true;
            }
        }

        // Claimed even while maximised: Snap Layouts offers to un-maximise into
        // a zone, which is exactly what the button is for.
        if (inMaximizeButton(local)) {
            *result = HTMAXBUTTON;
            return true;
        }
        if (inCaption(local)) {
            *result = HTCAPTION;
            return true;
        }
        *result = HTCLIENT;
        return true;
    }
    case WM_NCMOUSEMOVE:
        if (msg->wParam == HTMAXBUTTON) {
            if (!m_maxHovered) {
                setMaxHovered(true);
                // The only way a non-client hover ever ends: ask to be told.
                TRACKMOUSEEVENT tme{sizeof(TRACKMOUSEEVENT), TME_LEAVE | TME_NONCLIENT, msg->hwnd,
                                    0};
                TrackMouseEvent(&tme);
            }
            *result = 0;
            return true;
        }
        break;
    case WM_NCMOUSELEAVE:
        setMaxHovered(false);
        break;
    case WM_NCLBUTTONDOWN:
        // Swallowed so DefWindowProc does not run its own caption-button
        // tracking against a caption that is not there.
        if (msg->wParam == HTMAXBUTTON) {
            *result = 0;
            return true;
        }
        break;
    case WM_NCLBUTTONUP:
        if (msg->wParam == HTMAXBUTTON) {
            setMaxHovered(false);
            toggleMaximized();
            *result = 0;
            return true;
        }
        break;
    case WM_SIZE:
        // Aero Snap, Win+Up and Snap Layouts all maximise without going through
        // toggleMaximized(), and IsZoomed() is what `maximized` reads.
        emit maximizedChanged();
        break;
    default:
        break;
    }
#endif
    return QQuickWindow::nativeEvent(type, message, result);
}
