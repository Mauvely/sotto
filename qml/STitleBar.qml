import QtQuick
import QtQuick.Layouts
import Sotto

// Sotto's own title bar, 44px — the same band Compose, Snap, Relay and Suite
// draw, so switching between them does not feel like switching products.
//
// It sits on the window *ground* (`Theme.bg`), not on a panel: the panels below
// it float on that ground with 16px gutters, and a differently-tinted strip at
// the top would read as a fourth surface. The hairline underneath is the design
// system's separator — a drop shadow here reads as a different product.
//
// The drag, the double-click and the resize edges are the window's, not this
// item's: `SottoWindow` answers WM_NCHITTEST with HTCAPTION for every point in
// this bar that is not one of `exclusions`. The MouseArea below is the
// non-Windows path, where Qt's startSystemMove() does the same job.
Item {
    id: bar

    required property SottoWindow window
    property string subtitle: ""

    // Handed to the window so it can leave these alone when hit-testing.
    readonly property var exclusions: [minBtn, maxBtn, closeBtn]
    readonly property var maximizeButton: maxBtn

    implicitHeight: 44

    // Non-Windows only. On Windows this would never see a press anyway (the
    // point is non-client), but an Item that swallows hover would still fight
    // the buttons' own.
    MouseArea {
        anchors.fill: parent
        enabled: !bar.window.platformDrawsFrame
        visible: enabled
        onPressed: bar.window.beginSystemMove()
        onDoubleClicked: bar.window.toggleMaximized()
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 6
        spacing: 8

        LogoMark { size: 22 }

        Text {
            text: "Sotto"
            color: Theme.text
            font.family: Brand.displayFamily
            font.weight: Font.ExtraBold
            font.pixelSize: 16
        }

        Text {
            Layout.fillWidth: true
            Layout.leftMargin: 6
            visible: bar.subtitle.length > 0
            text: bar.subtitle
            color: Theme.textMuted
            font.family: Brand.displayFamily
            font.pixelSize: 12
            elide: Text.ElideRight
        }
        Item { Layout.fillWidth: !bar.subtitle.length }

        LocalBadge { Layout.rightMargin: 6 }

        SWindowButton {
            id: minBtn
            kind: "minimise"
            onClicked: bar.window.minimise()
        }
        SWindowButton {
            id: maxBtn
            kind: bar.window.maximized ? "restore" : "maximise"
            forcedHover: bar.window.maximizeButtonHovered
            onClicked: bar.window.toggleMaximized()
        }
        SWindowButton {
            id: closeBtn
            kind: "close"
            onClicked: bar.window.close()
        }
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: Theme.borderHairline
    }
}
