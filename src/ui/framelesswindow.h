#pragma once

// ─────────────────────────────────────────────────────────────────────────────
//  SottoWindow — Sotto's own QWindow-based port of the suite's frameless shell.
//
//  **This is not the pinned copy.** `app-base/src/ui/framelesswindow.{h,cpp}`
//  is a QWidget and is byte-identical across app-base, suite, snap and relay;
//  nothing here may be copied back there and nothing there can be copied here.
//  Sotto's UI is QML, so the shell is a QQuickWindow subclass and the pieces the
//  QWidget version does with layouts and child widgets — the title bar, the
//  window buttons, the resize cursors — live in `qml/STitleBar.qml` and
//  `qml/SWindowButton.qml` instead. What *is* ported, line for line and for the
//  same reasons, is the Windows half: kFrameStyles, ensureWindowsFrame(),
//  WM_NCCALCSIZE / WM_NCHITTEST / HTMAXBUTTON in nativeEvent(), autoHideEdge()
//  and a toggleMaximized() that goes through ShowWindow(SW_MAXIMIZE/SW_RESTORE)
//  rather than Qt (app-base commit f421c0a).
//
//  Why any of it: Qt::FramelessWindowHint leaves a WS_POPUP behind, and Windows
//  does not treat a popup as a window — no Snap Layouts under the maximise
//  button, no drag-to-edge tiling, no Win+Arrow, and SW_MAXIMIZE sizes it to the
//  whole monitor instead of the work area. So the native window keeps
//  WS_OVERLAPPEDWINDOW and WM_NCCALCSIZE takes the frame's *pixels* away
//  instead. WM_NCHITTEST then hands the caption band, the resize edges and the
//  maximise button back to Windows by name.
//
//  QML tells this class where those regions are, because only QML knows:
//
//      SottoWindow {
//          captionHeight: 44
//          maximizeButton: maxBtn          // hit-tested as HTMAXBUTTON
//          captionExclusions: [minBtn, maxBtn, closeBtn]
//          borderColor: Theme.borderStrong // DWMWA_BORDER_COLOR
//      }
//
//  Everything outside Windows falls back to Qt's own startSystemMove() /
//  startSystemResize(), which is what app-base does on Wayland and X11.
// ─────────────────────────────────────────────────────────────────────────────

#include <QColor>
#include <QPointer>
// The whole type, not a forward declaration: `maximizeButton` is a
// Q_PROPERTY of type QQuickItem*, and Qt's meta-type system static_asserts
// that a pointer property points at a fully-defined type.
#include <QQuickItem>
#include <QQuickWindow>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

class SottoWindow : public QQuickWindow
{
    Q_OBJECT
    QML_ELEMENT

    /** Height of the QML title bar, in logical pixels. Every point above it
     *  that is not in `captionExclusions` answers HTCAPTION, which is what
     *  makes the bar drag the window, double-click maximise it and right-click
     *  open the system menu — all of it Windows' own, not ours. */
    Q_PROPERTY(int captionHeight READ captionHeight WRITE setCaptionHeight NOTIFY captionHeightChanged)

    /** Items inside the caption band that must keep their mouse events: the
     *  window buttons, and anything else clickable a fork puts in the bar. */
    Q_PROPERTY(QVariantList captionExclusions READ captionExclusions WRITE setCaptionExclusions NOTIFY captionExclusionsChanged)

    /** The maximise button, named to Windows as HTMAXBUTTON so Windows 11 opens
     *  the Snap Layouts flyout when the pointer rests on it. */
    Q_PROPERTY(QQuickItem *maximizeButton READ maximizeButton WRITE setMaximizeButton NOTIFY maximizeButtonChanged)

    /** True while the pointer is over the maximise button. HTMAXBUTTON takes
     *  the button's mouse events away from Qt, so without this the button would
     *  look dead under the pointer. */
    Q_PROPERTY(bool maximizeButtonHovered READ maximizeButtonHovered NOTIFY maximizeButtonHoveredChanged)

    Q_PROPERTY(bool maximized READ isMaximized NOTIFY maximizedChanged)

    /** The window's edge colour. On Windows it is handed to DWM, which draws
     *  the one-pixel border and the rounded corners itself; elsewhere QML paints
     *  it. Bound to `Theme.borderStrong`, so it follows the theme. */
    Q_PROPERTY(QColor borderColor READ borderColor WRITE setBorderColor NOTIFY borderColorChanged)

    /** True when the platform draws the window's own edge and corners (Windows
     *  + DWM). QML paints its own hairline and rounds its own corners when this
     *  is false, and installs the edge-resize handles Qt needs there. */
    Q_PROPERTY(bool platformDrawsFrame READ platformDrawsFrame CONSTANT)

public:
    explicit SottoWindow(QWindow *parent = nullptr);

    int captionHeight() const { return m_captionHeight; }
    void setCaptionHeight(int h);
    QVariantList captionExclusions() const { return m_exclusions; }
    void setCaptionExclusions(const QVariantList &items);
    QQuickItem *maximizeButton() const { return m_maxButton; }
    void setMaximizeButton(QQuickItem *item);
    bool maximizeButtonHovered() const { return m_maxHovered; }
    QColor borderColor() const { return m_borderColor; }
    void setBorderColor(const QColor &c);

    bool isMaximized() const;
    static bool platformDrawsFrame();

public slots:
    /** Through Windows, not through Qt. Qt "maximises" a frameless window by
     *  sizing it to the screen and never telling the shell: WS_MAXIMIZE stays
     *  off, DWM keeps rounding the corners at the screen's edges, the
     *  WM_NCCALCSIZE insets never run, an auto-hidden taskbar has no pixel to
     *  rise from, and Snap Layouts sees nothing to restore. */
    void toggleMaximized();
    void beginSystemMove();
    void beginSystemResize(int edges); // Qt::Edges, as an int for QML
    void minimise();

signals:
    void captionHeightChanged();
    void captionExclusionsChanged();
    void maximizeButtonChanged();
    void maximizeButtonHoveredChanged();
    void maximizedChanged();
    void borderColorChanged();

protected:
    bool event(QEvent *ev) override;
    bool nativeEvent(const QByteArray &type, void *message, qintptr *result) override;

private:
    void onNativeWindowChanged();
    void applyWindowsChrome();
    /** Put the real frame styles back on the native window so the shell treats
     *  it as an ordinary top-level. Checks before it writes: the
     *  SWP_FRAMECHANGED it needs re-runs WM_NCCALCSIZE, and doing that on every
     *  theme change would be a relayout for nothing. */
    void ensureWindowsFrame();
    void setMaxHovered(bool on);
    /** `p` in this window's logical coordinates — the same space QML items map
     *  into with mapRectToScene(). */
    bool inCaption(const QPointF &p) const;
    bool inMaximizeButton(const QPointF &p) const;
    Qt::Edges edgeAtLocal(const QPointF &p) const;

    int m_captionHeight = 0;
    QVariantList m_exclusions;
    QPointer<QQuickItem> m_maxButton;
    bool m_maxHovered = false;
    QColor m_borderColor;
    /** The HWND, as void* so this header never has to pull in windows.h — every
     *  QML-facing header in the app is included by the moc/qmltyperegistrar
     *  build too. Null on every other platform and under the offscreen plugin,
     *  which hands out a synthetic window id that names no window. */
    void *m_hwnd = nullptr;

    static constexpr int kResizeMargin = 6;
};
