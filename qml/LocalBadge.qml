import QtQuick

// Constant reminder that nothing leaves the machine.
Rectangle {
    implicitWidth: label.implicitWidth + 14
    implicitHeight: 18
    radius: 4
    color: "transparent"
    border.width: 1
    border.color: Qt.rgba(1, 1, 1, 0.28)

    Text {
        id: label
        anchors.centerIn: parent
        text: qsTr("LOCAL")
        color: Qt.rgba(1, 1, 1, 0.75)
        font.pixelSize: 8
        font.bold: true
        font.letterSpacing: 1.5
    }
}
