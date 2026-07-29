import QtQuick
import Sotto

// Constant reminder that nothing leaves the machine. Styled as a signal
// badge (teal) since it communicates an always-on live guarantee, the same
// role teal plays elsewhere in the brand (live/complete states).
Rectangle {
    implicitWidth: label.implicitWidth + 14
    implicitHeight: 18
    radius: Brand.radiusXs
    color: Qt.rgba(Brand.teal500.r, Brand.teal500.g, Brand.teal500.b, 0.14)
    border.width: 1
    border.color: Qt.rgba(Brand.teal400.r, Brand.teal400.g, Brand.teal400.b, 0.45)

    Text {
        id: label
        anchors.centerIn: parent
        text: qsTr("LOCAL")
        color: Brand.teal300
        font.family: Brand.monoFamily
        font.pixelSize: 8
        font.bold: true
        font.letterSpacing: 1.5
    }
}
