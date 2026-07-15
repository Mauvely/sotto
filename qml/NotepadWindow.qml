import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// Dictate into a scratch buffer instead of the focused app.
Window {
    id: win
    width: 580
    height: 540
    minimumWidth: 380
    minimumHeight: 300
    title: qsTr("Sotto — Notepad")
    color: "#0D0D0F"

    palette {
        window: "#0D0D0F"
        windowText: "#F2F2F2"
        base: "#1A1A1D"
        text: "#F2F2F2"
        button: "#242428"
        buttonText: "#F2F2F2"
        highlight: "#4A4A52"
        highlightedText: "#FFFFFF"
        placeholderText: "#77777D"
    }

    readonly property bool recording: App.state === "listening"
    readonly property bool busy: App.state !== "idle" && !recording

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 18
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            LogoMark { size: 24 }
            Text {
                Layout.fillWidth: true
                text: qsTr("Dictate here, take the text anywhere.")
                color: Qt.rgba(1, 1, 1, 0.6)
                font.pixelSize: 12
            }
            LocalBadge {}
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true

            TextArea {
                id: area
                wrapMode: TextArea.Wrap
                color: "#F2F2F2"
                font.pixelSize: 14
                placeholderText: qsTr("Press Record and start speaking…")
                background: Rectangle {
                    color: "#1A1A1D"
                    radius: 8
                    border.width: 1
                    border.color: Qt.rgba(1, 1, 1, 0.08)
                }
                Component.onCompleted: text = App.notepadText
                onTextChanged: if (text !== App.notepadText) App.notepadText = text
                Connections {
                    target: App
                    function onNotepadTextChanged() {
                        if (area.text !== App.notepadText)
                            area.text = App.notepadText
                    }
                }
            }
        }

        Text {
            Layout.fillWidth: true
            visible: win.recording && App.partialText.length > 0
            text: App.partialText.replace(/\n+/g, "  ")
            color: Qt.rgba(1, 1, 1, 0.5)
            font.pixelSize: 12
            font.italic: true
            elide: Text.ElideLeft
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Button {
                text: win.recording ? qsTr("■ Stop") : (win.busy ? qsTr("Working…") : qsTr("● Record"))
                enabled: App.state === "idle" || win.recording
                onClicked: App.toggleNotepadDictation()
            }

            VisualizerBars {
                visible: win.recording
                Layout.preferredWidth: 90
                Layout.preferredHeight: 26
                levels: App.levels
                animated: Config.animationsEnabled
            }

            Item { Layout.fillWidth: true }

            Button {
                text: qsTr("Copy all")
                enabled: area.text.length > 0
                onClicked: App.copyToClipboard(area.text)
            }
            Button {
                text: qsTr("Clear")
                enabled: area.text.length > 0
                onClicked: App.notepadText = ""
            }
        }
    }
}
