# UI preview (host side)

The 3DS cannot be run here, so this tool compiles the real view classes of `source/ui` for the PC, draws sample screens with them
(a small recorder replaces the citro2d drawing) and renders the result with Python.

- `./build.sh` then `./run.sh` : writes `out/*.png` (the bottom screen at 3x).
- `harness.cpp` builds the sample screens (home list, settings, menus, dialog, channel rows). Add your own there.
- `stub.cpp` replaces the 3DS drawing and the thumbnail loader. Text widths use the Inter font as a stand-in for the system font, so
  line breaks and widths are close but not exact.
- It needs `g++`, Python with Pillow and the Inter font (`/usr/share/fonts/opentype/inter`).

It shows layout and colors, not performance or real pictures (thumbnails are colored blocks).

## Regression tests for touches
`./selector_test.sh` and `./input_test.sh` simulate taps and drags on the real view classes (selector buttons, buttons, tabs, a slider inside a scroll view) and print OK or FAIL. Run them after touching `source/ui/views`.
