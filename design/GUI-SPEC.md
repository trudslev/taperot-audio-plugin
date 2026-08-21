# TAPEROT — GUI SPEC

Model **MT-77**, tape degradation processor. Neon Foundry casting, harmonisation round.
Authoritative for the build.

**Read `shared/HEADER-PART.md` first.** The block, the band, the LCD cell with its budget and
cap, the Program buttons and their state matrix, and the meter wells are the shared part and
are not restated except where this casting's material meets them.

**Asset format: vector / code-drawn, with one exported bitmap.** Everything on the panel is
drawn at runtime except the **wordmark**, which ships as artwork for a licensing reason
stated in §9 — the face is not distributable, so no font binary accompanies it. Nothing
carrying a live value is baked.

---

## 1 · Canvas

| Figure | Value |
|---|---|
| Canvas | **1340 × 790** at 100 % |
| Fascia | `linear-gradient(180deg, #efe6d0, #e2d8bf)` with an 8 px inner bezel `#e4dac2` and a 1 px `rgba(0,0,0,.10)` line inside it |
| Thumbscrews | Ø11 at (2.5, 20.5), (1326.5, 20.5), (2.5, 758.5), (1326.5, 758.5), each with a 8 × 1.6 slot |
| Header block | 16, 16, 1308 × 104 — shared part, material `linear-gradient(180deg, #2c2923, #201d18)` |
| Wow/flutter scope | 16, 136, 1308 × 164 |
| Section dividers | 2 px at x **168 · 344 · 656 · 826 · 1002**, y 316 → 736 |
| Section headings | y **326** |
| Ø76 / Ø104 control row | pivot y **444**, shared label line y **516** |

**Call 1 cost TapeRot 4 px** (1336 → 1340) — the smallest move in the suite, because its
band was already close to the part's.

---

## 2 · Sections — the signal path, left to right

| Section | x | Width | Contents |
|---|---|---|---|
| INPUT | 20 | 144 | DRIVE Ø76 · FAIL lamp trio |
| MACHINE | 172 | 168 | MODEL Ø104 · machine readout |
| TRANSPORT | 348 | 304 | WOW Ø76 · FLUTTER Ø76 · SWITCHING shoe |
| NOISE | 660 | 162 | NOISE Ø76 · NOISE BED shoe · HUM shoe |
| DECAY | 830 | 168 | FAILURE Ø76 · SPREAD shoe · FAULT ACTIVITY |
| OUTPUT | 1006 | 314 | MIX Ø76 · OUTPUT Ø76 · LP Ø56 · HP Ø56 · **RAMP Ø56** |

