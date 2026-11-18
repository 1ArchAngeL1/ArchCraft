#include "world/Chunk.h"

// Chunk is header-only for now: every member is small enough to inline, and
// the mesher/generator want those accessors inlined badly (they are called
// millions of times per chunk). This translation unit exists so the layout
// static_asserts below are checked somewhere.

static_assert(kChunkBlockCount == 32768, "chunk size changed - update the docs");
static_assert(Chunk::index(0, 0, 0) == 0, "index() must start at 0");
static_assert(Chunk::index(kChunkSizeX - 1, kChunkSizeY - 1, kChunkSizeZ - 1) ==
                  kChunkBlockCount - 1,
              "index() must cover the array exactly");
