# TapeRot GUI — delta v1.0.2

Supersedes the corresponding files in v1.0.1. No sprite, coordinate, frame count or icon changes.

## Changed

| File | Size |
|---|---|
| `assets/1x/panel_background.png` | 1336 × 679 |
| `assets/2x/panel_background_2x.png` | 2672 × 1358 |
| `Handoff Assembly.dc.html` (+ `support.js`) | reference build |

### Scope strip legends removed from the plate

Both legend rows are now blank in the panel bitmap — dark strip fill, bezel and the canvas well between them are pixel-identical to v1.0.1.

| Row | Cleared region (1×) | Cleared region (2×) |
|---|---|---|
| Top | x 43–1293, y 140–154 | x 86–2586, y 280–308 |
| Bottom | x 43–1293, y 233–247 | x 86–2586, y 466–494 |

Cleared in full, static words included — `PITCH DEV ·`, `WOW`, `FLUT`, `GEN`, `FAIL` and `500 ms / DIV` are gone along with the numerals. Draw the whole row at runtime per §6: Share Tech Mono 12 px, +1.3 px tracking, `#E3A65A`.

Baselines for the drawn text sit at **y 151** (top row) and **y 244** (bottom row), 1×. Top row: left-aligned from x 43, right-aligned to x 1293. Bottom row: rate string left-aligned from x 43; `GEN n`, then an 18 px gap, then the FAIL LED and its label, right-aligned to x 1293.

The FAIL LED sprite at (1241.9, 230.3) is unchanged and was never part of the plate.

### Reference build paths

`Handoff Assembly.dc.html` now references `assets/…` rather than `export/assets/…`, so it renders straight from the bundle folder. Keep `support.js` beside it.
