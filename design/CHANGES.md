# TapeRot GUI — delta v1.0.4

Supersedes the corresponding files in v1.0.3. Knob sweep and tick placement, shipped together. No coordinate, frame count, sprite size, sheet layout or icon changes. The v1.0.3 LCD divider/chevron placement and the v1.0.2 cleared legend rows are carried through untouched.

## Changed

| File | Size |
|---|---|
| `assets/1x/knob_large.png` / `assets/2x/knob_large_2x.png` | 90 × 11520 / 180 × 23040 |
| `assets/1x/knob_small.png` / `assets/2x/knob_small_2x.png` | 52 × 6656 / 104 × 13312 |
| `assets/1x/knob_model.png` / `assets/2x/knob_model_2x.png` | 90 × 810 / 180 × 1620 |
| `assets/1x/panel_background.png` / `assets/2x/panel_background_2x.png` | 1336 × 679 / 2672 × 1358 |
| `TapeRot-GUI-Spec.md` | §3 corrected |

### 1. Sweep is now ±120°

Both continuous strips re-exported at −120° → +120°, still 128 frames, frame 0 = minimum. Frame size, cap artwork, bleed and frame order are unchanged; only the needle angle differs. You were right that the strips were baked to the spec line rather than the artwork.

**`knob_model` is included, and this is deliberate.** Its nine tick marks are drawn by the same routine that draws every other scale, so they moved from a 33.75° grid to a 30° grid when the sweep changed. Leaving the model strip at ±135° would have introduced exactly the misalignment being fixed elsewhere. Frame count, frame size and order are unchanged — the nine needle angles are now −120, −90, −60, −30, 0, 30, 60, 90, 120.

### 2. Non-linear ticks repositioned

Ticks moved on DRIVE, FLUTTER, LP, HP and RAMP; WOW, NOISE, FAILURE, MIX and OUTPUT are untouched. Minor ticks sit halfway in **rotation** between neighbouring majors, as specified.

DRIVE and FLUTTER take the 0 / 1 / 5 / 20 / 50 / 100 set — the spread reads as a proper log-tapered control, and the crowding of the 0/25/50/75/100 alternative was not worth keeping round numbers for.

| Knob | Marks at |
|---|---|
| DRIVE, FLUTTER | 0 → −120.0°, 1 → −24.5°, 5 → +11.8°, 20 → +53.9°, 50 → +88.9°, 100 → +120.0° |
| LP | 1 → −120.0°, 2 → −20.8°, 5 → +30.4°, 10 → +71.8°, 20 → +120.0° |
| HP | 20 → −120.0°, 50 → −51.7°, 200 → −3.1°, 800 → +61.5°, 2000 → +120.0° |
| RAMP | 0.05 → −120.0°, 0.2 → −55.1°, 0.5 → −19.3°, 1 → +15.7°, 4 → +120.0° |

Measured off the rendered plate before export: all eleven scales land within 0.05° of the table above.

### 3. Scale numerals moved off their ticks

The printed numerals sat tight against their tick marks on nearly every scale. The numeral ring radius goes from `D/2 + 17` to `D/2 + 20`, with a few marks pushed further where the glyph width still crowded the tick:

| Scale | Numeral ring |
|---|---|
| All knobs, default | D/2 + 20 |
| Large knobs, last mark (`100`, `+24`) | D/2 + 22 |
| HP `800` | D/2 + 22 |
| RAMP `0.05` | D/2 + 23 |
| HP `2000` | D/2 + 24 |

Tick marks themselves did not move — angles are exactly the table in §2.

The knob's bounding box is now pinned to the old radius rather than derived from the numeral ring, so the extra pixels do not grow the plate: it is still 1336 × 679 / 2672 × 1358 and all eleven knob centres are where v1.0.3 put them. Verified after re-bake.

### 4. Spec §3

Corrected to ±120°, with the skew curve and its five affected parameters written in beside it, so a future re-export can't reintroduce the ±135° figure.
