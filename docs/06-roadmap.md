# 06 — Roadmap

Ordered by **learning value per hour of work**, not by how impressive the result looks.
Each item says what to change and what you will learn.

## Tier 1 — the engine wants these next

### 1. Move chunk generation and meshing onto worker threads
*Touches: `World::update`, `ChunkMesher`*

Right now generation and meshing run on the main thread with a per-frame budget
(4 chunks generated, 3 meshed). That keeps frames smooth but caps how fast the world can
stream in — fly quickly and you outrun it.

The fix is a job queue: worker threads generate blocks and build **vertex data** (pure
CPU, no GL calls), the main thread only does the `Mesh::upload`, because an OpenGL
context belongs to one thread.

Learn: thread pools, the difference between CPU and GPU work, why `static PaddedBlocks
cache` in `ChunkMesher.cpp` must become `thread_local` the moment you do this (the
comment is already there waiting for you).

### 2. Greedy meshing
*Touches: `ChunkMesher.cpp`*

Merge adjacent coplanar faces with the same texture and the same AO into one large quad.
A flat 16×16 grass field becomes 1 quad instead of 256. Typical saving: 50–80% of
vertices.

Learn: the classic voxel optimisation, and why AO and greedy meshing fight each other
(you can only merge quads whose corner shading matches).

### 3. Save and load the world
*Touches: new `world/ChunkStorage.*`, `World`*

Only store chunks the player has **modified** — everything else regenerates from the
seed for free. A region file of modified chunks, plus the seed, is a complete save.

Learn: serialisation, and the payoff of having made generation deterministic.

### 4. A deferred feature queue for trees
*Touches: `TerrainGen`*

Remove the 2-block inset that keeps trees inside chunk borders: when generating a chunk,
also evaluate features originating in neighbouring chunks that overlap into this one.

Learn: how real engines keep generation order-independent while placing structures
larger than one chunk.

### 5. Lighting
*Touches: `Chunk` (add a light array), `ChunkMesher`, `TerrainGen`*

Minecraft-style flood-fill light: sky light propagates down and spreads sideways with
falloff; torches emit light. Store a light level per block, bake it into the vertex
shade next to AO.

Learn: BFS flood fill, why light updates are incremental, how data that lives *per
block* changes your chunk storage.

## Tier 2 — the game layer

### 6. A fixed timestep
*Touches: `main.cpp`*

Accumulate real time and run physics in constant 1/60 s slices, interpolating the camera
between them. Makes physics reproducible and independent of frame rate.

### 7. Swept collision
*Touches: `Player::moveAxis`*

March the move in sub-block steps so fast movement cannot tunnel through thin walls.

### 8. Block entities and interaction
Chests, doors, levers — blocks with state beyond their id. The interesting design
question is where that state lives, since a block is one byte.

### 9. Inventory and crafting
Mining yields items, items stack, recipes consume them. Pure game logic, no graphics —
a good place to practise clean data modelling.

### 10. Day/night cycle
Animate the sky colour and the directional light term. Cheap, and it transforms the mood.

## Tier 3 — graphics depth

### 11. Texture array instead of an atlas
`GL_TEXTURE_2D_ARRAY` gives each block type its own layer, so mipmapping and anisotropic
filtering work without tiles bleeding into each other. Fixes the shimmering at distance
that `GL_NEAREST` currently papers over.

### 12. Frustum-culled, distance-sorted transparency
Sort water chunks back-to-front before the blended pass, so overlapping water surfaces
compose correctly.

### 13. Shadow mapping
Render depth from the sun's point of view, then test each fragment against it. The
standard introduction to render-to-texture, and to the whole family of shadow artefacts.

### 14. Ambient occlusion in screen space, or a deferred renderer
Only worth it once you understand what the per-vertex AO is approximating.

### 15. Level of detail for distant chunks
Mesh far chunks at half or quarter resolution. Raises the practical render distance a
lot, and introduces the seam problem that LOD always brings.

## Smaller exercises

- A debug overlay with a real bitmap font instead of the window title.
- A block-breaking animation (crack texture over time).
- Footstep particle effects and a block-break particle burst.
- Flowing water that spreads to neighbours — cellular automata on a voxel grid.
- `--benchmark` mode: generate N chunks, report milliseconds per chunk, so your
  optimisations have numbers behind them.

## How to work on this

1. Change one thing at a time, and look at the triangle/chunk counters in the title bar
   before and after.
2. `./build/archcraft --frames 200 --screenshot before.tga` gives you reproducible
   screenshots to compare against.
3. Keep terrain generation pure. Every feature that breaks determinism costs you ten
   times what it saves.
