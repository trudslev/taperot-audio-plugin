# TapeRot GUI — delta v1.0.6

Pointer reach only. Three filmstrips re-exported; **the plate is unchanged** and needs no re-import.

## Changed

| File | Size |
|---|---|
| `assets/1x/knob_large.png` / `assets/2x/knob_large_2x.png` | 90 × 11520 / 180 × 23040 |
| `assets/1x/knob_small.png` / `assets/2x/knob_small_2x.png` | 52 × 6656 / 104 × 13312 |
| `assets/1x/knob_model.png` / `assets/2x/knob_model_2x.png` | 90 × 810 / 180 × 1620 |
| `TapeRot-GUI-Spec.md` | one line added to §2 |

## Longer needle

The needle now runs to **0.462 × cap diameter** from the dial centre, one factor applied to all three strips:

| Strip | Tip was | Tip now | Cap edge | Reach |
|---|---|---|---|---|
| `knob_large`, `knob_model` | r 30.8 | **r 36.0** | r 39.0 | 92 % |
| `knob_small` | r 15.8 | **r 18.0** | r 20.0 | 90 % |

Measured off the exported PNGs, not the source. The gap between tip and the start of the tick arc closes from 11.7 px to 6.5 px on the large caps and from 7.7 px to 5.5 px on the small ones, and the tip now clears the cap's dark ring rather than stopping behind it.

Inner end, taper, width (3.2 px), corner radius and colour (`#2B251C`) are untouched, as are sweep, frame count, frame size, sheet layout, bleed, cap face artwork and ring weight. Frame 0 is still minimum.

### Why the needle and not the tick arc

Both close the gap, and the tick arc is the more invasive of the two: moving it inward is a plate re-export, which re-opens the coordinate surface we have just spent three deltas settling, and it would crowd the printed numerals back toward the marks after v1.0.4 spent effort pushing them apart. Extending the needle changes three sprites and nothing else. It also suits the fascia better — the long thin needle against a wide scale is the vintage-meter idiom the panel is drawn in, and a short needle on a broad cap is what made it read as an unmarked knob in the first place.

### On the v1.0.4 sweep re-export

Noted, and it matches what I found from the other end: the ±135° plate and ±135° strips were consistent, and so are today's ±120° pair. Both plates were right; the offset centre made each of them look wrong in a different way. The one thing worth keeping from that round is that `knob_model` has to be re-exported alongside the other two whenever the cap changes — as it was here — because nothing else keeps its nine frames in step.
