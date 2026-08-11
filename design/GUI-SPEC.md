# TapeRot — GUI asset handoff

MT-77 · v1.0 · panel and icon approved 8 Aug 2026

**This file is the whole build contract.** Nothing else in the bundle needs reading to build the GUI: the `.dc.html` files are working references, `plate/buttons/` holds reference renders called out from §3, and `delta/CHANGES.md` (outside this bundle) lists only what moved since the last cut.

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
| PROGRAM LCD | 416.2 | 63 | 470 | 34 |
| Scope well (drawable area) | 43 | 158.5 | 1250 | 70 |
| MODEL readout | 508.5 | 476.5 | 134 | 27 |
| IN meter | 1132.4 | 63 | 80.8 | 34 |
| OUT meter | 1225.2 | 63 | 80.8 | 34 |
| SAVE / STORE button | 896.2 | 63 | 76 | 34 |
| DELETE / CANCEL button | 982.2 | 63 | 76 | 34 |

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

Tick arc radius is 46 (large) / 27 (small) from the dial centre. The needle reaches r 36 (large) / 18.5 (small) — 92 % of cap radius — so the eye can carry a straight line from pointer to mark.

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

### Program header — dual-legend backlit buttons

Each of the two header buttons carries **two permanently printed legends, stacked** — SAVE above STORE, DELETE above CANCEL. Top is the resting function, bottom is what the button becomes while a Program is being named. **The face never changes**; only the backlight behind each legend does. There is no disabled face and no relabelling.

**Dimensions.** Plate **76 × 34** — the suite-wide header height. At 10 px Helvetica Bold the cap height is 7 px, and the legends are placed by cap row rather than by line box: cap rows **10–16** and **23–29** of the sprite, 13 px cap-top to cap-top (3 px leading between the 10 px line boxes). That is a 20 px printed block with 7 px of clear plate above the first cap and 7 px below the second — 34 px exactly. Legend size is unchanged at 10 px; both are functional text and neither is available as the place to save the six pixels. Widest legend, CANCEL at 10 px + 2 px tracking, measures 55 px inside a 68 px usable width.

| Button | Sprite x | Sprite y |
|---|---|---|
| SAVE / STORE | 893.2 | 60.0 |
| DELETE / CANCEL | 979.2 | 60.0 |

**Face** (identical in every state): vertical `#35312A → #26231C`, border `#6B6254`, 1 px `rgba(255,255,255,.10)` inner top highlight, drop shadow `0 2px 3px rgba(0,0,0,.45)`, corner radius 4, 3 px bleed → sprite **82 × 40**.

**Legends:** Helvetica Bold **10 px**, +2 px tracking, centred. Neither legend goes below 10 px — both are functional text.

| Legend state | Value | Treatment | Contrast vs face |
|---|---|---|---|
| Lit | `#F6F1E4` | warm three-layer bloom: `0 0 1.5px rgba(255,238,208,.42)`, `0 0 4px rgba(255,216,154,.26)`, `0 0 8px rgba(255,198,128,.14)` | **11.5:1** top of gradient, 13.9:1 bottom |
| Unlit | `#877F72` | flat, matte, no bloom whatsoever | **3.27:1** top of gradient, 3.96:1 bottom |

Lit is a neutral bright, deliberately **not** the accent `#F0A94B` — the accent stays reserved for the scope trace, LCD and lamps. The *bloom* is warm (unsaturated amber-white, well off the accent hue) so the legend reads as genuinely backlit rather than as a heavier weight of the same ink: the two states differ in **kind** — luminous vs matte — not only in degree. The three layers give the falloff a real lamp has; a single flat shadow reads as a blur. The bloom is a **halo only** — it must not thicken the letterforms. In the exported sprites the lit glyph core measures within ~10 % of the same glyph drawn with no bloom at all, and the counters of S, A and E stay open; if a re-cut closes them, the bloom is compositing over the ink instead of around it. When baking, draw each bloom pass as shadow alone (glyph off-canvas, shadow offset onto the plate) and the ink once on top: stacking opaque passes compounds both halo alpha and glyph coverage, and radii lifted verbatim from CSS are stronger in canvas than in the browser. Unlit carries no bloom at all. Unlit is measured at the *worst case* end of the face gradient and clears the 3:1 state floor there; lit against unlit is 3.51:1, so which legend is live is legible as a difference and not only as a brightness.

**Frame layout.** Two strips, `btn_save_strip.png` and `btn_delete_strip.png` — frame **82 × 40** (2x: 164 × 80), strip **82 × 120** (2x: 164 × 240). Frame order is fixed and must not be reordered.

| Frame | 1x y | 2x y | Backlight | SAVE button state | DELETE button state |
|---|---|---|---|---|---|
| 0 | 0 | 0 | both dark | nothing to do — Program unmodified | nothing to do — Factory Program or INIT |
| 1 | 40 | 80 | top lit | SAVE live — saves a new Program | DELETE live — a User Program is selected |
| 2 | 80 | 160 | bottom lit | STORE live — commits the typed name | CANCEL live — abandons name entry |

Individual sprites carry the same three faces per button:

| Sprite | Face |
|---|---|
| `btn_save_off.png` / `btn_delete_off.png` | frame 0 — both legends dark |
| `btn_save_on.png` / `btn_delete_on.png` | frame 1 — top legend lit |
| `btn_save_store.png` / `btn_cancel.png` | frame 2 — bottom legend lit |

Both legends lit is not a state and is not exported: the two functions are mutually exclusive, so a fourth face would only ever indicate a bug.

