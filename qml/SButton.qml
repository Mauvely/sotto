import QtQuick
import QtQuick.Controls.Basic
import Sotto

// Brand button: filled primary by default, with secondary/ghost/soft variants
// for the settings window's action hierarchy (Download vs. Cancel/Remove/
// Refresh).
//
// Hover goes one step darker on the filled surface and one step tinted on a
// quiet one — never opacity-only, which the design system rules out — and takes
// `durFast`, so it stops moving the moment Animations is off.
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
        border.color: control.hovered ? Theme.textMuted : Theme.borderStrong
        color: {
            switch (control.variant) {
            case "secondary":
            case "ghost":
                return control.pressed ? Theme.primarySoftHover
                                       : (control.hovered ? Theme.primarySoft : "transparent")
            case "soft":
                return control.pressed ? Theme.primarySoftHover
                                       : (control.hovered ? Theme.primarySoftHover : Theme.primarySoft)
            default:
                return control.pressed ? Theme.primaryActive
                                       : (control.hovered ? Theme.primaryHover : Theme.primary)
            }
        }
        Behavior on color {
            ColorAnimation { duration: Theme.durFast; easing.type: Easing.Bezier
                             easing.bezierCurve: Theme.easeOut }
        }
        Behavior on border.color {
            ColorAnimation { duration: Theme.durFast }
        }
    }

    contentItem: Text {
        text: control.text
        font: control.font
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        color: control.variant === "primary" ? Theme.primaryInk : Theme.primaryText
    }
}
