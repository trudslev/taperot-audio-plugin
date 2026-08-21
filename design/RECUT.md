# RE-CUT SHEET — TAPEROT MT-77

**Every row carries delivered *and* target dimensions.** A target dimension read without its
base is how three figures went wrong in this round: a needle height taken from a placement
offset, a plate "already 3×" of a canvas that no longer exists, a sprite "2×" against an old
frame. All three were true ratios with the base left out. This sheet exists so the target never
travels without it — `../MANIFEST.md` has the same rows for the whole suite.

| File | Drawn at 1× | Delivered | Ratio | **Target (3×)** |
|---|---|---|---|---|
| `assets/taperot-wordmark.png` — **CUT, this bundle** | **231.2 × 50** | **694 × 150** | 3× | **694 × 150** |

**A new cut, not a re-cut.** The 1× box is the rendered Dymo plate measured on the panel,
including its 2 × 18 padding, and it is the **rotated** box: 230 × 44 unrotated turns into
231.2 × 50 at −1.5°. Cut rotated, as drawn — the strip is rendered flat at 3× and rotated on the
canvas, so the letterforms are not resampled twice and the corners stay transparent.

**Cut with Impact Label Reversed, as drawn.** The face was loaded when the cut was taken
(`document.fonts.check("40px 'Impact Label'")` → true), so these are the real letterforms and not
a Barlow Condensed fallback — which is the failure this row was held open to avoid, because a
fallback looks like a wordmark rather than like an error.

**The binary stays out of the bundle: absent by licensing, not missing.** Impact Label Reversed is
donationware and cannot be embedded in a shipping binary, so the letterforms ship as artwork and
the font does not. `fonts/` therefore carries no file for it **by decision**, and that decision is
stated here and in `GUI-SPEC.md` §9 so the gap cannot be read as an oversight and "fixed" by
substituting a face — which would move every measurement taken from the nameplate.

Nothing else on this panel is baked.


---

## The prototype places this artwork now (export 9)

`designs/TapeRot MT-77 Panel.dc.html` drew its nameplate in Impact Label Reversed with a CSS plate
behind it. That face is absent by decision and no bundle may carry it, so the declaration never
resolved and the nameplate rendered on the Barlow Condensed fallback — the one artefact in the
delivery that looked right and could not be measured.

**It now places `assets/taperot-wordmark.png`** at drawn **231.2 × 50** in the nameplate zone at
(22, 14), from `designs/assets/taperot/`. The artwork carries the plate, the emboss and the −1.5°
tilt, so the CSS plate, its 2 × 18 padding and its `rotate(-1.5deg)` were removed with the `<span>`;
the plate's `drop-shadow(0 1px 2px rgba(0,0,0,.6))` is kept in CSS. The `@font-face` rule is gone.

**This makes the artwork the source of record for the wordmark in both directions** — the build
embeds it and the prototype renders it, so a side-by-side compares like with like. A re-cut of this
file changes what both show.


---

## §3.2's WOW ring, re-legended in the prototype (export 10)

**No artwork, no pivot, no range — five numerals.** All **44 printed numerals** on the ten rings
were checked against the `NormalisableRange` that positions each pointer. **Forty-one agree
exactly**, including the three logarithmic rings (LP's stated .5090 for 3 kHz computes to
.508969, HP's .4871 for 200 Hz to .487098, RAMP's .4194 for 0.5 s to .419363) and DRIVE and
FLUTTER's shared skew-0.2 legend. Converting all 60 fractions back through the ranges returns
clean round values, so the marks were authored as values and published as derived output — which
is what `BRAND.md`'s rule assumes.

**The three that disagreed were all WOW's.** It had been given the shared `pct` table — five
numerals at five even **angles** — which is correct for NOISE, FAILURE and MIX, whose ranges are
linear. WOW's is **skew 0.3**, so the printed 25 / 50 / 75 pointed at values the pointer reaches
at **0.98 / 9.92 / 38.33 %**. Both endpoints agreed, which is why it was invisible: a ring wrong
only in its interior looks like a ring.

**Ruled 2026-08-21 — the ring prints a decade series, not even fifths:**

| Printed | f | Angle |
|---|---|---|
| **0** | 0 | −135.00° |
| **1** | .2512 | **−67.18°** |
| **10** | .5012 | **+0.32°** |
| **40** | .7598 | **+70.11°** |
| **100** | 1 | +135.00° |

Honestly-placed even fifths were the default and would have bunched clockwise the way DRIVE's
0 / 1 / 5 / 25 / 100 does. These values were chosen instead because their **honest** angles fall
within 3° of even fifths at every mark — a ring that looks regular and is also true, where the
old one only looked it. Verified in the render against NOISE's even-fifths ring at the same
diameter: numeral centres agree to a couple of px.

**WOW's skew of 0.3 is not to be changed.** The exponent is matched to FLUTTER's realised
deviation (WOW's is about 5×) so the same physical condition sits at the same knob position on
both, and five castings' transport feel rests on it.

**A comment in the first cut of this change was itself a hazard and was corrected.** The tuple's
6th element is the **demo pointer fraction**, and WOW's happens to be `0.30` — the same numeral as
its skew. A comment reading "WOW is skew 0.3" directly above that literal would have invited a
future editor to "correct" the pointer. The rationale now sits **inside** the marks array, where
it can only bind to the fractions.

**Unchanged:** DRIVE, FLUTTER, NOISE, FAILURE, MIX, OUTPUT, LP, HP, RAMP — all forty-one verified
numerals — and MODEL's nine detents.

## Three specification calls ruled with it

- **§2 — RAMP is in OUTPUT.** Pivot **1163** is the OUTPUT section's own centre (1006 + 314/2),
  between MIX at 1089 and OUTPUT at 1237, and directly under its heading. §2's contents column had
  listed it under DECAY; the prototype never did, the build followed the artefact, and the column
  is corrected. DECAY carries no Ø56.
- **§5.4 — GENERATION lights 1..GEN**, confirming shipped behaviour. §5.4's own argument for a
  ladder over a knob settles it: a count of five reads as five lit stages, where one lit stage in
  the fifth position reads as a selector and re-implies the interpolation the ladder was chosen to
  avoid. "Lit stage" means the topmost lit one; §7.4's two rows describe the **face** a stage
  wears, not how many wear it.
- **§7.5 — marked *does not apply*.** This casting has no bypass, by decision: the effect is a
  tape path and a bypass would be the tape being out of the machine. The specification is retained
  and marked rather than deleted, the way §6 marks Elmer's absent meter — §9 wants that gap
  visible. The veil stays implemented and driven from `getBypassParameter()`: unreachable, one
  override away, and stated in the code rather than left as a dead branch. Reversing it is a
  processor change, not a panel one.
