pragma Singleton
import QtQuick
import Sotto

// ─────────────────────────────────────────────────────────────────────────────
//  The design system's *semantic* layer, resolved for the theme that is on.
//
//  `Brand` is the raw ramps and never changes. This is the table that says what
//  a ground is, what a panel is and what text on one is — the QML counterpart of
//  the sibling apps' `Theme` (snap/src/core/theme.cpp), value for value, so the
//  native apps agree about what "panel" means.
//
//  ── App chrome, decided 2026-09-09 (design system § App chrome) ─────────────
//  The window ground is violet-tinted and every content region is a neutral
//  panel at 16px radius floating on it, 16px apart. Three steps of one neutral
//  is what made the suite read as one grey; the tint and the gaps are the fix.
//
//      bg            the window ground — what shows between panels
//      chromePanel   a rail or header panel: tinted, one step off the ground
//      panel         a content region: neutral
//      panelAlt      a strip inside a panel
//      surfaceSunken an input or a well — darker than the panel it sits on
//
//  Every colour below is `--token` from tokens/colors.css. Nothing here is
//  invented and nothing is a mirrored ramp step: the light table is the design
//  system's own light table.
// ─────────────────────────────────────────────────────────────────────────────
QtObject {
    id: theme

    // Config is a root-context property set before any QML is created, so a
    // singleton can read it. Everything else in this file is a binding on it,
    // which is why the whole app repaints when the theme changes.
    readonly property bool dark: Config.darkMode

    function withAlpha(c, a) { return Qt.rgba(c.r, c.g, c.b, a) }

    // The ink hairlines and shadows are drawn with: near-black on dark,
    // mauve-black on light (`--ink-rgb`).
    readonly property color ink: dark ? "#080610" : "#281c2d"
    readonly property color inkOn: "#e9e9ed" // --text-strong on dark

    // ── Grounds ──────────────────────────────────────────────────────────
    readonly property color bg: dark ? "#12142e" : "#e8e5f9"            // --app-ground
    readonly property color chromePanel: dark ? "#1b1a33" : "#f5f3ff"   // --app-chrome-panel
    readonly property color panel: dark ? "#1b1d2b" : "#f8f9fd"         // --app-panel
    readonly property color panelAlt: dark ? "#1f2131" : "#f3f5fe"      // --app-panel-alt
    readonly property color surface: dark ? "#232532" : "#ffffff"       // --surface-card
    readonly property color surfaceRaised: dark ? "#2b2d3c" : "#ffffff"
    readonly property color surfaceSunken: dark ? "#161826" : "#eef0f8" // --app-panel-sunken

    // ── Borders ──────────────────────────────────────────────────────────
    // Dividers go *between rows inside* a panel, never around one — a panel is
    // separated by the ground showing through, not by an outline.
    readonly property color borderSubtle: dark ? withAlpha(inkOn, 0.08) : withAlpha(ink, 0.07)
    readonly property color borderHairline: dark ? withAlpha(inkOn, 0.05) : withAlpha(ink, 0.04)
    readonly property color borderStrong: dark ? withAlpha(inkOn, 0.18) : Brand.slate300

    // ── Text ─────────────────────────────────────────────────────────────
    readonly property color text: dark ? "#e9e9ed" : Brand.slate900
    readonly property color textBody: dark ? Brand.slate300 : Brand.slate800
    readonly property color textMuted: dark ? Brand.slate500 : Brand.slate600
    readonly property color textFaint: dark ? Brand.slate600 : Brand.slate500
    readonly property color textQuiet: dark ? Brand.slate700 : Brand.slate400

    // ── Interactive violet ───────────────────────────────────────────────
    // On the dark ground the design steps *up* the ramp (violet 400) with deep
    // violet ink on top; on light it steps down (violet 600) with white ink, so
    // a filled label clears 4.5:1 either way.
    readonly property color primary: dark ? Brand.violet400 : Brand.violet600
    readonly property color primaryHover: dark ? Brand.violet300 : Brand.violet700
    readonly property color primaryActive: dark ? Brand.violet500 : Brand.violet800
    readonly property color primarySoft: dark ? withAlpha(Brand.violet400, 0.14) : Brand.violet100
    readonly property color primarySoftHover: dark ? withAlpha(Brand.violet400, 0.22) : Brand.violet200
    readonly property color primaryText: dark ? Brand.violet400 : Brand.violet700
    // `--text-on-primary`. Named primaryInk and not onPrimary, which is what the
    // sibling apps' C++ Theme calls it: in QML a property whose name is `on` +
    // a capital collides with the signal-handler syntax, so the initialiser is
    // never installed as a binding and every reader gets an invalid QColor —
    // which paints black. It cost a render of black button labels in both
    // themes to find, because black on violet 400 looks deliberate.
    readonly property color primaryInk: dark ? Brand.violet950 : "#ffffff"

    readonly property color signal: dark ? Brand.teal400 : Brand.teal500
    readonly property color signalText: dark ? Brand.teal300 : Brand.teal700
    readonly property color danger: dark ? Brand.dangerHover : Brand.danger

    readonly property color focusRing: dark ? withAlpha(Brand.violet400, 0.55)
                                            : withAlpha(Brand.violet600, 0.45)

    // ── Geometry ─────────────────────────────────────────────────────────
    // A content region on the ground, and the gutter between two of them. One
    // measurement: the outer margin and the gaps are the same 16.
    readonly property int radiusRegion: Brand.radiusXl // 16
    readonly property int gridGap: 16

    // ── Motion ───────────────────────────────────────────────────────────
    // Every duration in the app goes through one of these. They return 0 when
    // Settings ▸ Appearance ▸ Animations is off, and a QML animation given a
    // zero duration hands its end value straight over — so "off" is the same
    // code arriving at once rather than a second path nobody exercises. That is
    // also the reduced-motion behaviour the design system asks for: drop the
    // transform, keep the end state.
    readonly property bool animate: Config.animationsEnabled
    readonly property int durInstant: animate ? 80 : 0
    readonly property int durFast: animate ? 120 : 0   // hover / press colour
    readonly property int durBase: animate ? 180 : 0   // reveals, switches
    readonly property int durSlow: animate ? 280 : 0   // dialogs, the HUD
    // --ease-out, cubic-bezier(.22,.61,.36,1). QML wants the two control points
    // plus the (1,1) end point.
    readonly property var easeOut: [0.22, 0.61, 0.36, 1.0, 1.0, 1.0]
}
