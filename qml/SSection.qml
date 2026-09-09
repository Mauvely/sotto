import QtQuick
import QtQuick.Layouts
import Sotto

// A titled settings section — and, since the 2026-09-09 chrome decision, a
// panel on the window ground rather than a run of rows under a hairline. The
// gutter between two sections is the ground showing through, so the rule that
// used to close each title off is gone: with 16px of ground above it, a line
// there reads as a stray mark rather than as an edge.
//
// Its children are declared straight onto SPanel's default property, which is
// why the eyebrow below only has to be first in this file.
SPanel {
    id: section
    property string title: ""

    Text {
        text: section.title.toUpperCase()
        color: Theme.textMuted
        font.family: Brand.monoFamily
        font.pixelSize: 11
        font.letterSpacing: 1.4
        Layout.bottomMargin: 2
    }
}
