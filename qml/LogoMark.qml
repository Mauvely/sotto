import QtQuick
import Sotto

// The Sotto mark: a dot emitting two sound arcs. The dot carries the brand's
// teal signal accent (the wordmark's reversed-variant treatment: slate ground,
// teal dot); the arcs are drawn in the theme's text ink so the mark reads on a
// light panel as well as on the dark one it was designed against.
// The dot breathes gently while listening (unless animations are off).
Item {
    id: mark
    property bool speaking: false
    property bool animated: true
    property real size: 26
    property color arcColor: Theme.text

    width: size
    height: size

    Rectangle {
        id: dot
        width: mark.size * 0.30
        height: width
        radius: width / 2
        color: Theme.signal
        x: mark.size * 0.06
        y: (mark.height - height) / 2

        SequentialAnimation on scale {
            running: mark.speaking && mark.animated
            loops: Animation.Infinite
            alwaysRunToEnd: true
            NumberAnimation { from: 1.0; to: 1.35; duration: 520; easing.type: Easing.InOutSine }
            NumberAnimation { from: 1.35; to: 1.0; duration: 520; easing.type: Easing.InOutSine }
        }
    }

    Canvas {
        id: arcs
        anchors.fill: parent
        // A Canvas paints once and keeps the pixels. `arcColor` is a binding on
        // the theme, so without this the arcs stay whatever ink was current when
        // the window was first built and vanish on the other theme's panel.
        onArcColorChanged: requestPaint()
        property color arcColor: mark.arcColor
        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()
            ctx.strokeStyle = Qt.rgba(arcColor.r, arcColor.g, arcColor.b, 0.85)
            ctx.lineWidth = mark.size * 0.085
            ctx.lineCap = "round"
            var cx = mark.size * 0.21
            var cy = mark.height / 2
            ctx.beginPath()
            ctx.arc(cx, cy, mark.size * 0.38, -0.65, 0.65)
            ctx.stroke()
            ctx.beginPath()
            ctx.arc(cx, cy, mark.size * 0.62, -0.65, 0.65)
            ctx.stroke()
        }
    }
}
