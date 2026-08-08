# TapeRot GUI — delta v1.0.1

Supersedes the corresponding files in the v1.0 handoff. Nothing else changes: knob strips, buttons, lamps, LEDs, icon set and all spec coordinates are unaffected.

## Changed

| File | Size |
|---|---|
| `assets/1x/panel_background.png` | 1336 × 679 |
| `assets/2x/panel_background_2x.png` | 2672 × 1358 |

**What moved:** the PROGRAM LCD's dropdown indicator. It was a solid ▼ glyph; it is now a thin stroked chevron — two 1.4 px strokes at 45°, `#C9BC9D`, in a 16 × 16 box at the right end of the LCD, 14 px in from the frame's inner edge.

Baked into the panel bitmap as before — no runtime drawing, no new asset, no coordinate change for any other element.
