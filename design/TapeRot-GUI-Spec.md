# TapeRot — GUI asset handoff

MT-77 · v1.0 · panel and icon approved 8 Aug 2026

All coordinates are in 1x logical pixels, measured from the **top-left of `panel_background.png`** (the full plate including its 8 px outer bezel). Multiply by 2 for the `2x` set; nothing else changes.

Panel size: **1336 × 679** (1x) / **2672 × 1358** (2x).

Every asset is a PNG with straight (non-premultiplied) alpha. Glow and drop shadow are baked into each sprite, so sprites carry a transparent bleed margin — the placement figures below are the **sprite's** top-left, bleed included, so they can be blitted without further offset.

---

## 1. Panel background

| File | Size | Notes |
|---|---|---|
| `assets/1x/panel_background.png` | 1336 × 679 | fascia, header band, dividers, all printed labels, scale legends and unit marks, Dymo nameplate, footer stamps, corner screws, empty frames/bezels |
| `assets/2x/panel_background_2x.png` | 2672 × 1358 | |

Baked in: every scale legend and tick, positioned exactly as designed — do **not** recompute tick or numeral positions at runtime.

Empty frames included in the plate (contents drawn at runtime, see §6):

| Element | x | y | w | h |
|---|---|---|---|---|
| PROGRAM LCD | 417 | 47 | 470 | 40 |
| Scope well (drawable area) | 43 | 158.5 | 1250 | 70 |
| MODEL readout | 508.5 | 476.5 | 134 | 27 |
| IN meter | 1134 | 47 | 80 | 42 |
| OUT meter | 1226 | 47 | 80 | 42 |

---

## 2. Knob filmstrips

Vertical strips, frame 0 = minimum, last frame = maximum. Frame height = strip width. Sprite is the **cap only** — ticks, numerals, unit and control name are in the panel bitmap.

Two strips cover all ten continuous knobs — the cap art is identical, only the panel legend differs.

- `knob_large.png` — cap ⌀78, frame **90 × 90**, 128 frames → strip **90 × 11520**
- `knob_small.png` — cap ⌀40, frame **52 × 52**, 128 frames → strip **52 × 6656**
- `knob_model.png` — cap ⌀78, frame **90 × 90**, 9 frames → strip **90 × 810**

Frame index = `round(value01 * 127)`. Pointer sweeps **−120° → +120°** (measured from vertical, positive clockwise) — matching the printed scales on the plate.

Rotation is not proportional to value on every control. Five parameters use `rotation01 = ((value − min) / (max − min)) ^ skew`: DRIVE and FLUTTER (skew 0.2), LP and HP (skew 0.3), RAMP (skew 0.4). The remaining continuous controls are linear. The printed ticks are placed under those curves, so the frame index must be derived from `rotation01`, not from the raw value.

Placement is the sprite's top-left. **Dial centre** is the centre of the printed tick arc — the point the needle pivots about, and the point to measure tick angles from.

| Control | Strip | Frames | Frame | Sprite x | Sprite y | Dial centre |
|---|---|---|---|---|---|---|
| DRIVE | `knob_large` | 128 | 90 | 50.5 | 341.0 | 95.5, 386.0 |
| WOW | `knob_large` | 128 | 90 | 205.8 | 341.0 | 250.8, 386.0 |
| FLUTTER | `knob_large` | 128 | 90 | 353.8 | 341.0 | 398.8, 386.0 |
| MODEL | `knob_model` | **9** | 90 | 530.5 | 341.0 | 575.5, 386.0 |
| NOISE | `knob_large` | 128 | 90 | 678.5 | 341.0 | 723.5, 386.0 |
| FAILURE | `knob_large` | 128 | 90 | 864.3 | 341.0 | 909.3, 386.0 |
| MIX | `knob_large` | 128 | 90 | 1038.0 | 341.0 | 1083.0, 386.0 |
| OUTPUT | `knob_large` | 128 | 90 | 1186.0 | 341.0 | 1231.0, 386.0 |
| LP | `knob_small` | 128 | 52 | 1031.0 | 505.6 | 1057.0, 531.6 |
| RAMP | `knob_small` | 128 | 52 | 1131.0 | 505.6 | 1157.0, 531.6 |
| HP | `knob_small` | 128 | 52 | 1231.0 | 505.6 | 1257.0, 531.6 |

