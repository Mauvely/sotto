import QtQuick
import Sotto

// A window control: minimise, maximise, restore, close.
//
// 40×30 rather than the 28-ish the rest of the chrome uses, and the same size
// the sibling apps' `ui::TitleBar` gives them — these are the targets people hit
// without looking, and Fitts' law does not care that they are visually quiet.
//
// The glyph is stroked on a Canvas with Lucide's 1.5px round-cap geometry, for
// the same reason the checkbox tick and the combo chevron are: the design system
// rules out typing an icon as a character ("—", "▢", "✕").
//
// `forcedHover` exists because of Windows. Once WM_NCHITTEST names the maximise
// button HTMAXBUTTON — which is the only way Windows 11 knows where to open the
// Snap Layouts flyout — Windows keeps that button's mouse events and Qt never
// sees a hover. `SottoWindow.maximizeButtonHovered` is fed back in here so the
// button does not look dead under the pointer.
Item {
    id: root

    property string kind: "minimise" // minimise | maximise | restore | close
    property bool forcedHover: false
    signal clicked()

    readonly property bool active: hover.hovered || forcedHover
    readonly property bool danger: kind === "close"

    implicitWidth: 40
    implicitHeight: 30

    Rectangle {
        anchors.fill: parent
        radius: Brand.radiusXs
        // The close button is the only control in the suite that turns red on
        // hover. It is also the only one whose misfire costs you something.
        color: !root.active ? "transparent"
                            : (root.danger ? Theme.danger
                                           : (tap.pressed ? Theme.primarySoftHover
                                                          : Theme.primarySoft))
        Behavior on color {
            ColorAnimation { duration: Theme.durFast }
        }
    }

    Canvas {
        id: glyph
        anchors.centerIn: parent
        width: 11
        height: 11
        property color stroke: root.active
                               ? (root.danger ? "#ffffff" : Theme.text)
                               : Theme.textMuted
        property string shape: root.kind
        onStrokeChanged: requestPaint()
        onShapeChanged: requestPaint()
        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()
            ctx.strokeStyle = stroke
            ctx.lineWidth = 1.5
            ctx.lineCap = "round"
            ctx.lineJoin = "round"
            var w = width, h = height
            ctx.beginPath()
            if (shape === "minimise") {
                ctx.moveTo(0.5, h / 2)
                ctx.lineTo(w - 0.5, h / 2)
            } else if (shape === "maximise") {
                ctx.rect(0.75, 0.75, w - 1.5, h - 1.5)
            } else if (shape === "restore") {
                // Two overlapping squares — the same glyph swap every desktop
                // uses, so it needs no label.
                ctx.rect(0.75, h * 0.28, w * 0.72 - 0.75, h * 0.72 - 0.75)
                ctx.moveTo(w * 0.28, h * 0.28)
                ctx.lineTo(w * 0.28, 0.75)
                ctx.lineTo(w - 0.75, 0.75)
                ctx.lineTo(w - 0.75, h * 0.72)
                ctx.lineTo(w * 0.72, h * 0.72)
            } else {
                ctx.moveTo(0.75, 0.75)
                ctx.lineTo(w - 0.75, h - 0.75)
                ctx.moveTo(w - 0.75, 0.75)
                ctx.lineTo(0.75, h - 0.75)
            }
            ctx.stroke()
        }
    }

    HoverHandler { id: hover }
    TapHandler {
        id: tap
        onTapped: root.clicked()
    }
}
