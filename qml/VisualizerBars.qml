import QtQuick
import Sotto

// Teal (brand signal accent) level bars fed from App.levels (a scrolling
// window of recent microphone RMS values, newest on the right).
Item {
    id: bars
    property var levels: []
    property bool animated: true
    property bool dimmed: false

    implicitWidth: row.width
    implicitHeight: 34

    Row {
        id: row
        spacing: 3
        height: parent.height

        Repeater {
            model: bars.levels.length
            Rectangle {
                width: 3
                radius: 1.5
                color: Brand.signal
                height: 4 + (bars.levels[index] || 0) * (bars.height - 6)
                y: (bars.height - height) / 2
                opacity: bars.dimmed ? 0.25
                                     : 0.45 + 0.55 * (bars.levels[index] || 0)

                Behavior on height {
                    enabled: bars.animated
                    SmoothedAnimation { duration: 90 }
                }
            }
        }
    }
}
