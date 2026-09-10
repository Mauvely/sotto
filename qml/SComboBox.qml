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
            // Only while it is open, as Qt's own customisation example has it:
            // a ListView bound to the delegate model permanently keeps a set of
            // delegates alive behind a closed popup.
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex
            ScrollIndicator.vertical: ScrollIndicator {}
        }
    }

    // ── Two separate bugs lived in this delegate, and both drew empty rows ──
    //
    // 1. The text came from the shape Qt's ComboBox customisation *example*
    //    uses — `Array.isArray(control.model) ? modelData[textRole]
    //    : model[textRole]`. Every combo in this app whose model is a JS array
    //    of objects (language, behaviour, injection mode, theme) went down the
    //    `modelData[...]` branch and got **undefined**: a multi-role item
    //    exposes its keys as roles, and `modelData` is only the item itself for
    //    a single-value model. The console said so on every row — "Unable to
    //    assign [undefined] to QString" — and the popup drew a list of blank
    //    rows with a working highlight. `control.textAt(index)` is ComboBox's
    //    own C++ resolution of exactly this question and is right for a plain
    //    string list and a role-bearing object alike.
    //
    // 2. `padding: 0`, and it is load-bearing. Basic's ItemDelegate defaults to
    //    `padding: 12`, so a delegate pinned to 30px hands its contentItem an
    //    availableHeight of 30 − 24 = 6. A Text with `elide` set does not
    //    overflow a box too short for a line — it elides the line away and
    //    draws nothing. So even with (1) fixed the rows would still have been
    //    empty. The closed control was never affected: there the same Text gets
    //    the control's full height.
    delegate: ItemDelegate {
        id: item
        width: control.width - 8
        height: 30
        highlighted: control.highlightedIndex === index
        padding: 0

        contentItem: Text {
            text: control.textAt(index)
            font: control.font
            color: Theme.textBody
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
            leftPadding: 10
            rightPadding: 10
        }

        background: Rectangle {
            radius: Brand.radiusXs
            color: item.highlighted ? Theme.primarySoft : "transparent"
        }
    }
}
