# 03 — Chunks and meshing

This is the part that makes a voxel game possible. Everything else is ordinary graphics
programming; this is the idea that turns "millions of cubes" into something a GPU will
happily draw at 120 fps.

## Why chunks

Suppose you tried the obvious thing: keep a big array of blocks, and draw a cube for
each one.

- **Memory.** An "infinite" world cannot be one array.
- **Draw calls.** Each one has CPU overhead of a few microseconds. At 32,768 blocks per
  chunk × 200 chunks, you would issue 6.5 million draw calls per frame. At even 1 µs
  each that is 6.5 seconds per frame.
- **Updates.** Change one block and you would rebuild the whole world.

A chunk — here 16 × 128 × 16 = 32,768 blocks, 32 KB of ids — is the unit that fixes all
of it. It is the unit of:

| | |
|---|---|
| generation | make terrain for one chunk on demand |
| meshing | one vertex buffer per chunk, rebuilt only when that chunk changes |
| culling | test one bounding box instead of 32,768 blocks |
| streaming | load near the player, free far away |

Why these dimensions? 16 is a power of two, so `floorDiv`/`floorMod` compile to shifts
and masks, and it is small enough that re-meshing one chunk after a block edit is
imperceptible (~1 ms). Full-height chunks (128 rather than 16³ cubes) keep terrain
generation simple: one pass down each column, no vertical neighbours to coordinate.

## Mesh the surface, not the volume

Inside a hill, every block is surrounded by solid rock. Those faces can never be seen.
So the mesher (`src/world/ChunkMesher.cpp`) walks the grid once and emits a quad **only
where a block touches something see-through**:

```cpp
bool faceVisible(BlockId self, BlockId neighbour) {
    if (isOpaque(neighbour)) return false;              // buried: skip
    if (isLiquid(self) && neighbour == self) return false;  // interior of a lake
    return true;
}
```

The result in practice: a 32,768-block chunk of terrain becomes roughly 1,000–3,000
quads instead of 196,608. Measured on the default seed, the whole visible world —
197 chunks — is about 200,000 triangles, which a modern GPU draws without noticing.

And crucially this happens **once per chunk change**, not once per frame. Uploading is
expensive; drawing an uploaded buffer is nearly free. That asymmetry is the reason the
engine is structured the way it is.

## Ambient occlusion: the 10 lines that sell the look

Flat-shaded cubes look like a spreadsheet. Real surfaces get darker where they are
tucked into a corner, because less of the sky reaches them. For voxels there is a
famously cheap approximation: for each quad corner, look at the three blocks touching
that corner in the layer in front of the face.

```
   corner  side2          side1, side2 = the two edge neighbours
      ┌───┬───┐           corner       = the diagonal
      │ c │ s2│
      ├───┼───┤           occluders -> darkness
 side1│ s1│ V │ <- the quad corner we are shading
      └───┴───┘
```

```cpp
int vertexAo(bool side1, bool side2, bool corner) {
    if (side1 && side2) return 0;   // fully pinched
    return 3 - (side1 + side2 + corner);
}
```

The special case matters: if both edge neighbours are solid, the corner is sealed
regardless of the diagonal, and should be darkest. The 0..3 level maps to a brightness
of 0.55..1.0, multiplied by a per-face constant (top 1.0, sides 0.86/0.72, bottom 0.5).
That product is baked into the vertex as a single float, so the fragment shader does one
multiply.

### The flipped-quad detail

A quad is two triangles, and the diagonal you split on is visible once the corners have
different brightness. Split across the two *bright* corners and the shading bends the
wrong way, leaving an obvious crease. The fix is one line:

```cpp
if (ao[0] + ao[2] >= ao[1] + ao[3]) { /* 0-1-2, 0-2-3 */ }
else                                { /* 1-2-3, 1-3-0 */ }   // flip the diagonal
```

Always split along the diagonal joining the two most similar corners. This is the kind
of detail that separates "my voxel engine looks off and I can't say why" from "it looks
right".

## The padded neighbour cache

Meshing reads a lot of neighbours: 6 per block for culling, 12 more for AO. Going
through `World::getBlock()` each time means a hash-map lookup per read — millions per
chunk.

So before meshing, the chunk plus a one-block skin of its neighbours is copied into a
flat `18 × 130 × 18` array (~42 KB). Filling it costs 324 map lookups — one per column —
and every read afterwards is a single array index. This is a pattern worth internalising:
**when an inner loop touches a slow data structure, copy the working set into a fast one
first.**

Two deliberate choices in that copy:

- Below the world (`y = -1`) is **bedrock**, so the bottom faces of the lowest layer get
  culled rather than wastefully drawn.
- A neighbour chunk that is not loaded yet counts as **stone**. If it counted as air, we
  would emit a whole wall of faces that get thrown away a few frames later when the
  neighbour arrives. (`World::update` only meshes chunks whose four neighbours already
  have terrain, so this path is a fallback rather than the normal case.)

## Chunk borders: the classic bug

A block on a chunk edge is part of its neighbour's mesh too — the neighbour culled or
kept a face based on what was there. So editing one must dirty the other:

```cpp
if (localX == 0)               markNeighbourDirty({pos.x - 1, pos.z});
if (localX == kChunkSizeX - 1) markNeighbourDirty({pos.x + 1, pos.z});
```

Forget this and you get holes you can see through at chunk boundaries, usually noticed
only after you dig into a wall near an edge. The same reasoning applies when a chunk is
*generated*: its neighbours were meshed against "not loaded", so they are dirtied too
(see `World::ensureChunk`).

## The texture atlas

Switching textures between draw calls is expensive, and one chunk mesh contains many
block types. So every block face lives in one 256×256 texture — a 16×16 grid of 16×16
tiles — and each vertex's UV simply points at the right tile. One texture bound for the
entire world.

The pixels are painted in code (`src/gfx/TextureAtlas.cpp`) rather than loaded from
files: speckled noise for stone and dirt, a wobbling lip for grass sides, running-bond
mortar lines for brick, concentric rings for log tops, scattered alpha-0 holes for
leaves. Run `./build/archcraft --dump-atlas` and open `atlas.tga` to see it.

Two gotchas the code handles:

- **Seam bleeding.** UVs are inset by a fraction of a texel (`kInset`) so a face never
  samples the tile next door.
- **No mipmaps.** Mipmapping would average neighbouring atlas tiles together at
  distance — grass blending into brick. Filtering is `GL_NEAREST`, which also keeps
  16×16 art crisp instead of blurry. The real fix (padded tiles or a texture array) is
  in [the roadmap](06-roadmap.md).

## Exercises

1. In `faceVisible`, always return `true`. Watch the triangle count in the title bar and
   compare the frame rate. This is the "naive" engine, and the number is the whole
   lesson.
2. Force `ao[corner] = 3` everywhere. The world goes flat — that is how much those ten
   lines were doing.
3. Remove the quad-flip branch and look at a staircase of blocks from a low angle.
4. Make leaves `opaque = true` in `Block.cpp` and look at a tree. Then explain why the
   mesher's behaviour changed.
5. Harder: implement **greedy meshing** — merge adjacent coplanar quads with the same
   texture and AO into one large quad. Typical saving is 50–80% of the vertices on
   terrain. The catch is that AO must match across the merge, which is why the two
   features interact.
