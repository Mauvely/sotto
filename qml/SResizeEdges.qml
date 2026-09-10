import QtQuick
import Sotto

// Edge and corner resize handles for the platforms where the window manager is
// not doing it for us.
//
// On Windows it is: `SottoWindow` keeps WS_THICKFRAME and answers WM_NCHITTEST
// with HTLEFT/HTTOPRIGHT/… so the *shell* owns the resize — which is what buys
// snap-to-edge and the double-click-edge-to-fill that a hand-rolled drag never
// gets right. So this whole item is disabled there, and testing the edges in QML
// as well would consume the press before the shell ever saw it (the same reason
// app-base's eventFilter short-circuits on Windows).
//
// Everywhere else, Qt 6's startSystemResize() asks the compositor to do the same
// thing, and these eight strips are what call it.
Item {
    id: edges

    required property SottoWindow window
    readonly property bool wanted: !window.platformDrawsFrame && !window.maximized
    readonly property int m: 6

    anchors.fill: parent
    visible: wanted
    z: 1000

    // x, y, w, h are fractions of the window plus a margin term, so one model
    // covers four edges and four corners without eight anchor blocks.
    Repeater {
        model: [
            { e: Qt.LeftEdge,                 c: Qt.SizeHorCursor,   side: "l" },
            { e: Qt.RightEdge,                c: Qt.SizeHorCursor,   side: "r" },
            { e: Qt.TopEdge,                  c: Qt.SizeVerCursor,   side: "t" },
            { e: Qt.BottomEdge,               c: Qt.SizeVerCursor,   side: "b" },
            { e: Qt.LeftEdge | Qt.TopEdge,    c: Qt.SizeFDiagCursor, side: "tl" },
            { e: Qt.RightEdge | Qt.TopEdge,   c: Qt.SizeBDiagCursor, side: "tr" },
            { e: Qt.LeftEdge | Qt.BottomEdge, c: Qt.SizeBDiagCursor, side: "bl" },
            { e: Qt.RightEdge | Qt.BottomEdge, c: Qt.SizeFDiagCursor, side: "br" }
        ]

        MouseArea {
            required property var modelData
            readonly property int m: edges.m
            enabled: edges.wanted
            acceptedButtons: Qt.LeftButton
            cursorShape: modelData.c

            x: {
                switch (modelData.side) {
                case "r": case "tr": case "br": return edges.width - m
                case "t": case "b": return m
                default: return 0
                }
            }
            y: {
                switch (modelData.side) {
                case "b": case "bl": case "br": return edges.height - m
                case "l": case "r": return m
                default: return 0
                }
            }
            width: (modelData.side === "t" || modelData.side === "b")
                   ? Math.max(0, edges.width - 2 * m) : m
            height: (modelData.side === "l" || modelData.side === "r")
                    ? Math.max(0, edges.height - 2 * m) : m

            onPressed: edges.window.beginSystemResize(modelData.e)
        }
    }

    // The one-pixel edge. DWM draws it on Windows (DWMWA_BORDER_COLOR, fed from
    // `Theme.borderStrong`); nothing draws it elsewhere, so this does.
    Rectangle {
        anchors.fill: parent
        color: "transparent"
        border.width: 1
        border.color: Theme.borderStrong
    }
}
