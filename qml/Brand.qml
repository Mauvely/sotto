pragma Singleton
import QtQuick

// The Mauvely brand palette (design reference v1.1), verbatim. Single source
// of truth for brand colour/type in QML — mirrors compose's src/core/brand.h
// so the two apps don't drift.
//
// Violet is the identity colour. Mauve is the supporting accent. Teal is a
// locked signal accent used sparingly (progress fills, live/complete states).
// Slate is the neutral text/greys. Danger and warning are invented hues tuned
// to sit beside mauve and violet.
QtObject {
    // ── Violet — brand identity ──────────────────────────────────────────
    readonly property color violet50: "#f9f9ff"
    readonly property color violet100: "#f0eefe"
    readonly property color violet200: "#e7e5fe"
    readonly property color violet300: "#d2cefd"
    readonly property color violet400: "#b5abfc"
    readonly property color violet500: "#968ae0"
    readonly property color violet600: "#796cbf"
    readonly property color violet700: "#5d5294"
    readonly property color violet800: "#423a6a"
    readonly property color violet900: "#2b2741"
    readonly property color violet950: "#1a1728"

    // ── Mauve — supporting accent ────────────────────────────────────────
    readonly property color mauve50: "#faf6fb"
    readonly property color mauve100: "#f2eaf4"
    readonly property color mauve200: "#e2d2e6"
    readonly property color mauve300: "#cbb0d1"
    readonly property color mauve400: "#ac89b6"
    readonly property color mauve500: "#886193"
    readonly property color mauve600: "#6f4e79"
    readonly property color mauve700: "#5b4063"
    readonly property color mauve800: "#4a3651"
    readonly property color mauve900: "#3d2d43"
    readonly property color mauve950: "#281c2d"

    // ── Teal — the locked signal accent ──────────────────────────────────
    readonly property color teal50: "#e6fbf9"
    readonly property color teal100: "#c1f5f0"
    readonly property color teal200: "#8aeae2"
    readonly property color teal300: "#49d9cf"
    readonly property color teal400: "#17c6bb"
    readonly property color teal500: "#0fb3a8"
    readonly property color teal600: "#0d938b"
    readonly property color teal700: "#0f736d"
    readonly property color teal800: "#115a56"
    readonly property color teal900: "#123f3c"
    readonly property color teal950: "#082523"

    // ── Slate — neutral text and greys ───────────────────────────────────
    readonly property color slate50: "#f8f9fd"
    readonly property color slate100: "#f3f5fe"
    readonly property color slate200: "#e4e7f5"
    readonly property color slate300: "#cfd3e5"
    readonly property color slate400: "#b2b6ca"
    readonly property color slate500: "#9397ab"
    readonly property color slate600: "#75798c"
    readonly property color slate700: "#595d6c"
    readonly property color slate800: "#3f424d"
    readonly property color slate900: "#292b31"
    readonly property color slate950: "#161826"

    // ── Semantic roles ───────────────────────────────────────────────────
    readonly property color identity: violet500       // wordmarks, flat fields
    readonly property color primary: violet600        // filled interactive surfaces
    readonly property color primaryHover: violet700
    readonly property color primarySoft: violet100
    readonly property color accent: mauve500           // supporting accent
    readonly property color accentHover: mauve600
    readonly property color signal: teal500            // progress, live, complete
    readonly property color signalHover: teal600
    readonly property color success: teal600

    readonly property color danger: "#d94f6f"
    readonly property color dangerHover: "#b93c59"
    readonly property color dangerSoft: "#fdecf0"
    readonly property color warning: "#e8a33d"
    readonly property color warningHover: "#c9852a"
    readonly property color warningSoft: "#fdf1de"

    // Grounds. The overlay HUD and settings/notepad windows sit on slate 950.
    readonly property color appGround: slate950         // #161826
    readonly property color pageGround: "#eceaf0"
    readonly property color cardSurface: "#ffffff"

    readonly property color textStrong: slate100        // headings, on dark ground
    readonly property color textBody: "#e9e9ed"
    readonly property color textMuted: slate500
    readonly property color textFaint: slate600

    // ── Radii ─────────────────────────────────────────────────────────────
    // Not a strict grid — the brand board uses these freely. Panels 22,
    // tiles 16, controls 10–12, chips 6–8.
    readonly property int radiusXs: 6
    readonly property int radiusSm: 8
    readonly property int radiusMd: 10
    readonly property int radiusLg: 12
    readonly property int radiusXl: 16
    readonly property int radius2Xl: 22
    readonly property int radius3Xl: 28
    readonly property int radiusPill: 999

    // ── Faces ─────────────────────────────────────────────────────────────
    // Baloo 2 is the display face (ExtraBold titles, Regular subtitles — the
    // brand's signature move); Inter carries body/UI; JetBrains Mono carries
    // labels and code. All three are bundled (see resources/fonts/) and
    // registered with Qt in main.cpp before the QML engine starts.
    readonly property string displayFamily: "Baloo 2"
    readonly property string bodyFamily: "Inter"
    readonly property string monoFamily: "JetBrains Mono"
}
