import QtQuick
import QtQuick.Controls.Basic
import Sotto

// Brand text input: sunken, with a real 1px border so it reads as inset on a
// panel, and the focus colour on the border rather than a separate ring (a
// QML ring would need a second item outside the control's own bounds).
TextField {
    id: control

    implicitHeight: 34
    leftPadding: 12
    rightPadding: 12
    font.family: Brand.bodyFamily
    font.pixelSize: 13
    color: Theme.textBody
    placeholderTextColor: Theme.textQuiet
    selectionColor: Theme.primarySoftHover
    selectedTextColor: Theme.text
    opacity: enabled ? 1.0 : 0.45

    background: Rectangle {
        radius: Brand.radiusMd
        color: Theme.surfaceSunken
        border.width: 1
        border.color: control.activeFocus ? Theme.primary
                                          : (control.hovered ? Theme.textMuted : Theme.borderStrong)
        Behavior on border.color {
            ColorAnimation { duration: Theme.durFast }
        }
    }
}
