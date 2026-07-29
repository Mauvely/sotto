import QtQuick
import QtQuick.Layouts
import Sotto

// A titled settings section with a hairline divider.
ColumnLayout {
    id: section
    property string title: ""
    default property alias contents: inner.data

    Layout.fillWidth: true
    spacing: 12

    Text {
        text: section.title.toUpperCase()
        color: Brand.textMuted
        font.family: Brand.monoFamily
        font.pixelSize: 11
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
