import QtQuick
import QtQuick.Controls.Basic
import Sotto

// Brand select. Reads as inset — surfaceSunken with a real 1px border — which
// is the design system's rule for anything you can type into or choose from,
// as against a panel, which has no border at all.
ComboBox {
    id: control

    implicitHeight: 34
    font.family: Brand.bodyFamily
    font.pixelSize: 13
    opacity: enabled ? 1.0 : 0.45
    leftPadding: 12
    rightPadding: 30

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

    contentItem: Text {
        text: control.displayText
        font: control.font
        color: Theme.textBody
        elide: Text.ElideRight
        verticalAlignment: Text.AlignVCenter
    }

    // A chevron, stroked with Lucide's geometry rather than typed as "▾".
    indicator: Canvas {
        x: control.width - width - 11
        y: (control.height - height) / 2
        width: 12
        height: 12
        property color stroke: Theme.textMuted
        onStrokeChanged: requestPaint()
        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()
            ctx.strokeStyle = stroke
            ctx.lineWidth = 1.75
            ctx.lineCap = "round"
            ctx.lineJoin = "round"
            ctx.beginPath()
            ctx.moveTo(width * 0.2, height * 0.38)
            ctx.lineTo(width * 0.5, height * 0.68)
            ctx.lineTo(width * 0.8, height * 0.38)
            ctx.stroke()
        }
    }

    popup: Popup {
        y: control.height + 4
        width: control.width
        implicitHeight: Math.min(contentItem.implicitHeight + 8, 260)
        padding: 4

        background: Rectangle {
            radius: Brand.radiusMd
            color: Theme.surfaceRaised
            border.width: 1
            border.color: Theme.borderSubtle
        }

        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.delegateModel
            currentIndex: control.highlightedIndex
            ScrollIndicator.vertical: ScrollIndicator {}
        }
    }

    // Verbatim from Qt's own ComboBox customisation example, which is the only
    // shape that works for both a plain string list and a list of objects with
    // a textRole.
    delegate: ItemDelegate {
        id: item
        width: control.width - 8
        height: 30
        highlighted: control.highlightedIndex === index

        contentItem: Text {
            text: control.textRole
                  ? (Array.isArray(control.model)
                     ? modelData[control.textRole]
                     : model[control.textRole])
                  : modelData
            font: control.font
            color: Theme.textBody
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
            leftPadding: 8
        }

        background: Rectangle {
            radius: Brand.radiusXs
            color: item.highlighted ? Theme.primarySoft : "transparent"
        }
    }
}
