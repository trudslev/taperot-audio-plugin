# TapeRot GUI — delta v1.0.7

Program header buttons move out of the plate and into sprites. Knob strips, icons and every other sprite are unchanged.

## Changed

| File | Size |
|---|---|
| `assets/1x/panel_background.png` / `assets/2x/panel_background_2x.png` | 1336 × 679 / 2672 × 1358 |
| `assets/1x/btn_save_on.png`, `btn_save_off.png` | 82 × 46 |
| `assets/1x/btn_delete_on.png`, `btn_delete_off.png` | 82 × 46 |
| `assets/1x/btn_cancel.png` | 82 × 46 |
| `assets/2x/…_2x.png` | 164 × 92 |
| `TapeRot-GUI-Spec.md` | §4 gains a Program header section |

## Both frames are now empty in the plate

SAVE and DELETE are gone from the panel bitmap — the header band runs clean through both rects. Nothing else in the header moved: the LCD frame, its divider and chevron, and the IN/OUT meter frames are pixel-identical to v1.0.6.

| Button | Plate rect (1×) | Sprite top-left (1×) | Sprite |
|---|---|---|---|
| SAVE | 897.0, 47.0, 76 × 40 | **894.0, 44.0** | 82 × 46 |
| DELETE / CANCEL | 983.0, 47.0, 76 × 40 | **980.0, 44.0** | 82 × 46 |

Sprite top-left includes the 3 px transparent bleed carrying the drop shadow, so they blit with no further offset — same convention as §3.

Your measured rects came out 71.5 × 39.5 at x 899.0 / 985.5; those are the fill inside the 1 px border and the antialiased edge. The figures above are the full border-box, which is what the sprite covers.

## Five sprites

| Sprite | State |
|---|---|
| `btn_save_on` | SAVE enabled |
| `btn_save_off` | SAVE disabled |
| `btn_delete_on` | DELETE enabled |
| `btn_delete_off` | DELETE disabled |
| `btn_cancel` | replaces DELETE during name entry |

Enabled and disabled are the two treatments already on the panel, so the header keeps its existing vocabulary: enabled is the raised cream plate — `#EFE6D0 → #DBD0B4`, border `#8E8471`, 1 px white top highlight, `0 2px 3px rgba(0,0,0,.4)` shadow, label `#26221B`; disabled is the recessed dark plate — `#2A2721`, border `#3E382F`, inset top shade, label `#8F8574`. Both are drawn from the same source the plate was, so the enabled sprite lands on the pixels the baked SAVE occupied.

The slight difference from the colours you sampled (`#E6DDC5 → #D9CEB2`, ink `#29241C`) is the shadow and the header band showing through the antialiased edge in the composite; the sprite itself carries the source values above.

**CANCEL is neutral**, taking the enabled treatment with different lettering — your instinct was right. It is an escape hatch, and giving it an alert colour would put two loaded buttons side by side and dilute the one that actually destroys something. DELETE stays the only weighted control in the header.

Do not re-import: knob strips, tick placement, LCD divider and chevron, scope legend rows, frame counts, sheet layout, icons.
