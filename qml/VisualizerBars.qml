import QtQuick

// Monochrome level bars fed from App.levels (a scrolling window of recent
// microphone RMS values, newest on the right).
//
// The level window can hold more entries than the space the HUD reserves
// for the visualiser, so only as many of the *newest* levels as physically
// fit are drawn — an overflowing Row would paint bars underneath whatever
// sits next to this item (that was exactly the "text overlaps the waveform"
// bug in the overlay pill).
Item {
    id: bars
    property var levels: []
    property bool animated: true
    property bool dimmed: false
    property real barWidth: 3
    property real barSpacing: 3

    readonly property int barCount: Math.max(1, Math.min(levels.length,
        Math.floor((width + barSpacing) / (barWidth + barSpacing))))
    readonly property int firstIndex: levels.length - barCount

    implicitWidth: levels.length > 0
        ? levels.length * (barWidth + barSpacing) - barSpacing : 0
    implicitHeight: 34

    Row {
        id: row
        spacing: bars.barSpacing
        height: parent.height

        Repeater {
            model: bars.barCount
            Rectangle {
                width: bars.barWidth
                radius: bars.barWidth / 2
                color: "#FFFFFF"
                height: 4 + (bars.levels[bars.firstIndex + index] || 0) * (bars.height - 6)
                y: (bars.height - height) / 2
                opacity: bars.dimmed ? 0.25
                                     : 0.45 + 0.55 * (bars.levels[bars.firstIndex + index] || 0)

                Behavior on height {
                    enabled: bars.animated
                    SmoothedAnimation { duration: 90 }
                }
            }
        }
    }
}
