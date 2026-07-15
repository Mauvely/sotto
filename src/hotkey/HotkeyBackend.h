#pragma once

#include <QtGlobal>

// The global-shortcut backend for the current platform. Both classes share
// the same surface (bind/release/triggerDescription/available + the
// activated/deactivated/boundChanged/failed signals) and the same trigger
// spec syntax, so App only ever talks to HotkeyBackend.
#ifdef Q_OS_WIN
#include "hotkey/WinHotkey.h"
using HotkeyBackend = WinHotkey;
#else
#include "hotkey/GlobalShortcutsPortal.h"
using HotkeyBackend = GlobalShortcutsPortal;
#endif
