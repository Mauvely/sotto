import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Sotto

// Settings, as panels on the window ground (design system § App chrome,
// 2026-09-09). The header row sits directly on the ground — it is this
// window's title bar — and every section below it is a 16px-radius panel,
// 16px apart, with the ground showing between them. The outer margin and the
// gutters are the same measurement, `Theme.gridGap`.
Window {
    id: win
    width: 640
    height: 760
    minimumWidth: 520
    minimumHeight: 400
    title: qsTr("Sotto — Settings")
    color: Theme.bg

    readonly property bool isWindows: Qt.platform.os === "windows"

    // The Basic style still reaches for the palette in the few places these
    // S* wrappers do not cover (text selection handles, tooltips).
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
        mid: Theme.borderStrong
        dark: Theme.bg
        light: Theme.panelAlt
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.gridGap
        spacing: Theme.gridGap

        // ------------------------------------------------ header (on the ground)
        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 4
            Layout.rightMargin: 4
            spacing: 14

            LogoMark { size: 34 }

            ColumnLayout {
                spacing: 2
                Layout.fillWidth: true
                Text {
                    text: "Sotto"
                    color: Theme.text
                    font.family: Brand.displayFamily
                    font.weight: Font.ExtraBold
                    font.pixelSize: 22
                }
                Text {
                    text: qsTr("100% local dictation — audio never leaves this device.")
                    color: Theme.textMuted
                    font.family: Brand.displayFamily
                    font.pixelSize: 13
                }
            }

            LocalBadge {}
        }

        Flickable {
            id: flick
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: width
            contentHeight: content.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            ScrollBar.vertical: ScrollBar {
                policy: ScrollBar.AsNeeded
                contentItem: Rectangle {
                    implicitWidth: 5
                    radius: 2.5
                    color: Qt.rgba(Theme.text.r, Theme.text.g, Theme.text.b,
                                   parent.pressed ? 0.30 : 0.18)
                }
                background: null
            }

            ColumnLayout {
                id: content
                // A channel for the scrollbar, so the thumb rides on the ground
                // beside the panels rather than over a panel's rounded edge.
                width: flick.width - 10
                spacing: Theme.gridGap

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

                            SRadio {
                                checked: Config.modelId === modelData.id
                                enabled: modelData.installed
                                onClicked: Config.modelId = modelData.id
                            }
                            Text {
                                text: modelData.label
                                color: modelData.installed ? Theme.textBody : Theme.textMuted
                                font.family: Brand.bodyFamily
                                font.pixelSize: 13
                            }
                            Rectangle {
                                visible: modelData.recommended
                                radius: Brand.radiusXs
                                color: Theme.primarySoft
                                implicitWidth: recText.implicitWidth + 10
                                implicitHeight: 16
                                Text {
                                    id: recText
                                    anchors.centerIn: parent
                                    text: qsTr("RECOMMENDED")
                                    color: Theme.primaryText
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
                                color: Theme.textMuted
                                font.family: Brand.bodyFamily
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
                                    color: Theme.surfaceSunken
                                }
                                contentItem: Item {
                                    Rectangle {
                                        width: parent.width * dlBar.visualPosition
                                        height: 6
                                        radius: Brand.radiusPill
                                        color: Theme.signal
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
                        Text {
                            text: qsTr("Spoken language")
                            color: Theme.textBody
                            font.family: Brand.bodyFamily
                            font.pixelSize: 13
                        }
                        SComboBox {
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

                    SSwitch {
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
                        Text {
                            text: qsTr("Preferred keys")
                            color: Theme.textBody
                            font.family: Brand.bodyFamily
                            font.pixelSize: 13
                        }
                        STextField {
                            id: triggerField
                            implicitWidth: 180
                            text: Config.preferredShortcut
                            placeholderText: win.isWindows ? "CTRL+ALT+d" : "LOGO+ALT+d"
                        }
                        SButton {
                            size: "sm"
                            variant: "primary"
                            // One ampersand. "&&" is the QWidget escape for a
                            // literal "&" and Qt Quick does not parse mnemonics
                            // at all, so this button read "Apply && re-bind".
                            text: qsTr("Apply & re-bind")
                            onClicked: {
                                Config.preferredShortcut = triggerField.text
                                App.applyShortcutSettings()
                            }
                        }
                    }

                    SLabel {
                        text: App.boundShortcut.length > 0
                              ? qsTr("Currently bound: %1").arg(App.boundShortcut)
                              : win.isWindows
                                ? qsTr("Not bound yet. Windows keeps several combinations for "
                                       + "itself and refuses them here — LOGO+ALT+D is the "
                                       + "clock, for one. LOGO = the Windows key; if nothing "
                                       + "binds, try a different letter.")
                                : qsTr("Not bound yet. LOGO = the Super/Meta key. The binding can also "
                                       + "be changed any time in Plasma System Settings → Shortcuts, or "
                                       + "bind `sotto --toggle` to any compositor shortcut instead.")
                    }

                    RowLayout {
                        spacing: 10
                        enabled: Config.shortcutEnabled
                        Text {
                            text: qsTr("Behaviour")
                            color: Theme.textBody
                            font.family: Brand.bodyFamily
                            font.pixelSize: 13
                        }
                        SComboBox {
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
                        Layout.fillWidth: true
                        SComboBox {
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
                        Text {
                            text: qsTr("Insert text by")
                            color: Theme.textBody
                            font.family: Brand.bodyFamily
                            font.pixelSize: 13
                        }
                        SComboBox {
                            implicitWidth: 260
                            textRole: "name"
                            valueRole: "value"
                            // The "ydotool-type" value is what sotto.conf stores
                            // for "type it" on every platform; only the label
                            // names the platform's typing backend.
                            model: [
                                { name: qsTr("Auto (recommended)"), value: "auto" },
                                { name: qsTr("Clipboard + paste keystroke"), value: "clipboard-paste" },
                                { name: win.isWindows ? qsTr("Type it (simulated keystrokes)")
                                                      : qsTr("Type it (ydotool)"), value: "ydotool-type" },
                                { name: qsTr("Clipboard only"), value: "clipboard-only" }
                            ]
                            Component.onCompleted: currentIndex = Math.max(0, indexOfValue(Config.injectionMode))
                            onActivated: Config.injectionMode = currentValue
                        }
                    }

                    SLabel { text: App.injectionDiagnostics }

                    SSwitch {
                        text: qsTr("Restore the previous clipboard after pasting")
                        checked: Config.restoreClipboard
                        onToggled: Config.restoreClipboard = checked
                    }
                }

                // ------------------------------------------------ formatting
                SSection {
                    title: qsTr("Formatting")

                    SSwitch {
                        text: qsTr("Voice commands")
                        checked: Config.voiceCommands
                        onToggled: Config.voiceCommands = checked
                    }

                    GridLayout {
                        columns: 2
                        columnSpacing: 18
                        rowSpacing: 4
                        Layout.leftMargin: 12
                        enabled: Config.voiceCommands

                        SCheckBox {
                            text: qsTr("“new line”")
                            checked: Config.voiceCmdNewLine
                            onToggled: Config.voiceCmdNewLine = checked
                        }
                        SCheckBox {
                            text: qsTr("“new paragraph”")
                            checked: Config.voiceCmdNewParagraph
                            onToggled: Config.voiceCmdNewParagraph = checked
                        }
                        SCheckBox {
                            text: qsTr("“delete last line”")
                            checked: Config.voiceCmdDeleteLastLine
                            onToggled: Config.voiceCmdDeleteLastLine = checked
                        }
                        SCheckBox {
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
                            color: Theme.textBody
                            font.family: Brand.bodyFamily
                            font.pixelSize: 13
                        }
                        SSlider {
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
                            color: Theme.textMuted
                            font.family: Brand.bodyFamily
                            font.pixelSize: 13
                        }
                    }
                }

                // ------------------------------------------------ appearance
                SSection {
                    title: qsTr("Appearance")

                    RowLayout {
                        spacing: 10
                        Text {
                            text: qsTr("Theme")
                            color: Theme.textBody
                            font.family: Brand.bodyFamily
                            font.pixelSize: 13
                        }
                        SComboBox {
                            implicitWidth: 220
                            textRole: "name"
                            valueRole: "value"
                            model: [
                                { name: qsTr("Match the desktop"), value: "system" },
                                { name: qsTr("Dark"), value: "dark" },
                                { name: qsTr("Light"), value: "light" }
                            ]
                            Component.onCompleted: currentIndex = Math.max(0, indexOfValue(Config.theme))
                            onActivated: Config.theme = currentValue
                        }
                    }

                    SSwitch {
                        // Not "in the dictation popup" any more: this switch is
                        // what `Theme.durFast/durBase/durSlow` return zero for,
                        // so it governs every animation in the app.
                        text: qsTr("Animations")
                        checked: Config.animationsEnabled
                        onToggled: Config.animationsEnabled = checked
                    }

                    SSwitch {
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
                        Text {
                            text: qsTr("Show the popup on")
                            color: Theme.textBody
                            font.family: Brand.bodyFamily
                            font.pixelSize: 13
                        }
                        SComboBox {
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

                    SSwitch {
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
            }
        }
    }
}
