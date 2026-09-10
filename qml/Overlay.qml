import QtQuick
import QtQuick.Window
import Sotto

// The dictation HUD: a pill floating at the bottom of the active screen.
// Window/layer-shell setup happens in OverlayController.
//
// The pill is `chromePanel` — the tinted chrome step, not the neutral content
// one. It is the only chrome Sotto has, and the 2026-09-09 decision is that
// chrome is tinted; a content panel here would read as a piece of some other
// app's window floating over the desktop.
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
        // Outlast the exit animation, whatever the setting made its duration.
        interval: Theme.durSlow + 20
        onTriggered: root.visible = false
    }

    Rectangle {
        id: pill
        anchors.fill: parent
        radius: height / 2
        // Translucent mode leaves enough alpha for compositor blur (KWin
        // blur-behind, Hyprland `layerrule = blur, sotto-hud`, Mica once
        // Windows support lands) to show through.
        color: Qt.rgba(Theme.chromePanel.r, Theme.chromePanel.g, Theme.chromePanel.b,
                       Config.overlayTranslucent ? 0.55 : 0.95)
        border.width: 1
        border.color: Theme.borderStrong
        opacity: 0
        scale: 0.97

        // A fade and a 3% scale at `--dur-slow`, the design system's entrance
        // for an overlay. Deliberately *not* the 4px rise it also asks for: the
        // window is exactly the pill, and OverlayController derives the KWin
        // blur region from the window's own geometry — a pill that moves inside
        // its window would be clipped at the bottom edge and blurred through a
        // capsule it no longer fills.
        Behavior on opacity {
            NumberAnimation { duration: Theme.durSlow; easing.type: Easing.Bezier
                              easing.bezierCurve: Theme.easeOut }
        }
        Behavior on scale {
            NumberAnimation { duration: Theme.durSlow; easing.type: Easing.Bezier
                              easing.bezierCurve: Theme.easeOut }
        }
        Behavior on color {
            ColorAnimation { duration: Theme.durBase }
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
            color: Theme.text
            font.family: Brand.bodyFamily
            font.pixelSize: 14
            // Live transcript elides from the left so the newest words stay
            // visible; a status line has to elide from the right, or
            // "Transcribing… 2 passages left" arrives as "…passages left".
            readonly property bool liveText: App.state === "listening"
                                             && App.partialText.length > 0
            elide: liveText ? Text.ElideLeft : Text.ElideRight
            maximumLineCount: 1
            // One source for every surface that says what the app is doing —
            // App::statusText(). It used to say "Formatting…" for the whole of
            // the finalizing state, which is the state that waits on whisper,
            // not on the formatter.
            text: liveText ? App.partialText.replace(/\n+/g, "  ") : App.statusText
            opacity: App.state === "listening" && App.partialText.length === 0 ? 0.55 : 1.0
            Behavior on opacity {
                NumberAnimation { duration: Theme.durBase }
            }
        }

        LocalBadge {
            id: badge
            anchors.right: parent.right
            anchors.rightMargin: 20
            anchors.verticalCenter: parent.verticalCenter
        }
    }
}
