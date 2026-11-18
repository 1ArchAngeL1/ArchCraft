cmake_minimum_required(VERSION 3.20)
project(ArchCraft VERSION 0.1.0 LANGUAGES CXX DESCRIPTION "An educational voxel engine")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Default to an optimised build with debug symbols: a voxel engine in a pure
# debug build is painfully slow (the mesher touches ~32k voxels per chunk).
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

# ---------------------------------------------------------------- dependencies
find_package(SDL3 QUIET)
if(NOT SDL3_FOUND)
  message(FATAL_ERROR
    "SDL3 was not found.\n"
    "  macOS:  brew install sdl3\n"
    "  Linux:  build from https://github.com/libsdl-org/SDL (or your distro's sdl3-dev)\n"
    "  Windows: vcpkg install sdl3")
endif()
find_package(OpenGL REQUIRED)

# --------------------------------------------------------------------- sources
add_executable(archcraft
  src/main.cpp

  src/core/Window.cpp
  src/core/Input.cpp

  src/math/Frustum.cpp

  src/gfx/gl.cpp
  src/gfx/Shader.cpp
  src/gfx/Mesh.cpp
  src/gfx/TextureAtlas.cpp
  src/gfx/Camera.cpp
  src/gfx/Screenshot.cpp
  src/gfx/WorldRenderer.cpp
  src/gfx/Hud.cpp

  src/world/Block.cpp
  src/world/Noise.cpp
  src/world/Chunk.cpp
  src/world/ChunkMesher.cpp
  src/world/TerrainGen.cpp
  src/world/World.cpp
  src/world/Raycast.cpp

  src/player/Player.cpp
)

target_include_directories(archcraft PRIVATE src)
target_link_libraries(archcraft PRIVATE SDL3::SDL3 OpenGL::GL)

# Shaders are read from disk at startup so you can edit them and just restart
# (no recompile). ARCHCRAFT_ASSET_DIR points at the source tree during dev.
target_compile_definitions(archcraft PRIVATE
  ARCHCRAFT_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets"
  GL_SILENCE_DEPRECATION    # Apple deprecated OpenGL in favour of Metal
)

if(MSVC)
  target_compile_options(archcraft PRIVATE /W4)
else()
  target_compile_options(archcraft PRIVATE -Wall -Wextra -Wpedantic)
endif()
