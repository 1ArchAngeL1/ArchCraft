# 04 — World generation

The goal: given `(x, z)` and a seed, decide what every block is — for a world that has
no edges and is never stored.

## The rule that makes it work

**Generation must be a pure function of (coordinate, seed).** No shared state, no
dependence on what was generated before.

Get this right and the hard problems evaporate: chunks generated in any order fit
together seamlessly, the world is identical after a restart or on a friend's machine,
and you can throw chunks away and regenerate them for free (which is exactly what
`World::update` does when you walk away and come back).

Get it wrong — say, a global `rand()` whose sequence depends on generation order — and
you get seams, flickering terrain, and bugs you cannot reproduce.

This is why `src/world/Noise.h` offers `hashCoords(x, z, seed)`: a deterministic hash
for "random but reproducible" decisions like *is there a tree in this column?*

## Perlin noise in four steps

`rand()` gives you static: every point independent, no structure. A heightmap made of
`rand()` is a spiky mess. Perlin noise is **smooth**: nearby points get nearby values.

1. Chop space into a grid of integer cells.
2. Give each grid corner a pseudo-random gradient direction — looked up from a shuffled
   permutation table, so it is reproducible and needs no memory.
3. For a sample point, dot each corner's gradient with the offset from that corner to
   the point. That is each corner's "opinion" of the value here.
4. Blend the opinions with the fade curve `6t⁵ − 15t⁴ + 10t³`, whose first and second
   derivatives vanish at 0 and 1 — which is why there are no visible grid-aligned
   creases.

One seed produces one permutation table, via a Fisher–Yates shuffle driven by xorshift.
Each noise field in `TerrainGen` gets **its own** table (`seed + 1`, `seed + 2`, …): reusing
one table at different frequencies correlates the fields, and your biomes would secretly
line up with your mountains.

## fbm: adding scales

One frequency of noise gives rolling hills with no detail. Real landscapes have features
at many scales at once, so sum several **octaves**, each double the frequency and half
the amplitude:

```cpp
for (int i = 0; i < octaves; ++i) {
    sum += perlin2(x * frequency, y * frequency) * amplitude;
    amplitude *= gain;        // 0.5
    frequency *= lacunarity;  // 2.0
}
```

`TerrainGen::surfaceHeight` combines three bands, each with a job:

```cpp
continent = fbm(x * 0.0021, z * 0.0021, 4)   // ~500-block features: land vs. ocean
hills     = fbm(x * 0.011,  z * 0.011,  4)   // ~90-block features: hills and valleys
detail    = fbm(x * 0.047,  z * 0.047,  2)   // ~20-block features: roughness
```

Tuning those six numbers *is* world generation. Halve a frequency and the features get
twice as wide; raise an amplitude and that scale dominates. Try it — it is the single
most instructive five minutes in this repo.

## Ridged noise for mountains

Plain fbm makes rounded blobs. Taking `1 − |noise|` folds the negative half upwards, so
what were zero-crossings become sharp crests — the signature of mountain ranges.

```cpp
const float mountainMask = clampf((continent - 0.12f) / 0.45f, 0.0f, 1.0f);
const float ridge = 1.0f - std::fabs(hills);
height += mountainMask * ridge * ridge * 38.0f;
```

The **mask** is as important as the ridge: it only applies the effect where the
continent field is already high, so ranges appear inland instead of being sprinkled
evenly over the map. Multiplying one noise field by another to control *where* an effect
applies is the core technique of layered world generation.

## Worm caves from two 3D fields

The zero-set of a 3D noise field is a *surface*. The intersection of two such surfaces is
a *line*. So carving only where both fields are near zero gives tunnels rather than
blobby swiss cheese:

```cpp
const float a = caves_.fbm3(x * 0.020f, y * 0.042f, z * 0.020f, 2);
const float b = caves_.fbm3(x * 0.020f + 31.7f, y * 0.042f + 11.3f, z * 0.020f + 7.9f, 2);
return std::fabs(a) < 0.055f && std::fabs(b) < 0.055f;
```

Sampling `y` at a higher frequency than `x`/`z` squashes the tunnels so they run
horizontally, like real cave systems. Widen the `0.055` threshold for bigger caverns;
set both to `0.2` for a world that is mostly hole.

Caves are not carved through the top three blocks of a column, so lakes do not drain
into them, nor through bedrock, so you cannot fall out of the world.

## Biomes

A separate low-frequency field acts as "temperature", picking the surface block:

```cpp
if (height > kSeaLevel + 34) return Biome::Snow;     // altitude beats latitude
if (temperature >  0.30f)    return Biome::Desert;
if (temperature < -0.26f)    return Biome::Snow;
if (temperature >  0.04f)    return Biome::Forest;   // denser trees
return Biome::Plains;
```

Because the field is smooth and low-frequency, biomes are large and their borders
wander. Columns at or just above sea level become sand regardless — that is the beach
rule, and it is why coastlines read correctly.

## Column assembly, and trees

Each column is filled top-down: bedrock floor, stone body, 3 blocks of soil, one surface
block, then water filled in from the terrain height up to sea level.

Trees are a second pass, and they expose the one place where purity is hard. A tree
whose canopy crosses a chunk border would need the neighbouring chunk to exist already —
which would make generation order-dependent. ArchCraft takes the simple way out: trees
are only planted with a 2-block inset from the chunk edge.

The consequence is visible if you look for it: a faint grid of tree-free lines every 16
blocks. The real fix is a **deferred feature queue** — when generating a chunk, also
compute which features *overlap* it from neighbouring chunks and apply those too. That
is roadmap item 4.

## Exercises

1. Change the seed (`--seed 42`). Then change only `continent`'s frequency to `0.0005`
   and look at the world from the air (`F` to fly, `Left Ctrl` to go fast).
2. Set `gain` to `0.75` in `fbm2`. What happens to the roughness, and why?
3. Make the cave threshold `0.12` and go spelunking.
4. Add a `Biome::Swamp` with a lower terrain height and more trees. Notice you only need
   to touch `biomeAt` and the column rules.
5. Write the tree placement as a deferred feature list and remove the 2-block inset.
