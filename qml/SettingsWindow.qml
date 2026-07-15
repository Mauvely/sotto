import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Window {
    id: win
    width: 640
    height: 760
    minimumWidth: 520
    minimumHeight: 400
    title: qsTr("Sotto — Settings")
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
        mid: "#3A3A40"
        dark: "#111114"
        light: "#2E2E33"
    }

    Flickable {
        anchors.fill: parent
        contentWidth: width
        contentHeight: content.height + 56
        clip: true
        ScrollBar.vertical: ScrollBar {}

        ColumnLayout {
            id: content
            x: 28
            y: 28
            width: parent.width - 56
            spacing: 30

            // ------------------------------------------------ header
            RowLayout {
                Layout.fillWidth: true
                spacing: 14

                LogoMark { size: 34 }

                ColumnLayout {
                    spacing: 2
                    Layout.fillWidth: true
                    Text {
                        text: "Sotto"
                        color: "#FFFFFF"
                        font.pixelSize: 22
                        font.bold: true
                    }
                    Text {
                        text: qsTr("100% local dictation — audio never leaves this device.")
                        color: Qt.rgba(1, 1, 1, 0.6)
                        font.pixelSize: 12
                    }
                }

                LocalBadge {}
            }

            // ------------------------------------------------ model
            SSection {
                title: qsTr("Speech model")

                SLabel {
                    text: qsTr("Models run entirely on this machine via whisper.cpp "
                               + "(backend: %1). Large v3 Turbo is the sweet spot on a "
                               + "discrete GPU; Small if you want a light download.")
                               .arg(App.gpuBackend)
                }

                Repeater {
                    model: Models.models
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10

                        RadioButton {
                            checked: Config.modelId === modelData.id
                            enabled: modelData.installed
                            onClicked: Config.modelId = modelData.id
                        }
                        Text {
                            text: modelData.label
                            color: modelData.installed ? "#F2F2F2" : Qt.rgba(1, 1, 1, 0.5)
                            font.pixelSize: 13
                        }
                        Rectangle {
                            visible: modelData.recommended
                            radius: 3
                            color: Qt.rgba(1, 1, 1, 0.12)
                            implicitWidth: recText.implicitWidth + 10
                            implicitHeight: 16
                            Text {
                                id: recText
                                anchors.centerIn: parent
                                text: qsTr("RECOMMENDED")
                                color: Qt.rgba(1, 1, 1, 0.8)
                                font.pixelSize: 8
                                font.letterSpacing: 1
                            }
                        }
                        Item { Layout.fillWidth: true }
                        Text {
                            text: modelData.sizeMB >= 1000
                                  ? (modelData.sizeMB / 1000).toFixed(1) + " GB"
                                  : modelData.sizeMB + " MB"
                            color: Qt.rgba(1, 1, 1, 0.45)
                            font.pixelSize: 12
                        }
                        ProgressBar {
                            visible: modelData.downloading
                            value: Models.downloadProgress
                            implicitWidth: 110
                        }
                        Button {
                            visible: !modelData.installed && !modelData.downloading
                            enabled: Models.downloadingId === ""
                            text: qsTr("Download")
                            onClicked: Models.download(modelData.id)
                        }
                        Button {
                            visible: modelData.downloading
                            text: qsTr("Cancel")
                            onClicked: Models.cancelDownload()
                        }
                        Button {
                            visible: modelData.installed
                            text: qsTr("Remove")
                            onClicked: {
                                if (Config.modelId === modelData.id)
                                    Config.modelId = ""
                                Models.remove(modelData.id)
                            }
                        }
                    }
                }

                RowLayout {
                    spacing: 10
                    Text { text: qsTr("Spoken language"); color: "#F2F2F2"; font.pixelSize: 13 }
                    ComboBox {
                        id: langBox
                        implicitWidth: 220
                        textRole: "name"
                        valueRole: "code"
                        model: [
                            { name: qsTr("Auto-detect"), code: "auto" },
                            { name: "English", code: "en" },
                            { name: "Deutsch", code: "de" },
                            { name: "Français", code: "fr" },
                            { name: "Español", code: "es" },
                            { name: "Italiano", code: "it" },
                            { name: "Português", code: "pt" },
                            { name: "Nederlands", code: "nl" },
                            { name: "Polski", code: "pl" },
                            { name: "Русский", code: "ru" },
                            { name: "Українська", code: "uk" },
                            { name: "Türkçe", code: "tr" },
                            { name: "日本語", code: "ja" },
                            { name: "한국어", code: "ko" },
                            { name: "中文", code: "zh" }
                        ]
                        Component.onCompleted: currentIndex = Math.max(0, indexOfValue(Config.language))
                        onActivated: Config.language = currentValue
                    }
                }
            }

            // ------------------------------------------------ shortcut
            SSection {
                title: qsTr("Global shortcut")

                Switch {
                    text: qsTr("Enable the global shortcut")
                    checked: Config.shortcutEnabled
                    onToggled: {
                        Config.shortcutEnabled = checked
                        App.applyShortcutSettings()
                    }
                }

                RowLayout {
                    spacing: 10
                    enabled: Config.shortcutEnabled
                    Text { text: qsTr("Preferred keys"); color: "#F2F2F2"; font.pixelSize: 13 }
                    TextField {
                        id: triggerField
                        implicitWidth: 180
                        text: Config.preferredShortcut
                        placeholderText: "LOGO+ALT+d"
                    }
                    Button {
                        text: qsTr("Apply && re-bind")
                        onClicked: {
                            Config.preferredShortcut = triggerField.text
                            App.applyShortcutSettings()
                        }
                    }
                }

                SLabel {
                    text: App.boundShortcut.length > 0
                          ? qsTr("Currently bound: %1").arg(App.boundShortcut)
                          : qsTr("Not bound yet. LOGO = the Super/Meta key. The binding can also "
                                 + "be changed any time in Plasma System Settings → Shortcuts, or "
                                 + "bind `sotto --toggle` to any compositor shortcut instead.")
                }

                RowLayout {
                    spacing: 10
                    enabled: Config.shortcutEnabled
                    Text { text: qsTr("Behaviour"); color: "#F2F2F2"; font.pixelSize: 13 }
                    ComboBox {
                        implicitWidth: 220
                        textRole: "name"
                        valueRole: "value"
                        model: [
                            { name: qsTr("Press to start / stop"), value: "toggle" },
                            { name: qsTr("Hold to talk"), value: "hold" }
                        ]
                        Component.onCompleted: currentIndex = Math.max(0, indexOfValue(Config.hotkeyMode))
                        onActivated: Config.hotkeyMode = currentValue
                    }
                }
            }

            // ------------------------------------------------ audio
            SSection {
                title: qsTr("Microphone")

                RowLayout {
                    spacing: 10
                    ComboBox {
                        id: deviceBox
                        Layout.fillWidth: true
                        model: App.audioDevices
                        Component.onCompleted: {
                            var i = App.audioDevices.indexOf(Config.audioDevice)
                            currentIndex = i >= 0 ? i : 0
                        }
                        onActivated: Config.audioDevice = currentText
                    }
                    Button {
                        text: qsTr("Refresh")
                        onClicked: App.refreshAudioDevices()
                    }
                }
            }

            // ------------------------------------------------ output
            SSection {
                title: qsTr("Output")

                RowLayout {
                    spacing: 10
                    Text { text: qsTr("Insert text by"); color: "#F2F2F2"; font.pixelSize: 13 }
                    ComboBox {
                        implicitWidth: 260
                        textRole: "name"
                        valueRole: "value"
                        model: [
                            { name: qsTr("Auto (recommended)"), value: "auto" },
                            { name: qsTr("Clipboard + paste keystroke"), value: "clipboard-paste" },
                            { name: qsTr("Type it (ydotool)"), value: "ydotool-type" },
                            { name: qsTr("Clipboard only"), value: "clipboard-only" }
                        ]
                        Component.onCompleted: currentIndex = Math.max(0, indexOfValue(Config.injectionMode))
                        onActivated: Config.injectionMode = currentValue
                    }
                }

                SLabel { text: App.injectionDiagnostics }

                Switch {
                    text: qsTr("Restore the previous clipboard after pasting")
                    checked: Config.restoreClipboard
                    onToggled: Config.restoreClipboard = checked
                }
            }

            // ------------------------------------------------ formatting
            SSection {
                title: qsTr("Formatting")

                Switch {
                    text: qsTr("Voice commands")
                    checked: Config.voiceCommands
                    onToggled: Config.voiceCommands = checked
                }

                GridLayout {
                    columns: 2
                    columnSpacing: 18
                    rowSpacing: 0
                    Layout.leftMargin: 12
                    enabled: Config.voiceCommands

                    CheckBox {
                        text: qsTr("“new line”")
                        checked: Config.voiceCmdNewLine
                        onToggled: Config.voiceCmdNewLine = checked
                    }
                    CheckBox {
                        text: qsTr("“new paragraph”")
                        checked: Config.voiceCmdNewParagraph
                        onToggled: Config.voiceCmdNewParagraph = checked
                    }
                    CheckBox {
                        text: qsTr("“delete last line”")
                        checked: Config.voiceCmdDeleteLastLine
                        onToggled: Config.voiceCmdDeleteLastLine = checked
                    }
                    CheckBox {
                        text: qsTr("“delete last sentence”")
                        checked: Config.voiceCmdDeleteLastSentence
                        onToggled: Config.voiceCmdDeleteLastSentence = checked
                    }
                }

                SLabel {
                    visible: Config.voiceCommands
                    text: qsTr("Commands are matched in the transcript, so mind false positives "
                               + "(“a new line of products”) — disable the ones you don't use. "
                               + "“delete …” also answers to remove/scratch/erase.")
                }

                RowLayout {
                    spacing: 10
                    Text {
                        text: qsTr("New paragraph after a pause of")
                        color: "#F2F2F2"
                        font.pixelSize: 13
                    }
                    Slider {
                        id: pauseSlider
                        from: 1.0
                        to: 4.0
                        stepSize: 0.1
                        value: Config.paragraphPauseSec
                        onMoved: Config.paragraphPauseSec = value
                        implicitWidth: 180
                    }
                    Text {
                        text: pauseSlider.value.toFixed(1) + " s"
                        color: Qt.rgba(1, 1, 1, 0.6)
                        font.pixelSize: 13
                    }
                }
            }

            // ------------------------------------------------ appearance
            SSection {
                title: qsTr("Appearance")

                Switch {
                    text: qsTr("Animations in the dictation popup")
                    checked: Config.animationsEnabled
                    onToggled: Config.animationsEnabled = checked
                }

                Switch {
                    text: qsTr("Translucent popup background")
                    checked: Config.overlayTranslucent
                    onToggled: Config.overlayTranslucent = checked
                }

                SLabel {
                    visible: Config.overlayTranslucent
                    text: qsTr("Lets your compositor blur through the popup. KWin blurs it "
                               + "automatically%1; on Hyprland add "
                               + "`layerrule = blur, sotto-hud` to your config.")
                               .arg(App.blurAvailable ? "" : qsTr(" (rebuild with KWindowSystem installed)"))
                }

                RowLayout {
                    spacing: 10
                    Text { text: qsTr("Show the popup on"); color: "#F2F2F2"; font.pixelSize: 13 }
                    ComboBox {
                        implicitWidth: 240
                        model: App.screenNames
                        displayText: currentText === "auto" ? qsTr("Active screen (auto)") : currentText
                        Component.onCompleted: {
                            var i = App.screenNames.indexOf(Config.overlayScreen)
                            currentIndex = i >= 0 ? i : 0
                        }
                        onActivated: Config.overlayScreen = currentText
                    }
                }
            }

            // ------------------------------------------------ general
            SSection {
                title: qsTr("General")

                Switch {
                    text: qsTr("Launch at login")
                    checked: Config.launchAtLogin
                    onToggled: Config.launchAtLogin = checked
                }

                SLabel {
                    text: qsTr("Sotto %1 · whisper.cpp backend: %2").arg(App.version).arg(App.gpuBackend)
                }
                SLabel {
                    visible: App.systemInfo.length > 0
                    text: App.systemInfo
                    font.pixelSize: 10
                }
            }

            Item { implicitHeight: 8 }
        }
    }
}