**RAMP is in OUTPUT** (pivot 1163 — the section's own centre, 1006 + 314/2), corrected here
2026-08-21; this column previously listed it under DECAY and the prototype never did. The build
followed the artefact and was right to. DECAY carries no Ø56.

**The order is the signal path and it is the reorganisation this round made** — the previous
arrangement grouped by control type. Section headings Barlow Condensed 600
**12 px / line box 15 / .28 em**, `#3a3328`, centred on the column.

---

## 3 · Knobs — three classes

| Class | Ø | Controls | Cap |
|---|---|---|---|
| Signature | **104** | MODEL only | **dark** — `radial-gradient(circle at 34% 24%, #4a4237, #2b2620 46%, #14110d)`, rim `#0e0c09`, pointer `#f2e9d6` |
| Primary | **76** | DRIVE, WOW, FLUTTER, NOISE, FAILURE, MIX, OUTPUT | ivory — `radial-gradient(circle at 34% 24%, #f8f2e3, #efe7d2 45%, #ded3b8)`, rim `#443e36`, pointer `#2b251c` |
| Standard | **56** | LP, HP, RAMP | ivory, same construction |

**TapeRot is one of two castings carrying the signature diameter**, and MODEL earns it: it is
the control the unit is described by, and it is marked by **material as well as size** — the
only dark cap on a cream fascia. That is what keeps Ø104 meaning something.

Sweep 270°, angle = `−135 + 270 f`. Ticks: major **2 × 9** at every numeralled position,
minor **1.5 × 5** at every unnumeralled one, ink `#3a3328`. Numeral ring at `r + 29.5`,
Barlow Condensed 500 **11 px / 13 / .04 em**. Sweep arc a 270° conic wedge
`rgba(58,51,40,.28)` masked to 1.4 px. Pointer 3 × (r − 8).

### 3.1 Registration — already correct, by a different route

The Ø104 and Ø76 controls share pivot y **444** and one label line at y **516**. MODEL's
Ø104 body would put its label 14 px below that line, so **its label is pinned to 516**
rather than derived from its own diameter.

That is the same outcome the suite's registration rule reaches by
`dy = (larger − smaller) / 2` on a shared box — the label registers on something other than
the ring. TapeRot arrived at it independently and is **already conformant**; the pinned
figure is kept because it is measured on this panel, and it should be read as an instance of
the general rule rather than a local trick.

### 3.2 Mark lists

Angle = `−135 + 270 f`. Numerals **bold**; unbolded rows are minor ticks at real values.

**DRIVE and FLUTTER share one legend** — 0–100 % at skew 0.2, five numerals and six minors:

| f | 0 | .2513 | .3466 | **.3981** | .4573 | **.5493** | .6310 | **.7579** | .8706 | .9441 | **1** |
|---|---|---|---|---|---|---|---|---|---|---|---|
| Printed | **0** | · | · | **1** | · | **5** | · | **25** | · | · | **100** |

**The marks bunch heavily toward the clockwise end and that is intended** — it is what a
skew of 0.2 looks like drawn honestly. Thinning the numerals near the top is a reasonable
answer if taken knowingly; five is the primary-class ceiling and this ring is at it.

| Knob | Ø | Marks |
|---|---|---|
| NOISE · FAILURE · MIX | 76 | even fifths — **0 / 25 / 50 / 75 / 100** (ranges are linear) |
| WOW | 76 | **0** (f 0) · **1** (.2512) · **10** (.5012) · **40** (.7598) · **100** (1) — see below |
| OUTPUT | 76 | even fifths — **−24 / −12 / 0 / +12 / +24** dB, leading plus kept |
**WOW does not take the shared even-fifths legend** — its range is skew 0.3, so even angles
would have printed 25 / 50 / 75 at positions the pointer reaches at 0.98 / 9.92 / 38.33 %.
Ruled 2026-08-21: the ring prints a **decade series, 0 / 1 / 10 / 40 / 100**, chosen so the
honestly-placed angles (−135 / −67.18 / +0.32 / +70.11 / +135) fall within 3° of even fifths —
an even-looking ring that is also true, which the old one only looked like. **WOW's skew of
0.3 is not to be changed**; the exponent is matched to FLUTTER's realised deviation and five
castings' transport feel rests on it.

| LP | 56 | **1** (f 0) · **3** (.5090) · **20** (1) kHz, eight minors between |
| HP | 56 | **20** (f 0) · **200** (.4871) · **2k** (1) Hz, eleven minors between |
| RAMP | 56 | **0.05** (f 0) · **0.5** (.4194) · **4** (1) s, ten minors between |

LP, HP and RAMP are logarithmic; **their fractions are the contract and must not be evened
out.** Standard class carries three numerals, and the demoted values keep their ticks — what
is dropped is the numeral, not the mark. Units print inside the arc's bottom gap.

### 3.3 MODEL — nine detents, no numerals

Ø104, **nine major ticks (2 × 9), every one a real detent, none numeralled.** The machine
names print in the **MACHINE readout** (134 × 27 at (189, 566), Share Tech Mono 12.5 / 16,
`#f2b25c` on the LCD material) and not on the fascia. Pointer angle = `−135 + 270 × index / 8`.

**The readout is the label, so the fascia carries no machine names** — nine names printed
around a Ø104 dial would not fit at the type floor, and the readout already has to exist to
show which is loaded. Position names are the enum's; the readout is drawn at runtime.

---

## 4 · Wow/flutter scope

1308 × 164 at (16, 136), `linear-gradient(180deg, #16130f, #100e0a)`, `inset 0 0 0 1px #4e4740`,
`inset 0 3px 9px rgba(0,0,0,.9)`.

| Element | Spec |
|---|---|
| Grid | 1 px `rgba(240,169,75,.13)` — vertical every 163.5, horizontal every 41 |
| Zero line | y 82, dashed `rgba(240,169,75,.30)` 3 on / 4 off |
| Trace | `#f0a94b`, 1.7 px, `drop-shadow(0 0 5px rgba(217,131,36,.55))` — **9.24:1** on the well |
| Readouts | Share Tech Mono 12 / 15 / 1.3 px, `#e3a65a` — **8.70:1** |

Four readouts, one per corner: `PITCH DEV · ±34 cents` · `500 ms / DIV` ·
`WOW 0.50 Hz · FLUT 11.2 Hz` · `GEN <n>` with the **FAIL lamp** and its legend. All live,
all drawn at runtime. The trace is the sum of three sines — wow, flutter and drift — and is
the casting's signature display.

---

## 5 · Switch groups

### 5.1 Shoes — §4B, and this panel does not currently match the part

Three groups are built as shoes, all two-state at the part's **128 × 32 in two 64 halves**:
**SWITCHING** (FADE / CLUNK), **HUM** (OFF / ON) and **SPREAD** (LINKED / STEREO).
**NOISE BED is not a shoe** — see §5.3.

Legends are printed once per position, centred under their own segment, 10 px / 13 / .14 em
`#3a3328`, **never re-inked and never moved** — the shoe carries the state. Engaged segment
`linear-gradient(#f2ebd8, #d8cdb0)` with `inset 0 1px 0 rgba(255,255,255,.9)`; disengaged
`linear-gradient(#241f18, #15120d)`. Group ring `inset 0 0 0 1px #a79b80`.

All three were drawn 6 px short at 128 × 26 and are now **32**, which moves nothing: NOISE
BED at y 548 and HUM at y 622 still clear.

### 5.2 NOISE BED — lamp buttons, under §4B's scope clause

**A three-state control whose section has no room for the 168 × 45 footprint.** The NOISE
section is 162 px wide (x 660 → 822) and the group sits at x 677, so the part does not fit.
Widening the section would move the divider at 826 and with it FAULT ACTIVITY's 176 px group,
on the densest panel in the suite — re-planning a casting's layout to satisfy a footprint,
which inverts the round's scope: the header is the part, body layout is the casting's.

**So it takes the form this panel already uses**: three round lamp buttons, TAPE / VCR /
DUST, Ø26 with the lit lamp in the accent, identical in construction to the fault group in
§5.3. That is TapeRot's own vocabulary rather than a fallback — the same reasoning that made
the shoe right on Gatecrasher and Elmer, where the shoe was already their construction.

**§4B carries a scope clause for this**, because 168 × 45 was derived from a single
three-state control and failed to fit the second one that arrived: the three-state footprint
applies where the section has room for it, and a casting whose own vocabulary already answers
a three-state control may use that instead. TapeRot is the named instance. A figure derived
from one case does not read as a general claim.

### 5.3 Round lamp-buttons

Ø26 dark caps, `radial-gradient(circle at 36% 28%, #4a423a, #2a251e 52%, #15120e)`, each with
an Ø11 lamp in its face and its legend below at 10 px / 13 / .14 em.

| Group | At | Positions |
|---|---|---|
| FAIL | 20, 548 | STP · FLT · FAI |
| NOISE BED | 677, 548 | TAPE · VCR · DUST |
| FAULT ACTIVITY | 826, 622 | DRP · SNG · CRK · WBL |

Lamps: lit `radial-gradient(circle at 38% 30%, #ffd48a, #f0a94b 42%, #b4741d 78%, #6b4310)`
with `inset 0 0 6px 1px rgba(240,169,75,.5)` and a 7 px outer glow; unlit
`radial-gradient(#6a6152, #4b443a 60%, #2a251c)` with no glow. **Light stops at the lens
edge** — no halo on the fascia, and an unlit lamp is a dark lens, not a hole.

### 5.4 GENERATION selector

Eight stages, lit stage in the accent, unlit in the dark lamp material. The selector is a
stage ladder rather than a knob because the parameter is an integer count of tape
generations, and a pointer implies interpolation between them.

**Stages 1..GEN light, not GEN alone** — ruled 2026-08-21, confirming shipped behaviour. The
reason is §5.4's own argument: a count of five reads as five lit stages, where one lit stage in
the fifth position reads as a selector and re-implies the thing the ladder was chosen to avoid.
"Lit stage" above means the topmost lit one. §7.4's two rows describe the FACE a stage wears,
not how many wear it.

---

## 6 · Palette and measured contrast

Computed in one pass from this panel's own hexes against each ground **by name**, worst case
where the ground is a gradient. Functional 7:1, flavour 4.5:1, state 3:1. **Every functional
role on this panel clears its floor** — no ink changed in this pass.

### On fascia (worst `#e2d8bf`)

| Ink | Role | Ratio |
|---|---|---|
| `#3a3328` | section headings, control labels, units, scale numerals, shoe legends, lamp legends, switch captions | **8.79** (10.03 at the light end) |
| `#3a3328` | shoe legends where they sit under an engaged segment's `#d8cdb0` shadow | **7.89** |

### On the header block (worst, i.e. lightest, `#2c2923`)

| Ink | Role | Ratio |
|---|---|---|
| `#efe7d3` | function descriptor | **11.76** |
| `#ccc1a6` | model line, PROGRAM caption, IN / OUT captions | **8.11** |

`#ccc1a6` is the reconciled hex for this role — the six-material header strip carried
`#b0a695`, which measures **6.03** against this block and was **worse than the body it was
meant to have fixed**. The strip had also drawn this block as `#1a1613 → #100d0b`, a
material that does not ship, so its 7.48 was measured against the wrong ground. Both are
corrected; see §9.

### On the Dymo strip (`#100e0c → #1c1815`)

| Ink | Role | Ratio |
|---|---|---|
| `#f4efe3` | wordmark | **15.37** |

### On the LCD and scope well (`#16130f → #100e0b`)

| Ink | Role | Ratio |
|---|---|---|
| `#f2b25c` | program name, bank tag, live readout, chevron, MACHINE readout | **9.96** |
| `#efe7d3` | IN / OUT meter values | **15.03** |
| `#e3a65a` | scope readouts | **8.70** |
| `#f0a94b` | scope trace | 9.24 — graphic |

### Pointers against their caps

| Cap | Pointer | Ratio |
|---|---|---|
| Ivory `#efe7d2` | `#2b251c` | **12.30** |
| Signature `#2b2620` | `#f2e9d6` | **12.42** |

Both far clear of the suite's thinnest pointer separation (4.24), which is what a two-tone
cap scheme buys.

### Momentary buttons

| State | Face | Ink | Ratio |
|---|---|---|---|
| Rest | `#eae0c8 → #dbd0b4` | `#2e2820` | **9.51** |
| Held | `#2a2620 → #191610` | `#f6f1e4` + 7 px bloom | **13.33** |

### Accent

**One accent: `#f0a94b`.** Every lamp, the scope trace and the lit GENERATION stage. The LCD's
`#f2b25c` is the display phosphor, not the accent — it is the LCD material's own ink and
appears only on glass.

---

## 7 · State matrices

### 7.1 Program legends — shared part

| Panel state | SAVE | STORE | DELETE | CANCEL |
|---|---|---|---|---|
| Factory Program, unmodified | idle | idle | idle | idle |
| Factory Program, edited | **lit** | idle | idle | idle |
| User Program, unmodified | idle | idle | **lit** | idle |
| User Program, edited | **lit** | idle | **lit** | idle |
| Naming a Program | idle | **lit** | idle | **lit** |

### 7.2 Shoes

| Group | Positions | Engaged segment |
|---|---|---|
| SWITCHING | FADE · CLUNK | the selected one is pale; the other is dark |
| HUM | OFF · ON | as above |
| SPREAD | LINKED · STEREO | as above |

Six cells, one rule, and **no legend changes in any of them.** NOISE BED's three states are
in §7.3's lamp table.

### 7.3 Fault group and FAIL lamps

| Lamp | Lit when |
|---|---|
| STP | a stop-band failure is active |
| FLT | a flutter failure is active |
| FAI | the failure engine has fired this cycle |
| DRP · SNG · CRK · WBL | that fault category is enabled |
| TAPE · VCR · DUST | that noise bed is selected — exactly one lit |
| FAIL (scope corner) | any failure is currently sounding |

All five fault categories are carried; none was dropped in the reorganisation.

### 7.4 GENERATION

| Stage | Face |
|---|---|
| Selected | accent radial + `0 0 7px 1px rgba(240,169,75,.35)` |
| Unselected | `radial-gradient(#4a423a, #2a251e 55%, #15120e)`, no glow |

### 7.5 Bypass — **DOES NOT APPLY TO THIS CASTING**

**TapeRot has no bypass, by decision** (`Source/PluginProcessor.h`: this effect is a tape path,
and a bypass would be the tape being out of the machine). `getBypassParameter()` returns
nullptr, so the state below is unreachable. Marked here rather than deleted, the way §6 marks
Elmer's absent meter — a call in a spec the casting cannot enter is a gap §9 wants visible. The
decision is **not** reversed. The build's veil stays implemented and driven from
`getBypassParameter()`: it costs nothing, cannot rot, and is one override away if the processor
decision ever changes.

The specification, retained for the round:

Host-driven, no on-panel control. Full-bleed **0.50 `#808080` multiply** over the whole
panel. Pointers do not move, the scope freezes, every lamp goes out, no caption, no
desaturation. The legibility floors do not apply in this state.

---

## 8 · Type

Every size is a CSS px em size with a pinned line box (call 4).

| Role | Face | Size / line box | Tracking | Ink |
|---|---|---|---|---|
| Wordmark | Impact Label Reversed | 40 / 40 | .10 em | `#f4efe3` |
| Function descriptor | Barlow Condensed 600 | 14 / 17 | .26 em | `#efe7d3` |
| Model line | Share Tech Mono | 11 / 14 | .20 em | `#ccc1a6` |
| Section heading | Barlow Condensed 600 | 12 / 15 | .28 em | `#3a3328` |
| Control label | Barlow Condensed 600 | 12 / 15 | .18 em | `#3a3328` |
| Unit | Share Tech Mono | 10 / 13 | .10 em | `#3a3328` |
| Scale numeral | Barlow Condensed 500 | 11 / 13 | .04 em | `#3a3328` |
| Switch caption | Barlow Condensed 600 | 10 / 13 | .22 em | `#3a3328` |
| Shoe / lamp legend | Barlow Condensed 600 | 10 / 13 | .14 em | `#3a3328` |
| Scope readout | Share Tech Mono | 12 / 15 | 1.3 px | `#e3a65a` |
| MACHINE readout | Share Tech Mono | 12.5 / 16 | 1.2 px | `#f2b25c` |
| LCD / meter value | Share Tech Mono | 17 / 22 | .10 em | `#f2b25c` / `#efe7d3` |
| Program legend | Barlow Condensed 600 | 11 / 13 | .12 em | see 7.1 |

**Numerals, units, the model line and both readouts stay in Share Tech Mono** — this
casting's own mono, per call 7's split. Panel lettering is Barlow Condensed 600.

---

## 9 · Conformance and the wordmark exception

**§9 and §10 together account for every call.** A call appearing in neither this section nor
the changelog is a gap by construction, not an omission.

| Call | State |
|---|---|
| **1** — 1340 frame | **already conformed within 4 px**; TapeRot was 1336 and its band was already the part's width. |
| **2** — Share Tech Mono LCD | **already conformed** on face; only the cap moved, and upward (25 → 47). |
| **3's signature class** | **already conformed.** MODEL is the control the unit is described by, at Ø104 with the only dark cap on the panel — material as well as diameter. |
| **4** — size and line box as a pair | **already conformed**; every figure in §8 was already a pair. |
| **5** — code-drawn, cached, no filmstrips | **already conformed** in artwork; the sheets are retired by the call and `setBufferedToImage` is the build's to add. |
| **7** — Barlow Condensed panel lettering | **already conformed**, with the wordmark outside the call as the nameplate metaphor. |
| **Registration** — pivots on one Y, labels on one line | **already conformed** by pinning MODEL's label to y 516 (§3.1) — the general rule reached independently. |
| **Lamps** — light stops at the lens edge, unlit stays a lens | **already conformed** on all eight lamps and the FAIL indicator. |
| **§4B shoes** | **conformant.** The three two-state groups are at the part's 128 × 32; NOISE BED takes the scope clause (§5.2) rather than an exception, and is the clause's named instance. |

### The wordmark's licensing exception

**Impact Label Reversed is donationware and cannot be embedded in a shipping binary**, so the
wordmark **ships as artwork and the font does not** — `fonts/` carries no file for it, and that is
**absent by licensing, not missing**. The wordmark is delivered cut at a **stated path** —
`assets/taperot-wordmark.png` — because a named path is confirmed in one look while an unnamed one
takes exhausting the whole image set to establish: **a stated path matters as much as the file.**
The cut is **694 × 150**, rotated −1.5° as
drawn, with the face checked loaded at cut time so the letterforms are the real ones and not a
Barlow Condensed fallback. An absent font that is
not declared looks like a delivery defect and gets "fixed" by substituting a face, which
moves every measurement taken from the nameplate. The strip is drawn at 40 / 40 / .10 em,
rotated **−1.5°**, on a Dymo-style plate `linear-gradient(#100e0c, #1c1815)` with 2 × 18
padding and a 2 px radius.

---

## 10 · Changelog and outstanding

### This round

1. **Canvas 1336 → 1340** (call 1).
2. **Reorganised to the signal path** — INPUT · MACHINE · TRANSPORT · NOISE · DECAY · OUTPUT,
   replacing a grouping by control type. Dividers at 168 / 344 / 656 / 826 / 1002.
3. **Ø90 → Ø76 primary** (call 3); MODEL kept at **Ø104** as the signature control; LP, HP
   and RAMP to **Ø56** with three numerals each and the demoted values kept as minors.
4. **SWITCHING, HUM and SPREAD rebuilt as §4B shoes** at the part's 128 × 32 (drawn 26 in
   the first pass), replacing a lamp-button stack — the shoe is a shared part and it halves
   the height those groups take in sections that also carry MODEL and NOISE.
   **NOISE BED stayed with lamp buttons** under §4B's scope clause (§5.2): a three-state
   control in a 162 px section, answered by the panel's own vocabulary.
5. **Header replaced by the shared part**; cap 25 → 47.
6. **LCD chevron re-drawn** as the shared 14 × 8 stroked path, replacing a 9 × 9 rotated box.
7. **Model line reconciled to `#ccc1a6`** — the six-material strip had `#b0a695` at 6.03
   against the real block, and had drawn the block in a material that does not ship. The
   body's hex won and the strip was corrected to it.
8. **GENERATION** kept as an eight-stage ladder; all five fault categories retained.

### Outstanding

- Wire both meter wells, the scope and the pitch-deviation readout to real signal; the render
  shows `−6.2` / `−1.4`, `±34 cents` and `WOW 0.50 Hz · FLUT 11.2 Hz` as samples.
- ~~Wordmark re-cut at 3×~~ — **cut, 694 × 150**, rotated −1.5° as drawn, with Impact Label
  Reversed checked loaded at cut time and the font binary absent by licensing (§9).
- Confirm DRIVE / FLUTTER's skew 0.2 and the three log rings against the build's
  `NormalisableRange` before the marks are final.
- **`shared/HEADER-PART.md` revision 3 is pending three build answers** — the meter's display
  clamp, its format at both ends, and the sign convention. Nothing on this panel changes
  either way.
