import QtQuick
import Sotto

// The Sotto mark: a dot emitting two sound arcs. The dot carries the brand's
// teal signal accent (the wordmark's reversed-variant treatment: slate
// ground, teal dot); the arcs stay a soft white so the mark reads at a
// glance against the dark pill.
// The dot breathes gently while listening (unless animations are off).
Item {
    id: mark
    property bool speaking: false
    property bool animated: true
    property real size: 26

    width: size
    height: size

    Rectangle {
        id: dot
        width: mark.size * 0.30
        height: width
        radius: width / 2
        color: Brand.signal
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
        anchors.fill: parent
        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()
            ctx.strokeStyle = Qt.rgba(1, 1, 1, 0.85)
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
