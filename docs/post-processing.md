# Post-processing

Post-processing works on the finished 3D scene before the interface is drawn on
top of it. It adds two things to the Classic look:

- **Bloom**: spells, fires, torches and portals spread a soft halo around
  themselves. Only effects drawn as light (additive blending) glow; bright
  surfaces such as sky or snow do not. Characters - players, monsters, NPCs,
  their equipment, wings, pets and mounts - and items on the ground never glow:
  they look exactly as without post-processing, however shiny their gear. The
  halo is screen-blended, so it lights dark surroundings without washing bright
  ones out to white, and it stays close to its effect.
- **Colour grading**: a map can carry its own colour mood through a lookup
  table. A map without a table is not graded. A Classic table never makes anything
  brighter and leaves highlights (armour, glows, light pools) untouched; it
  cools and deepens shadows. The Remaster client has its own, livelier tables
  for every map ("Remaster tone" below).

The HUD, windows, text, the cursor and item previews in windows are never
processed: they look exactly as without post-processing.

Post-processing is part of the Enhanced look and off in Classic
([enhanced-look.md](enhanced-look.md)). With it on, a bloom strength of 0 and
no grading table, the frame looks the same as with it off.

## Settings

The `Look` preset switches post-processing with the rest of the Enhanced look,
and the options window's "Bloom and colour grading" row switches it off on its
own. Its strengths are values of the look profile, `Data/Render/Look.json`:
`bloom.strength` (0 to 2, shipped 1.0, 0.3 in Icarus), `bloom.levelFalloff`
(0.25 to 1, how wide the halo spreads) and `grading.strength` (0 to 1, how much
of a map's table applies). A grading strength of 0 applies no grading at all
while the bloom stays on.

Players tune the three under that row (Bloom intensity, Bloom spread, Colour
grading strength, the last in percent); see "Tuning it yourself" in
[enhanced-look.md](enhanced-look.md). A value a player changed applies on every
map in place of the map's value until it is reset.

If the graphics device cannot build post-processing, the Enhanced look logs a
warning and the frame stays unprocessed.

Developer builds (control socket or DevEditor) have, for the session only:
`$post on|off` (post-processing alone, cleared by choosing a look),
`$bloom <value>` and `$bloom falloff <value>` (the look profile's values), and
for diagnosis `$post view source` (only what feeds the bloom: the additive
effects, black elsewhere), `$post view bloom` (the blurred glow) and
`$post view scene` (back). The views need post-processing on and a bloom
strength above 0.

## Grading tables

A map's table is `Data/Grading/World<N>.png`, where `N` is the map number plus
one (Lorencia, map 0, is `World1.png`; Dungeon is `World2.png`). The client
loads it when the map is entered and logs which table it uses, or that the map
has none. A file that cannot be used is logged with the reason and the map is
not graded.

A table is a 1024×32 PNG: a 32×32×32 colour cube cut into 32 slices of 32×32
placed side by side. Inside a slice red runs left to right and green top to
bottom; the slice number is blue. Each entry holds the colour that the colour
at its position becomes.

`scripts/grading_table.py identity <file>` writes the table that changes
nothing. To make a new table, paste that strip into a screenshot of the map,
grade the whole image in an image editor, and cut the strip out again. A
table must never brighten a colour and must leave colours above 60 %
brightness as they are; `scripts/grading_table.py` refuses to write one that
does. Strong darkening belongs to the map's lighting, not its grading.

The shipped tables come from `scripts/grading_table.py maps src/bin/Data/Grading`
(one preset per map in `MAP_TABLES`, each a shadow tint with a little
saturation or desaturation and deepening; mid-tones and highlights unchanged):

| Map | Files | Mood |
|---|---|---|
| Lorencia | World1 | warm evening shadows, a touch more colour |
| Dungeon | World2 | colder, slightly desaturated, a little deeper |
| Devias | World3 | blue snow-light shadows |
| Noria | World4 | green forest shade |
| Lost Tower | World5 | violet gloom, deeper |
| Atlans | World8 | underwater teal |
| Tarkan | World9 | dusty amber |
| Icarus | World11 | cold thin air |
| Aida | World34 | deep emerald |
| Kanturu 1–3 | World38–40 | cold steel, desaturated |
| Swamp of Quiet | World57 | murky olive |
| Raklion, its boss map | World58–59 | icy blue |

### Remaster tone

With HD textures on (the Remaster client) the client first looks in
`Data/Grading/Remaster`: the map's `World<N>.png`, else `Default.png`; only
when neither loads does it use the Classic table above. These tables are the
map's Classic mood followed by the Remaster tone (lighter mid-tones, an S
curve, 18 % more colour, warm light and cool shadows: variant "3 rich" picked
by ghinzuro on 2026-11-12, then made weaker on every map; Devias lifts its
mid-tones less), so unlike the Classic tables they may brighten and
touch highlights. They come from
`scripts/grading_table.py remaster src/bin/Data/Grading/Remaster`; per-map
numbers go in `REMASTER_TONES`. The browser build never has HD textures and
never reads them. Like every grading table, the tone shows only while
the look's post-processing ("Bloom and colour grading") is on.

## Screen filters

Two player settings filter the finished scene in the same pass, before the
bloom and the grading: **FXAA** (`[Render] Fxaa`, smooths jagged edges in one
cheap pass, meant for when multisampling is off) and **sharpening**
(`[Render] Sharpen`, 0 to 100 %, contrast-adaptive: flat areas and full-contrast
edges are left alone, so it does not ring or add grain). Where FXAA smoothed an
edge the pixel is not sharpened. FXAA is skipped while multisampling is on (2x or
more): MSAA already smooths the edges, and FXAA on top only blurred texture
detail. The FXAA setting is kept and works again once anti-aliasing is off. They are chosen by the graphics preset or one
by one in the options window, and work in both looks: with either on and the
look's post-processing off (Classic, or the row switched off), the pass runs
for the filters only, with no bloom and no grading. The interface is never
filtered, and the diagnostic `$post view` images are not either.
