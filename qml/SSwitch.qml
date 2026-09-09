import QtQuick
import QtQuick.Controls.Basic
import Sotto

// Brand switch: a pill track that fills with primary when on, and a knob that
// slides. Both the slide and the fill are `durFast` — hover/press colour speed,
// because a toggle is a control, not a reveal.
Switch {
    id: control

    font.family: Brand.bodyFamily
    font.pixelSize: 13
    spacing: 10
    padding: 0
    opacity: enabled ? 1.0 : 0.45

    indicator: Rectangle {
        implicitWidth: 38
        implicitHeight: 22
        x: control.leftPadding
        y: control.topPadding + (control.availableHeight - height) / 2
        radius: height / 2
        color: control.checked ? Theme.primary : Theme.surfaceSunken
        border.width: control.checked ? 0 : 1
        border.color: control.hovered ? Theme.primary : Theme.borderStrong

        Behavior on color {
            ColorAnimation { duration: Theme.durFast; easing.type: Easing.Bezier
                             easing.bezierCurve: Theme.easeOut }
        }

        Rectangle {
            id: knob
            width: 16
            height: 16
            radius: width / 2
            y: (parent.height - height) / 2
            x: control.checked ? parent.width - width - 3 : 3
            color: control.checked ? Theme.primaryInk : Theme.textMuted

            Behavior on x {
                NumberAnimation { duration: Theme.durFast; easing.type: Easing.Bezier
                                  easing.bezierCurve: Theme.easeOut }
            }
            Behavior on color {
                ColorAnimation { duration: Theme.durFast }
            }
        }
    }

    contentItem: Text {
        leftPadding: control.indicator.width + control.spacing
        text: control.text
        font: control.font
        color: Theme.textBody
        wrapMode: Text.WordWrap
        verticalAlignment: Text.AlignVCenter
    }
}
