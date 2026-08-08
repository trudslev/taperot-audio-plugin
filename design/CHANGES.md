# TapeRot GUI — delta v1.0.3

Supersedes the corresponding files in v1.0.2. Panel bitmap only — no sprite, coordinate, frame count or icon changes, and the scope legend rows stay exactly as v1.0.2 shipped them.

## Changed

| File | Size |
|---|---|
| `assets/1x/panel_background.png` | 1336 × 679 |
| `assets/2x/panel_background_2x.png` | 2672 × 1358 |

### PROGRAM LCD marks moved right

Horizontal only — both marks keep their y, stroke weight, angle, colour and 16 × 16 box.

| Mark | Was (1×) | Now (1×) | Now (2×) |
|---|---|---|---|
| Divider rule | x 444 | **x 490.9** | x 981.8 |
| Chevron ink | x 457.5–468.0 | **x 859.3–870.7** | x 1718.6–1741.4 |

The chevron's 16 × 16 box now sits at x 857–873, i.e. 14 px in from the frame's inner edge at 887 — the v1.0.1 description applied to the right edge, as intended.

**Root cause, for the record.** Both marks are laid out in flow between the bank chip and the program name. The bake pass blanked the chip's and name's text outright, so those two boxes collapsed to zero width and the divider and chevron slid left with them. The bake now hides that text while preserving each box's width, so every mark bakes at the position the runtime lays out. This was a plate-generation bug, not a design coordinate — which is why the runtime and the plate disagreed by exactly the width of the collapsed text.

No code change on receipt.
