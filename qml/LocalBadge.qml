import QtQuick
import Sotto

// Constant reminder that nothing leaves the machine. Styled as a signal
// badge (teal) since it communicates an always-on live guarantee, the same
// role teal plays elsewhere in the brand (live/complete states).
Rectangle {
    implicitWidth: label.implicitWidth + 14
    implicitHeight: 18
    radius: Brand.radiusXs
    color: Qt.rgba(Brand.teal500.r, Brand.teal500.g, Brand.teal500.b,
                   Theme.dark ? 0.14 : 0.16)
    border.width: 1
    border.color: Qt.rgba(Brand.teal400.r, Brand.teal400.g, Brand.teal400.b,
                          Theme.dark ? 0.45 : 0.55)

    Text {
        id: label
        anchors.centerIn: parent
        text: qsTr("LOCAL")
        // teal 300 has no contrast on a light panel; the light theme steps the
        // same accent down the ramp rather than reaching for a different hue.
        color: Theme.signalText
        font.family: Brand.monoFamily
        font.pixelSize: 8
        font.bold: true
        font.letterSpacing: 1.5
    }
}
