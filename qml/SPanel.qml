import QtQuick
import QtQuick.Layouts
import Sotto

// A content region on the window ground: 16px radius, neutral fill, no border.
// Panels are separated by the ground showing through a 16px gutter, which is
// why this draws no outline — an outline plus a gap reads as a box inside a
// box. See the design system § App chrome (2026-09-09).
//
// `tinted` switches the fill to chromePanel, the violet-tinted step used for a
// rail or a header rather than for content.
Rectangle {
    id: panel

    property bool tinted: false
    property int padding: 18
    default property alias contents: inner.data

    Layout.fillWidth: true
    implicitHeight: inner.implicitHeight + padding * 2
    radius: Theme.radiusRegion
    color: tinted ? Theme.chromePanel : Theme.panel

    // The fill follows the light/dark toggle live, so it animates with
    // everything else rather than snapping while the rest fades.
    Behavior on color {
        ColorAnimation { duration: Theme.durBase; easing.type: Easing.Bezier
                         easing.bezierCurve: Theme.easeOut }
    }

    ColumnLayout {
        id: inner
        anchors.fill: parent
        anchors.margins: panel.padding
        spacing: 12
    }
}
