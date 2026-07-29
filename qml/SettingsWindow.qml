import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Sotto

Window {
    id: win
    width: 640
    height: 760
    minimumWidth: 520
    minimumHeight: 400
    title: qsTr("Sotto — Settings")
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
        mid: Brand.slate700
        dark: Brand.slate950
        light: Brand.slate800
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
                        color: Brand.textStrong
                        font.family: Brand.displayFamily
                        font.weight: Font.ExtraBold
                        font.pixelSize: 22
                    }
                    Text {
                        text: qsTr("100% local dictation — audio never leaves this device.")
                        color: Brand.textMuted
                        font.family: Brand.displayFamily
                        font.pixelSize: 13
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
                            color: modelData.installed ? Brand.textBody : Brand.textMuted
                            font.pixelSize: 13
                        }
                        Rectangle {
                            visible: modelData.recommended
                            radius: Brand.radiusXs
                            color: Qt.rgba(Brand.violet500.r, Brand.violet500.g, Brand.violet500.b, 0.20)
                            implicitWidth: recText.implicitWidth + 10
                            implicitHeight: 16
                            Text {
                                id: recText
                                anchors.centerIn: parent
                                text: qsTr("RECOMMENDED")
                                color: Brand.violet300
                                font.family: Brand.monoFamily
                                font.pixelSize: 8
                                font.letterSpacing: 1
                            }
                        }
                        Item { Layout.fillWidth: true }
                        Text {
                            text: modelData.sizeMB >= 1000
                                  ? (modelData.sizeMB / 1000).toFixed(1) + " GB"
                                  : modelData.sizeMB + " MB"
                            color: Brand.textMuted
                            font.pixelSize: 12
                        }
                        ProgressBar {
                            id: dlBar
                            visible: modelData.downloading
                            value: Models.downloadProgress
                            implicitWidth: 110
                            background: Rectangle {
                                implicitHeight: 6
                                radius: Brand.radiusPill
                                color: Qt.rgba(1, 1, 1, 0.10)
                            }
                            contentItem: Item {
                                Rectangle {
                                    width: parent.width * dlBar.visualPosition
                                    height: 6
                                    radius: Brand.radiusPill
                                    color: Brand.signal
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                            }
                        }
                        SButton {
                            size: "sm"
                            variant: "primary"
                            visible: !modelData.installed && !modelData.downloading
                            enabled: Models.downloadingId === ""
                            text: qsTr("Download")
                            onClicked: Models.download(modelData.id)
                        }
                        SButton {
                            size: "sm"
                            variant: "secondary"
                            visible: modelData.downloading
                            text: qsTr("Cancel")
                            onClicked: Models.cancelDownload()
                        }
                        SButton {
                            size: "sm"
                            variant: "secondary"
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
                    Text { text: qsTr("Spoken language"); color: Brand.textBody; font.pixelSize: 13 }
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
                    Text { text: qsTr("Preferred keys"); color: Brand.textBody; font.pixelSize: 13 }
                    TextField {
                        id: triggerField
                        implicitWidth: 180
                        text: Config.preferredShortcut
                        placeholderText: "LOGO+ALT+d"
                    }
                    SButton {
                        size: "sm"
                        variant: "primary"
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
                    Text { text: qsTr("Behaviour"); color: Brand.textBody; font.pixelSize: 13 }
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
                    SButton {
                        size: "sm"
                        variant: "secondary"
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
                    Text { text: qsTr("Insert text by"); color: Brand.textBody; font.pixelSize: 13 }
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
                    text: qsTr("Voice commands (“new line”, “new paragraph”)")
                    checked: Config.voiceCommands
                    onToggled: Config.voiceCommands = checked
                }

                RowLayout {
                    spacing: 10
                    Text {
                        text: qsTr("New paragraph after a pause of")
                        color: Brand.textBody
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
                        color: Brand.textMuted
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

                RowLayout {
                    spacing: 10
                    Text { text: qsTr("Show the popup on"); color: Brand.textBody; font.pixelSize: 13 }
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
