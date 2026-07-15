import QtQuick
import QtQuick.Window

// The dictation HUD: a black pill floating at the bottom of the active
// screen. Window/layer-shell setup happens in OverlayController.
Window {
    id: root
    width: 440
    height: 64
    color: "transparent"
    visible: false

    readonly property bool active: App.state !== "idle"
    readonly property bool anim: Config.animationsEnabled

    onActiveChanged: {
        if (active) {
            hideTimer.stop()
            visible = true
            pill.opacity = 1
            pill.scale = 1
        } else if (anim) {
            pill.opacity = 0
            pill.scale = 0.97
            hideTimer.restart()
        } else {
            visible = false
        }
    }

    Timer {
        id: hideTimer
        interval: 200
        onTriggered: root.visible = false
    }

    Rectangle {
        id: pill
        anchors.fill: parent
        radius: height / 2
        // Translucent mode leaves enough alpha for compositor blur (KWin
        // blur-behind, Hyprland `layerrule = blur, sotto-hud`, Mica once
        // Windows support lands) to show through.
        color: Qt.rgba(0, 0, 0, Config.overlayTranslucent ? 0.55 : 0.93)
        border.width: 1
        border.color: Qt.rgba(1, 1, 1, Config.overlayTranslucent ? 0.16 : 0.10)
        opacity: 0
        scale: 0.97

        Behavior on opacity {
            enabled: root.anim
            NumberAnimation { duration: 180; easing.type: Easing.OutCubic }
        }
        Behavior on scale {
            enabled: root.anim
            NumberAnimation { duration: 180; easing.type: Easing.OutCubic }
        }

        LogoMark {
            id: logo
            anchors.left: parent.left
            anchors.leftMargin: 20
            anchors.verticalCenter: parent.verticalCenter
            speaking: App.state === "listening"
            animated: root.anim
        }

        VisualizerBars {
            id: bars
            anchors.left: logo.right
            anchors.leftMargin: 14
            anchors.verticalCenter: parent.verticalCenter
            width: 96
            height: 34
            levels: App.levels
            animated: root.anim
            dimmed: App.state !== "listening"
        }

        Text {
            id: status
            anchors.left: bars.right
            anchors.leftMargin: 14
            anchors.right: badge.left
            anchors.rightMargin: 12
            anchors.verticalCenter: parent.verticalCenter
            color: Qt.rgba(1, 1, 1, 0.92)
            font.pixelSize: 14
            elide: Text.ElideLeft // live text: keep the newest words visible
            maximumLineCount: 1
            text: {
                switch (App.state) {
                case "loading":    return qsTr("Loading model…")
                case "finalizing": return qsTr("Formatting…")
                case "inserting":  return qsTr("Inserting…")
                case "listening":
                    return App.partialText.length > 0
                        ? App.partialText.replace(/\n+/g, "  ")
                        : qsTr("Listening…")
                }
                return ""
            }
            opacity: App.state === "listening" && App.partialText.length === 0 ? 0.55 : 1.0
        }

        LocalBadge {
            id: badge
            anchors.right: parent.right
            anchors.rightMargin: 20
            anchors.verticalCenter: parent.verticalCenter
        }
    }
}