Tick arc radius is 46 (large) / 27 (small) from the dial centre.

> **MODEL is 9 frames, not 8.** The shipped tuning has nine machine positions — NONE, REVOX B77, VCR HIFI, VCR LP, CAMCORDER, CASSETTE I, CASSETTE II, DICTAPHONE, TOY. Frame index = model index (0–8), no interpolation. Say the word if the build wants eight and one gets dropped.

### Parameter ranges (for the LCD readouts)

| Control | Range | Print format |
|---|---|---|
| DRIVE, WOW, FLUTTER, NOISE, FAILURE, MIX | 0–100 | `NN %` |
| OUTPUT | −24 → +24 dB, linear | `+N.N dB` |
| LP | 1 → 20 kHz, log | `N.N kHz` |
| RAMP | 0.05 → 4 s, log | `N.NN s` |
| HP | 20 → 2000 Hz, log | `NNN Hz` |

---

## 3. Two-state buttons

Plate 98 × 25 with a 2 px bleed for the drop shadow → sprite **102 × 29**; placement below is the sprite's top-left (plate top-left is +2, +2). Lit state = amber LED plus its glow; the plate itself does not change.

| Button | Files | Sprite x | Sprite y |
|---|---|---|---|
| SWITCHING · FADE | `btn_fade_on.png` / `btn_fade_off.png` | 482.5 | 534.5 |
| SWITCHING · CLUNK | `btn_clunk_on.png` / `_off.png` | 482.5 | 566 |
| NOISE BED · TAPE | `btn_tape_on.png` / `_off.png` | 598.5 | 534.5 |
| NOISE BED · VCR | `btn_vcr_on.png` / `_off.png` | 598.5 | 566 |
| NOISE BED · DUST | `btn_dust_on.png` / `_off.png` | 598.5 | 597.5 |
| HUM · OFF | `btn_off_on.png` / `btn_off_off.png` | 714.5 | 534.5 |
| HUM · ON | `btn_on_on.png` / `btn_on_off.png` | 714.5 | 566 |
| SPREAD · LINKED | `btn_linked_on.png` / `_off.png` | 858.3 | 499.5 |
| SPREAD · STEREO | `btn_stereo_on.png` / `_off.png` | 858.3 | 531 |

Each group is exclusive-select: exactly one member lit at all times.

---

## 4. Lit / unlit pairs

### GENERATION — 8 segments

Segment 20 × 17, 3 px bleed for the glow → sprite **26 × 23**. Two files only, reused for all eight positions.

`gen_seg_on.png` / `gen_seg_off.png` — sprite y **493.5**; sprite x = `224.3 + n × 25` for n = 0…7 (224.3, 249.3, 274.3, 299.3, 324.3, 349.3, 374.3, 399.3).

Segments 1…GEN are lit; the rest unlit. The 1–8 numerals underneath are printed in the panel bitmap.

### FAULT ACTIVITY dots

Lamp ⌀22, 6 px bleed → sprite **34 × 34**. Sprite y **567.5**. All four dots use the shared `lamp_on.png` / `lamp_off.png`.

| Dot | Sprite x |
|---|---|
| DRP | 839.4 |
| SNG | 874.4 |
| CRK | 909.4 |
| WBL | 944.8 |

Flash duration in the design is 260 ms.

### FAIL buttons

Same shared lamp sprite, **34 × 34**, sprite y **490.5** — `lamp_off.png` / `lamp_on.png` / `lamp_press.png`. Momentary: `lamp_press` while the pointer is down, `lamp_on` lit-but-released, `lamp_off` at rest.

| Button | Sprite x |
|---|---|
| STP | 44.5 |
| FLT | 78.8 |
| FAI | 112.8 |

### FAIL indicator (scope strip)

⌀8 LED, 5 px bleed → sprite **18 × 18**, at **1241.9, 230.3**. `led_fail_on.png` / `led_fail_off.png`. Lit while any of STP/FLT/FAI is held.

---

## 5. Product icon — direction D2

| File | Size | Use |
|---|---|---|
| `icon/taperot_icon_1024.png` | 1024 × 1024 | JUCE plugin / standalone app icon |
| `icon/taperot_icon_256.png` | 256 × 256 | |
| `icon/taperot_icon_32.png` | 32 × 32 | browser-list check render |
| `icon/taperot_icon.svg` | vector | editable source |

