import QtQuick
import QtQuick.Controls.Basic
import Sotto

// Brand radio. The Basic style's own indicator is a flat mid-grey disc that
// belongs to neither theme; this is the only reason these S* control wrappers
// exist at all.
RadioButton {
    id: control

    font.family: Brand.bodyFamily
    font.pixelSize: 13
    spacing: 10
    padding: 0
    opacity: enabled ? 1.0 : 0.45

    indicator: Rectangle {
        implicitWidth: 18
        implicitHeight: 18
        x: control.leftPadding
        y: control.topPadding + (control.availableHeight - height) / 2
        radius: width / 2
        color: control.checked ? Theme.primary : "transparent"
        border.width: control.checked ? 0 : 1
        border.color: control.hovered ? Theme.primary : Theme.borderStrong

        Behavior on color {
            ColorAnimation { duration: Theme.durFast; easing.type: Easing.Bezier
                             easing.bezierCurve: Theme.easeOut }
        }

        Rectangle {
            anchors.centerIn: parent
            width: 7
            height: 7
            radius: width / 2
            color: Theme.primaryInk
            visible: control.checked
        }
    }

    contentItem: Text {
        leftPadding: control.indicator.width + control.spacing
        text: control.text
        font: control.font
        color: Theme.textBody
        verticalAlignment: Text.AlignVCenter
    }
}
