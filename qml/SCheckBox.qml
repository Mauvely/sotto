import QtQuick
import QtQuick.Controls.Basic
import Sotto

// Brand checkbox. The tick is stroked rather than set as a "✓" character: the
// design system forbids Unicode standing in for an icon, and a 2px round-capped
// stroke is the Lucide geometry the rest of the suite draws.
CheckBox {
    id: control

    font.family: Brand.bodyFamily
    font.pixelSize: 13
    spacing: 10
    padding: 0
    opacity: enabled ? 1.0 : 0.45

    indicator: Rectangle {
        id: box
        implicitWidth: 18
        implicitHeight: 18
        x: control.leftPadding
        y: control.topPadding + (control.availableHeight - height) / 2
        radius: Brand.radiusXs
        color: control.checked ? Theme.primary : "transparent"
        border.width: control.checked ? 0 : 1
        border.color: control.hovered ? Theme.primary : Theme.borderStrong

        Behavior on color {
            ColorAnimation { duration: Theme.durFast; easing.type: Easing.Bezier
                             easing.bezierCurve: Theme.easeOut }
        }

        Canvas {
            anchors.fill: parent
            visible: control.checked
            // The stroke colour is a binding, and Canvas does not repaint on
            // its own when one changes — so ask, or the tick keeps the ink of
            // whichever theme was on when it was first painted.
            property color stroke: Theme.primaryInk
            onStrokeChanged: requestPaint()
            onPaint: {
                var ctx = getContext("2d")
                ctx.reset()
                ctx.strokeStyle = stroke
                ctx.lineWidth = 2
                ctx.lineCap = "round"
                ctx.lineJoin = "round"
                ctx.beginPath()
                ctx.moveTo(width * 0.26, height * 0.52)
                ctx.lineTo(width * 0.44, height * 0.70)
                ctx.lineTo(width * 0.75, height * 0.31)
                ctx.stroke()
            }
        }
    }

    contentItem: Text {
        leftPadding: control.indicator.width + control.spacing
        text: control.text
        font: control.font
        color: Theme.textBody
        wrapMode: Text.NoWrap
        verticalAlignment: Text.AlignVCenter
    }
}
