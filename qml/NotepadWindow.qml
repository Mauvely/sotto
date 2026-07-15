import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Sotto

// Dictate into a scratch buffer instead of the focused app.
Window {
    id: win
    width: 580
    height: 540
    minimumWidth: 380
    minimumHeight: 300
    title: qsTr("Sotto — Notepad")
    color: Brand.appGround

    palette {
        window: Brand.appGround
        windowText: Brand.textBody
        base: Brand.slate900
        text: Brand.textBody
        button: Brand.slate800
        buttonText: Brand.textBody
        highlight: Brand.primary
        highlightedText: "#FFFFFF"
        placeholderText: Brand.slate500
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
                color: Brand.textMuted
                font.family: Brand.displayFamily
                font.pixelSize: 13
            }
            LocalBadge {}
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true

            TextArea {
                id: area
                wrapMode: TextArea.Wrap
                color: Brand.textBody
                font.pixelSize: 14
                placeholderText: qsTr("Press Record and start speaking…")
                background: Rectangle {
                    color: Brand.slate900
                    radius: Brand.radiusSm
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
            color: Brand.textMuted
            font.pixelSize: 12
            font.italic: true
            elide: Text.ElideLeft
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            SButton {
                variant: win.recording ? "secondary" : "primary"
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

            SButton {
                variant: "secondary"
                text: qsTr("Copy all")
                enabled: area.text.length > 0
                onClicked: App.copyToClipboard(area.text)
            }
            SButton {
                variant: "secondary"
                text: qsTr("Clear")
                enabled: area.text.length > 0
                onClicked: App.notepadText = ""
            }
        }
    }
}
