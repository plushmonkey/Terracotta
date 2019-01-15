# Terracotta
This is a toy Minecraft client that I'm using to learn OpenGL. I have no experience, so this is probably really bad.

## Running
This makes use of the Minecraft assets stored in the 1.13.2 jar.  
Copy the 1.13.2.jar, bin/blocks.json, bin/shaders, and mclib.dll over to the folder with terracotta.exe for it to work correctly.

It can currently connect to Minecraft servers and attempts to render blocks. The block data isn't fully parsed yet, so it renders them with full block faces. Some of the texture resolution isn't working yet.

## Videos
[Basic rendering](https://gfycat.com/InferiorMessyFreshwatereel) - Renders most basic blocks. Doesn't render plants and water. Entities are rendered with a black box.

## Building
### Dependencies
- C++14 compiler
- CMake
- libglew-dev
- libglfw3-dev
- libglm-dev
- [mclib](https://github.com/plushmonkey/mclib)
