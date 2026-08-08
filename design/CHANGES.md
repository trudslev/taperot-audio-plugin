# TapeRot GUI — delta v1.0.5

**No bitmaps changed.** The plate and all three filmstrips in v1.0.4 are correct as shipped — do not re-export them. This delta corrects two numbers in the spec.

## Changed

| File |
|---|
| `TapeRot-GUI-Spec.md` — §2 table |
| `Handoff Assembly.dc.html` (+ `support.js`) — reference build |

## The scales are at ±120°. The centre I gave you was wrong.

The mismatch is real, but it is not in the plate. The "Cap centre" column in §2 has been 7.27 px too low for every knob since the original handoff, and the sprite y derived from it is 7.27 px too low as well. Measuring tick angles about that point compresses them symmetrically toward vertical — which is exactly the ±106° you measured, and the ±111° I measured before I found this.

The arithmetic, for a large knob (tick radius 46, offset 7.27):

```
apparent = atan2(46·sin 120°, 46·cos 120° + 7.27) = 111.5°
```

and for a small knob (radius 27):

```
apparent = atan2(27·sin 120°, 27·cos 120° + 7.27) = 105.1°
```

Those are the two figures in your report, reproduced from the offset alone. Re-measuring the v1.0.4 export about the corrected centres gives every major mark within 0.6° of the required table — residual is pixel-centroid noise at that radius, not geometry:

| Knob | Measured on the v1.0.4 export |
|---|---|
| WOW | −119.3 / −59.6 / −0.3 / +59.4 / +119.8 |
| MIX, OUTPUT | −119.0 / −59.7 / −0.7 / +59.2 / +119.9 |
| DRIVE, FLUTTER | −119.4 / −24.1 / +11.8 / +53.4 / +88.2 / +119.6 |
| MODEL | −118.8 / −89.4 / −59.9 / −30.6 / −1.2 / +28.8 / +59.0 / +89.4 / +120.2 |
| LP | −119.0 / −21.6 / +29.5 / +71.5 / +120.7 |
| HP | −119.0 / −52.1 / −3.2 / +61.0 / +120.7 |
| RAMP | −119.0 / −55.2 / −20.0 / +15.1 / +120.7 |

### Root cause

The knob is a two-part element: a square dial box with the control name beneath it. I measured the centre of the whole element instead of the dial box, and the name below pulled it down by half its height — 7.27 px. The v1.0.3 → v1.0.4 shift you saw (±60.0° → ±52.8°) is the numeral ring change from §3 of that delta making the element taller still, moving that same wrong centre further down. The ticks themselves never moved.

### Corrected §2

Sprite **x** was always right; only **y** changes, by −7.27 px, for all eleven knobs. The column is renamed **Dial centre** to make clear it is the tick-arc centre and the needle pivot.

| Control | Sprite y was | Sprite y now | Dial centre now |
|---|---|---|---|
| DRIVE, WOW, FLUTTER, MODEL, NOISE, FAILURE, MIX, OUTPUT | 348.3 | **341.0** | y **386.0** |
| LP, RAMP, HP | 512.8 | **505.6** | y **531.6** |

This is worth acting on beyond the measurement: at the old y the build blits every knob cap 7.27 px below the centre of its own tick arc. Tick arc radius is 46 (large) / 27 (small) from the dial centre, now stated in §2.

Unchanged: plate size, all three filmstrips, every x coordinate, the LCD divider and chevron, the cleared legend rows, frame counts, sprite sizes, sheet layout, icons.
