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
