import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Sotto

// Dictate into a scratch buffer instead of the focused app.
//
// Two panels on the ground: the editor, and the transport strip under it. The
// header row is chrome and sits on the ground itself, like the settings
// window's — see the design system § App chrome (2026-09-09).
Window {
    id: win
    width: 580
    height: 540
    minimumWidth: 380
    minimumHeight: 300
    title: qsTr("Sotto — Notepad")
    color: Theme.bg

    palette {
        window: Theme.bg
        windowText: Theme.textBody
        base: Theme.surfaceSunken
        text: Theme.textBody
        button: Theme.surface
        buttonText: Theme.textBody
        highlight: Theme.primarySoftHover
        highlightedText: Theme.text
        placeholderText: Theme.textQuiet
    }

    readonly property bool recording: App.state === "listening"
    readonly property bool busy: App.state !== "idle" && !recording

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.gridGap
        spacing: Theme.gridGap

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 4
            Layout.rightMargin: 4
            spacing: 12
            LogoMark { size: 24 }
            Text {
                Layout.fillWidth: true
                text: qsTr("Dictate here, take the text anywhere.")
                color: Theme.textMuted
                font.family: Brand.displayFamily
                font.pixelSize: 13
            }
            LocalBadge {}
        }

        SPanel {
            Layout.fillWidth: true
            Layout.fillHeight: true
            padding: 16

            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true

                TextArea {
                    id: area
                    wrapMode: TextArea.Wrap
                    color: Theme.textBody
                    font.family: Brand.bodyFamily
                    font.pixelSize: 14
                    placeholderText: qsTr("Press Record and start speaking…")
                    placeholderTextColor: Theme.textQuiet
                    selectionColor: Theme.primarySoftHover
                    selectedTextColor: Theme.text
                    leftPadding: 0
                    rightPadding: 0
                    topPadding: 0
                    // No ground of its own. The panel *is* the editor: a sunken
                    // well filling a panel edge to edge draws the same region
                    // twice, and the design system's inset treatment is for a
                    // field sitting among other things, not for the one piece of
                    // content a window exists to show.
                    background: null
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
                color: Theme.textMuted
                font.family: Brand.bodyFamily
                font.pixelSize: 12
                font.italic: true
                elide: Text.ElideLeft
            }
        }

        SPanel {
            Layout.fillWidth: true
            tinted: true
            padding: 12

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                SButton {
                    variant: win.recording ? "secondary" : "primary"
                    text: win.recording ? qsTr("Stop") : (win.busy ? qsTr("Working…") : qsTr("Record"))
                    enabled: App.state === "idle" || win.recording
                    onClicked: App.toggleNotepadDictation()
                }

                // The state dot the "● Record" / "■ Stop" glyphs used to carry.
                // Unicode standing in for an icon is out (design system
                // § Iconography); a teal signal dot is the brand's own way to
                // say live, and it is the same mark the HUD uses.
                Rectangle {
                    visible: win.recording
                    Layout.preferredWidth: 8
                    Layout.preferredHeight: 8
                    radius: 4
                    color: Theme.signal
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
}
