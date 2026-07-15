import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// Speak, and read it back: press the mic, talk, and each dictation comes
// back as a transcript bubble. Entries live in memory only — nothing is
// written to disk.
Window {
    id: win
    width: 460
    height: 620
    minimumWidth: 340
    minimumHeight: 380
    title: qsTr("Sotto — Chat")
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

    readonly property bool mine: App.dictationTarget === "chat"
    readonly property bool recording: mine && App.state === "listening"
    readonly property bool busy: mine && App.state !== "idle" && !recording

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
                text: qsTr("Speak, and read it back.")
                color: Qt.rgba(1, 1, 1, 0.6)
                font.pixelSize: 12
            }
            LocalBadge {}
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 10
            model: App.chatEntries
            ScrollBar.vertical: ScrollBar {}

            onCountChanged: Qt.callLater(function() { list.positionViewAtEnd() })

            Text {
                anchors.centerIn: parent
                visible: list.count === 0 && !win.recording && !win.busy
                text: qsTr("Press the mic and start talking…")
                color: Qt.rgba(1, 1, 1, 0.35)
                font.pixelSize: 13
            }

            delegate: Column {
                width: list.width - 8
                spacing: 4

                Rectangle {
                    id: bubble
                    width: Math.min(entryText.implicitWidth + 28, parent.width)
                    height: entryText.implicitHeight + 20
                    radius: 10
                    color: "#1A1A1D"
                    border.width: 1
                    border.color: Qt.rgba(1, 1, 1, 0.08)

                    TextEdit {
                        id: entryText
                        anchors.fill: parent
                        anchors.margins: 10
                        text: modelData.text
                        readOnly: true
                        selectByMouse: true
                        wrapMode: TextEdit.Wrap
                        color: "#F2F2F2"
                        selectionColor: "#4A4A52"
                        font.pixelSize: 13
                    }
                }

                RowLayout {
                    spacing: 8
                    Text {
                        text: modelData.time
                        color: Qt.rgba(1, 1, 1, 0.35)
                        font.pixelSize: 10
                    }
                    Text {
                        text: qsTr("Copy")
                        color: copyArea.containsMouse ? Qt.rgba(1, 1, 1, 0.9)
                                                      : Qt.rgba(1, 1, 1, 0.45)
                        font.pixelSize: 10
                        MouseArea {
                            id: copyArea
                            anchors.fill: parent
                            anchors.margins: -4 // easier to hit
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: App.copyToClipboard(modelData.text)
                        }
                    }
                }
            }

            footer: Item {
                width: list.width
                height: pending.visible ? pending.height + 10 : 0

                Rectangle {
                    id: pending
                    visible: win.recording || win.busy
                    width: Math.min(pendingText.implicitWidth + 28, parent.width - 8)
                    height: pendingText.implicitHeight + 20
                    radius: 10
                    color: "transparent"
                    border.width: 1
                    border.color: Qt.rgba(1, 1, 1, 0.14)

                    Text {
                        id: pendingText
                        anchors.fill: parent
                        anchors.margins: 10
                        wrapMode: Text.Wrap
                        color: Qt.rgba(1, 1, 1, 0.55)
                        font.pixelSize: 13
                        font.italic: true
                        text: win.busy ? qsTr("Transcribing…")
                                       : App.partialText.length > 0
                                         ? App.partialText.replace(/\n+/g, "  ")
                                         : qsTr("Listening…")
                        onTextChanged: Qt.callLater(function() { list.positionViewAtEnd() })
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Button {
                text: win.recording ? qsTr("■ Stop") : (win.busy ? qsTr("Working…") : qsTr("● Record"))
                enabled: App.state === "idle" || win.recording
                onClicked: App.toggleChatDictation()
            }

            VisualizerBars {
                // opacity, not visible: toggling visibility of a layout
                // child mid-flight confuses Qt 6.4's RowLayout, and keeping
                // the slot reserved avoids reflow when recording starts.
                opacity: win.recording ? 1 : 0
                Layout.preferredWidth: 90
                Layout.preferredHeight: 26
                levels: App.levels
                animated: Config.animationsEnabled
            }

            Item { Layout.fillWidth: true }

            Button {
                text: qsTr("Clear")
                enabled: list.count > 0
                onClicked: App.clearChat()
            }
        }

        Text {
            Layout.fillWidth: true
            text: qsTr("Transcripts stay in this window and vanish when Sotto quits.")
            color: Qt.rgba(1, 1, 1, 0.3)
            font.pixelSize: 10
        }
    }
}