Rounded container is **baked in**: corner radius 22.37 % of the square (229 px at 1024), so hosts that don't mask get the intended shape. A 1.2 %-width `#C07E23` edge stroke sits inside the corner so the amber field keeps an edge on light backgrounds; verified at 32 px against white, near-black and mid-grey.

Geometry, as a fraction of the square: hub radius 0.36; six notches at 60° spacing starting −90° + 0.32 rad, half-width 0.062, spanning radius 0.245 → 0.372. Notch count and weight are the two knobs to turn if it ever softens at small sizes — do not thin the notches below 0.055.

---

## 6. Runtime-drawn elements

Not exported; draw these over the panel bitmap so they match it.

| Element | Typeface | Size | Colour |
|---|---|---|---|
| PROGRAM LCD text | Share Tech Mono | 18 px, +2 px letterspacing | `#F2B25C`, glow `rgba(240,169,75,.35)` |
| Bank chip (`FACT` / `USER`) | Share Tech Mono | 18 px | `#F2B25C` |
| MODEL readout | Share Tech Mono | 12.5 px, +1.2 px | `#F2B25C` |
| IN / OUT numerals | Share Tech Mono | 19 px | `#EFE7D3` |
| Scope strip legends | Share Tech Mono | 12 px, +1.3 px | `#E3A65A` |
| Scope trace | — | 1.7 px stroke, 5 px halo at 28 % | `#F0A94B`, halo `rgba(217,131,36,.28)` |
| Scope grid | — | 1 px, 8 columns × 4 rows | `rgba(240,169,75,.13)`; centre line dashed 3/4 at `.30` |
| Scope well fill | — | — | `#100E0A` |

LCD behaviour: while a knob is dragged the LCD shows `PARAMETER: value unit` and reverts to the program name 1.1 s after release.

---

## 7. Colour and type

**Accent (icon and panel must stay in step):** `#F0A94B` — icon edge stroke `#C07E23`, lit-segment gradient `#F5B85F → #D98324`.

| Role | Value |
|---|---|
| Fascia | `#EFE6D0 → #E2D8BF` (vertical), outer plate `#E4DAC2` |
| Header band | `#2C2923 → #201D18`, base rule `#0E0C09` |
| Well / LCD interior | `#16130F → #100E0B`, bezel `#4E4740` |
| Printed label (dark on fascia) | `#3A3328`; secondary `#453E31`; footer `#5F5749` |
| Header label | `#EFE7D3`; secondary `#CCC1A6` / `#C4B99F` |
| Knob cap | radial `#F8F2E3 → #EFE7D2 45% → #DED3B8`, rim `#443E36`, pointer `#2B251C` |
| Button plate | `#EAE0C8 → #DBD0B4`, border `#A79B80`, text `#2E2820` |
| LED unlit | `#B3A88C` on fascia, `#4B443A` on dark |
| Lamp unlit | `#3A342C → #17140F`, ring `#6E675A` |
| Divider | 1 px `#B2A68A` + 1 px `rgba(255,255,255,.7)` |

**Typefaces**

- Helvetica Bold / Regular — all printed panel text. Section headers 11 px / +3.6 px tracking; control names 10.5 px bold / +1.7 px; group labels 10 px bold / +2.2 px; footer 10 px regular / +2.4 px.
- Share Tech Mono — every runtime readout (see §6).
- ImpactLabel (embossed Dymo face, `uploads/Impact_label*.ttf`) — nameplate only, already baked into the panel bitmap. Not needed at runtime.

---

## 8. Files

```
export/
  assets/1x/   panel_background.png
               knob_large.png, knob_small.png, knob_model.png
               btn_*_on|off.png (18)
               gen_seg_on|off.png
               lamp_on|off|press.png
               led_fail_on|off.png
  assets/2x/   same set, _2x suffix, all dimensions doubled
  icon/        taperot_icon_1024.png, _256.png, _32.png, taperot_icon.svg
  Handoff Assembly.dc.html + support.js
  TapeRot-GUI-Spec.md
```

Sprites whose art is identical across controls are shipped once and shared — the two knob strips, the ⌀22 lamp, and the ⌀8 LED. The panel bitmap is what distinguishes one control from another.

Open `Handoff Assembly.dc.html` (in the bundle, beside `support.js`) in any browser: it composites these bitmaps at the coordinates above and is the reference build.
