import QtQuick
import QtQuick.Controls.Basic
import Sotto

// Brand slider: a sunken track, a violet fill, a round handle that takes the
// primary-soft ring on hover rather than growing (no scale changes in chrome).
Slider {
    id: control

    implicitHeight: 24
    opacity: enabled ? 1.0 : 0.45

    background: Rectangle {
        x: control.leftPadding
        y: control.topPadding + control.availableHeight / 2 - height / 2
        width: control.availableWidth
        height: 6
        radius: 3
        color: Theme.surfaceSunken
        border.width: 1
        border.color: Theme.borderStrong

        Rectangle {
            width: control.visualPosition * parent.width
            height: parent.height
            radius: parent.radius
            color: Theme.primary
        }
    }

    handle: Rectangle {
        x: control.leftPadding + control.visualPosition * (control.availableWidth - width)
        y: control.topPadding + control.availableHeight / 2 - height / 2
        implicitWidth: 16
        implicitHeight: 16
        radius: width / 2
        color: control.pressed ? Theme.primaryHover : Theme.primary
        border.width: control.hovered ? 3 : 0
        border.color: Theme.primarySoft

        Behavior on color {
            ColorAnimation { duration: Theme.durFast }
        }
    }
}
