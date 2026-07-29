import QtQuick
import QtQuick.Controls.Basic
import Sotto

// Brand button: filled primary by default, with secondary/ghost/soft variants
// for the settings window's action hierarchy (Download vs. Cancel/Remove/
// Refresh). Adapted for the dark (slate 950) ground the app windows sit on.
Button {
    id: control

    property string variant: "primary" // primary | secondary | ghost | soft
    property string size: "md"         // sm | md | lg

    readonly property int _h: size === "sm" ? 32 : size === "lg" ? 48 : 40
    readonly property int _fs: size === "sm" ? 13 : size === "lg" ? 16 : 15
    readonly property int _pad: size === "sm" ? 14 : size === "lg" ? 24 : 18
    readonly property int _radius: size === "sm" ? Brand.radiusSm
                                    : size === "lg" ? Brand.radiusLg : Brand.radiusMd

    implicitHeight: _h
    leftPadding: _pad
    rightPadding: _pad
    font.family: Brand.bodyFamily
    font.pixelSize: _fs
    font.weight: Font.DemiBold
    opacity: enabled ? 1.0 : 0.45

    background: Rectangle {
        radius: control._radius
        border.width: control.variant === "secondary" ? 1 : 0
        border.color: Qt.rgba(1, 1, 1, 0.18)
        color: {
            switch (control.variant) {
            case "secondary": return control.pressed ? Qt.rgba(1, 1, 1, 0.10)
                                                       : (control.hovered ? Qt.rgba(1, 1, 1, 0.06) : "transparent")
            case "ghost":     return control.pressed ? Qt.rgba(1, 1, 1, 0.12)
                                                       : (control.hovered ? Qt.rgba(1, 1, 1, 0.07) : "transparent")
            case "soft":      return control.pressed ? Qt.rgba(Brand.violet500.r, Brand.violet500.g, Brand.violet500.b, 0.32)
                                                       : (control.hovered ? Qt.rgba(Brand.violet500.r, Brand.violet500.g, Brand.violet500.b, 0.24)
                                                                           : Qt.rgba(Brand.violet500.r, Brand.violet500.g, Brand.violet500.b, 0.16))
            default:          return control.pressed ? Brand.violet700 : (control.hovered ? Brand.primaryHover : Brand.primary)
            }
        }
        Behavior on color { ColorAnimation { duration: 100 } }
    }

    contentItem: Text {
        text: control.text
        font: control.font
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        color: control.variant === "primary" ? "#ffffff" : Brand.violet300
    }
}
