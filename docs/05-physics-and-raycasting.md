# 05 — Physics and raycasting

Two problems that look unrelated and share a trick: in a grid world, you can always
reduce the question to "which integer cells am I touching?"

## The player is a box

No meshes, no capsules: an axis-aligned bounding box, 0.6 × 1.8 × 0.6, with the eye at
1.62. Those are Minecraft's numbers, and they are not arbitrary — they are tuned so a
1-block gap is walkable and a 2-block gap is not crawlable.

Position is stored at the **centre of the feet**, which makes "am I standing on
something?" and "where do I place a block?" read naturally.

## Integration

```cpp
velocity.y -= kGravity * dt;      // a = -28 m/s²
position   += velocity * dt;
```

This is explicit Euler, and for a game it is fine. Gravity of 28 and a jump velocity of
8.4 give a jump height of `v²/2a = 1.26` blocks: enough to climb one block, not two.
Change `kJumpSpeed` to 11 and you can jump two blocks — the whole feel of the game
changes with one constant, which is worth experiencing.

Terminal velocity (`kMaxFallSpeed`) is not just realism: it bounds how far you can move
in one step, which bounds how badly collision can fail.

## Collision: one axis at a time

The whole algorithm, in `Player::moveAxis`:

1. Move along **one** axis.
2. Find every block the box now overlaps.
3. If any is solid, snap back so the box just touches it, and zero the velocity on that
   axis.
4. Repeat for the next axis.

Doing X, then Y, then Z separately is what makes this so short — and it gives you
**sliding along walls for free**. Walk diagonally into a wall: the X move is blocked and
cancelled, the Z move is not, so you slide. No contact normals, no impulses.

It also gives the ground check for free:

```cpp
if (axis == 1 && amount < 0.0f) onGround_ = true;
```

"I was moving down and hit something" is exactly what "can I jump?" needs.

### The skin width

```cpp
constexpr float kSkin = 1e-3f;
```

After a collision, the box is placed a hair away from the surface rather than exactly on
it. Without that gap, floating-point rounding puts the box boundary *inside* the block,
and next frame `floor()` reports an overlap that never resolves: the player sinks
through the floor or sticks to walls. Every collision system has a constant like this,
and leaving it out produces bugs that feel haunted.

The matching epsilon in the overlap loop stops a box whose face sits exactly on a block
boundary from claiming the next block as well.

### What this does not do

Tunnelling: if a single frame's movement exceeds one block, the box can pass through a
thin wall, because we only test the cells it *ends up* in. The clamped `dt` plus
terminal velocity keep us under that threshold in practice. The real fix is a swept test
(march in sub-block steps), which is roadmap item 7.

## Raycasting: which block is the crosshair on?

The tempting approach is to step along the ray in small increments and sample. It is
both slower and wrong: too large a step skips thin walls, too small a step burns
thousands of samples per frame, and neither is exact.

The right answer is the **Amanatides & Woo** grid traversal (1987) — the 3D cousin of
Bresenham. It visits every cell the ray actually touches, in order, exactly once.

The idea: for each axis, track how far along the ray (`t`) you must travel to cross the
next cell boundary. Repeatedly step across whichever boundary is nearest.

```cpp
deltaX = |1 / dir.x|;    // t-cost of crossing one whole cell on X
nextX  = (distance to the first X boundary) * deltaX;

while (travelled <= maxDistance) {
    if (solid at (x,y,z)) return hit;
    if (nextX < nextY && nextX < nextZ) { x += stepX; travelled = nextX; nextX += deltaX; }
    else if (nextY < nextZ)             { y += stepY; travelled = nextY; nextY += deltaY; }
    else                                { z += stepZ; travelled = nextZ; nextZ += deltaZ; }
}
```

For a 5-block reach this visits perhaps 10 cells. Exact, no tuning parameter, and the
loop body is three adds and a compare.

### The normal falls out for free

Each step records which axis you crossed and in which direction. The face you entered
through is the opposite:

```cpp
enteredNormal[0] = -stepX;
```

That normal is exactly where a newly placed block goes: `hit + normal`. This is why
right-clicking the top of a block puts the new block on top, and right-clicking its side
puts it beside — without a single extra line of geometry.

### Placing blocks without trapping yourself

```cpp
if (boxOverlapsBlock(px, py, pz)) return;
```

Reuse the collision overlap test before placing. Omit this and you can seal yourself
inside a block; it is the first thing players find.

## Water

Liquids are `solid = false`, so the collision loop ignores them and you fall in. Being
in water is sampled at hip height, so a shallow puddle does not count as swimming:
gravity drops to 28%, speed drops, and `Space` swims upwards. Thirty lines, and it reads
as a different medium.

## Exercises

1. Set `kSkin` to `0.0f` and walk around. Then set it to `0.1f`.
2. Remove the per-axis split — move all three axes, then resolve once. Try walking
   diagonally into a wall and see what you lose.
3. Raise `kMaxFallSpeed` to 500 and jump off the highest mountain you can find. Which
   failure mode do you hit first?
4. Add a `kReach` of 50 and notice the DDA loop does not care.
5. Implement step-up: when a horizontal move is blocked by exactly one block and there
   is headroom, raise the player instead of stopping. That is what makes walking up
   terraced hills feel smooth without jumping.
