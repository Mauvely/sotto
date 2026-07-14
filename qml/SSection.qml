import QtQuick
import QtQuick.Layouts

// A titled settings section with a hairline divider.
ColumnLayout {
    id: section
    property string title: ""
    default property alias contents: inner.data

    Layout.fillWidth: true
    spacing: 12

    Text {
        text: section.title.toUpperCase()
        color: Qt.rgba(1, 1, 1, 0.55)
        font.pixelSize: 11
        font.bold: true
        font.letterSpacing: 1.4
    }
    Rectangle {
        Layout.fillWidth: true
        implicitHeight: 1
        color: Qt.rgba(1, 1, 1, 0.08)
    }
    ColumnLayout {
        id: inner
        Layout.fillWidth: true
        spacing: 12
    }
}