Frame 0 is inert in code as well as in appearance — a button showing both legends dark must not act when clicked, or the backlight is claiming something the control contradicts.

### Program header — state matrix

Five panel states, and the two buttons are not independent — read the row, not the buttons. `lit` is the backlit legend; every other legend on that button is dark.

| Panel state | SAVE | STORE | DELETE | CANCEL | SAVE frame | DELETE frame |
|---|---|---|---|---|---|---|
| Factory Program, unmodified | dark | dark | dark | dark | 0 | 0 |
| Factory Program, edited | **lit** | dark | dark | dark | 1 | 0 |
| User Program, unmodified | dark | dark | **lit** | dark | 0 | 1 |
| User Program, edited | **lit** | dark | **lit** | dark | 1 | 1 |
| Naming a Program | dark | **lit** | dark | **lit** | 2 | 2 |

Rules the lighting alone does not imply:

- **Escape out of naming leaves the edited state set**, because nothing was stored — the panel returns to whichever row it came from, so SAVE comes back lit. It does not fall back to "unmodified".
- **Naming overrides both resting legends.** While naming, SAVE and DELETE are dark even on an edited User Program: nothing can be saved or deleted until the name is committed or abandoned.
- **INIT counts as a Factory Program** for row purposes — nothing to delete, so DELETE stays dark however edited it is.
- **A dark legend is an inert control**, not a hidden one (see below).

Both legends lit on one button is not a state and no such face is exported; a fourth face could only ever indicate a bug.

### Program header — reference renders

The header row moves as one band, so the pair is rendered **together**, at 3×, one file per matrix row. Diff these, not per-face crops.

| File | Matrix row |
|---|---|
| `plate/buttons/01-rest-nothing-to-do.png` | Factory Program, unmodified |
| `plate/buttons/02-factory-edited-save.png` | Factory Program, edited |
| `plate/buttons/03-user-program-delete.png` | User Program, unmodified |
| `plate/buttons/04-user-edited-both.png` | User Program, edited |
| `plate/buttons/05-naming-store-cancel.png` | Naming a Program |

Each is 504 × 120 (168 × 40 at 3×) on the fascia gradient, pitch 86 px — reference only, not a shippable asset.

### What stays runtime, and why

Baked into sprites: **both legends and their backlighting**. The legend text is fixed for the life of the panel and only three faces per button exist, so baking costs nothing and guarantees the bloom matches the approved cut — a runtime `text-shadow` re-derives the halo from CSS radii, which render stronger in canvas than in the browser (see the bloom note above) and drift per platform.

Drawn live: **LCD contents** (Program index, name, dirty marker), **meter fills**, **scope trace**. Those are data, not lighting — baking any of them would freeze one moment's reading into the bitmap.

**Per-casting choice, resolved.** BRAND.md allows either a backlit legend or a discrete lamp beside a printed one. TapeRot's fascia is a mid-warm grey, light enough that a small lamp beside the legend would read as a scuff at 10 px, so **the legend itself lights** and there is no lamp beside it. **Do not mix the two forms on this panel** — the exclusive-select group buttons in §3 keep their lamps because they carry no second legend, and that is the only place lamps appear.

**On the removed disabled face.** The greyed-out plate is gone. A lamp behind the legend going out is what rack hardware does, and it is what BRAND.md's LED rule already prescribed — the disabled face was the divergence. Both legends dark reads as "nothing to do here" while the button is still plainly a button, since the face is unchanged and both legends stay above the state floor.

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

### Header row height and the LCD character budget

Every element in the header row shares one height: **34 px** — PROGRAM LCD, SAVE, DELETE, IN meter, OUT meter. TapeRot came down from 40; widths are unchanged.

| Element | Final size | Type |
|---|---|---|
| PROGRAM LCD | 470 × 34 (border-box) | Share Tech Mono 18 px, +2 px |
| SAVE / STORE | 76 × 34 | Helvetica Bold 10 px, +2 px, two stacked legends |
| DELETE / CANCEL | 76 × 34 | Helvetica Bold 10 px, +2 px, two stacked legends |
| IN meter | 80.8 × 34 (border-box) | Share Tech Mono 19 px |
| OUT meter | 80.8 × 34 (border-box) | Share Tech Mono 19 px |

**No type shrank.** Six pixels came out of padding, not font size. The 18 px LCD face in a 34 px cell has 6.6 px of clear space above and below the line box; the 19 px meter numerals sit in 31.2 px of interior. Both were checked at 34 before anything was resized.

**Character budget — measured at 18 px, not estimated.** Share Tech Mono advances **11.72 px** per character at 18 px with +2 px tracking. The 470 px cell spends 2.8 px on its bezel, 20 px on padding, 46.9 px on the `FACT`/`USER`/`NAME` chip plus 6 px of chip padding, 20 px on two 10 px gaps, 1 px on the divider and 19 px on the chevron — leaving **354.3 px** of text width.

354.3 ÷ 11.72 = **30 characters**. The budget is 30, above the required 27, so the 25-character cap on user Program names (27 less the two-digit index and the dirty marker) is unchanged and no cap contracts. Had it fallen short, the cell would have widened rather than the face shrinking or the cap dropping.

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
| Header button plate | `#35312A → #26231C`, border `#6B6254`; legend lit `#F6F1E4`, unlit `#877F72` |
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
               btn_*_on|off.png (18) + btn_save_on|off|store.png, btn_delete_on|off.png,
               btn_cancel.png, btn_save_strip.png, btn_delete_strip.png
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
