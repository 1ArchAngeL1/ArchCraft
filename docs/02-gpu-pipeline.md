# 02 — The GPU pipeline

The one-paragraph version: you upload vertices to the GPU once, then each frame you tell
it "draw those, with this shader program and these uniforms". The GPU runs your **vertex
shader** once per vertex, assembles triangles, works out which pixels each triangle
covers, and runs your **fragment shader** once per covered pixel.

```
  vertex buffer (VBO)            "here are 3000 vertices"
        │
        ▼
  vertex shader        × 3000    position → clip space        block.vert
        │
        ▼
  rasteriser           (fixed)   triangles → pixels, interpolating vertex outputs
        │
        ▼
  fragment shader      × ~2M     texture lookup, lighting, fog   block.frag
        │
        ▼
  depth test + blend   (fixed)   keep the nearest fragment; mix transparency
        │
        ▼
  framebuffer                    the image you see
```

Two numbers drive every decision here: **3,000 vertices vs. ~2,000,000 fragments**. Work
you can do per vertex is ~600× cheaper than the same work per fragment. That is why
ArchCraft bakes lighting into the vertices (see [03](03-chunks-and-meshing.md)) and why
`block.frag` is four lines of arithmetic.

## Coordinate spaces

A vertex travels through four spaces on its way to the screen:

```
chunk-local  --+ uChunkOrigin -->  world  --uView-->  view  --uProjection-->  clip
 (0..16)                        (-∞..∞)       (camera at origin)      (-w..w)
```

Then the GPU does the **perspective divide** (`x/w, y/w, z/w`) and maps the result to
pixels. That divide is what makes distant things small; it is the only reason a 3D
picture looks 3D, and it is three characters of fixed-function hardware.

- **Model → world.** Chunk meshes store positions in chunk-local coordinates (0..16),
  so the "model transform" is just adding the chunk's origin. A full 4×4 matrix would
  work, but a `vec3` uniform is 3 floats instead of 16, uploaded per chunk per frame.
  Keeping vertices chunk-local also keeps them small and precise: world coordinates far
  from the origin lose float precision and the geometry starts to wobble.
- **World → view** (`Mat4::lookAt`). Rather than moving the camera, move the entire
  world so that the camera sits at the origin looking down −Z. The matrix is the
  camera's three basis vectors, transposed, plus a translation by −eye.
- **View → clip** (`Mat4::perspective`). Read the source: the whole trick is
  `r.m[11] = -1.0f`, which copies −z into `w` so the divide shrinks distant geometry.

Matrices are **column-major**, matching OpenGL, so `Mat4::m` goes straight to
`glUniformMatrix4fv` without transposing. And remember multiplication reads
right-to-left: `projection * view * position` applies the view first.

## The four pieces of fixed-function state that matter

ArchCraft sets exactly four, in `WorldRenderer`:

**Depth testing** (`GL_DEPTH_TEST`). Each pixel keeps the depth of the nearest fragment
written so far; a fragment further away is discarded. Without it, the last triangle
drawn wins and terrain renders inside-out. This is also why we can draw chunks in any
order.

**Back-face culling** (`GL_CULL_FACE`). The GPU drops triangles whose vertices wind
clockwise on screen — which, with consistent winding, means exactly the faces pointing
away from you. Half the rasterisation work disappears for free. The cost is that your
mesher must get the winding right; see the face table in `ChunkMesher.cpp`, where each
quad is listed counter-clockwise *as seen from outside the cube*. Get one face backwards
and it becomes invisible from outside and visible from inside.

**Alpha cutout** (`discard`). Leaves and glass have fully transparent texels. In
`block.frag`:

```glsl
if (texel.a < uAlphaCutoff) discard;
```

`discard` throws the fragment away entirely — no colour, no depth — so you see straight
through the hole to whatever is behind. This is the cheap way to do transparency and it
needs **no sorting**, which is why leaves live in the opaque pass.

**Blending** (`GL_BLEND`, for water only). Real translucency mixes the fragment with
what is already in the framebuffer: `src.rgb * src.a + dst.rgb * (1 - src.a)`. That
means (a) whatever is behind must already be drawn, so water comes after the opaque
pass, and (b) `glDepthMask(GL_FALSE)` during the pass, so one water quad does not block
the water quad behind it. Order matters for blending in a way it never does for opaque
geometry — this is the single most common source of "why is my transparency wrong?".

## Fog, and the hole it hides

Chunks stop existing at the render distance, which would otherwise be a hard edge with
sky beyond it. So `block.frag` fades colour towards the sky colour with distance:

```glsl
float fog = clamp((vViewDistance - uFogStart) / (uFogEnd - uFogStart), 0.0, 1.0);
colour = mix(colour, uFogColour, fog);
```

Two details worth copying: fog uses **horizontal** distance only (`worldPos.xz`), so
looking up at the sky does not fog it and the fog ring stays circular around the player;
and the fog colour is *exactly* the clear colour, so the fade lands on the sky invisibly.
Change `kSkyColour` without changing the fog and you will see a grey wall at the horizon.

## Uniforms vs. attributes

| | Changes per | Example |
|---|---|---|
| attribute | vertex | position, uv, baked shade |
| uniform | draw call | the view matrix, the fog colour, the chunk origin |

`glGetUniformLocation` is a string lookup inside the driver, so `Shader` caches the
locations in a hash map. A location of `-1` means "no such uniform" — including uniforms
the compiler removed because nothing read them. Setting `-1` is silently ignored, which
is a classic way to spend an hour debugging a shader that was optimised out.

## Exercises

1. Comment out `glEnable(GL_DEPTH_TEST)`. Look at the result for a while — it tells you
   exactly what the depth buffer does.
2. Reverse one face's winding in the `kFaces` table and find the hole it makes.
3. Remove the `glDepthMask(GL_FALSE)` around the water pass and look at a lake edge-on.
4. Make `uFogColour` bright red. Now you can see precisely where fog starts — then set
   the fog range from `0.1` to `0.3` of the render distance and stand on a hill.
