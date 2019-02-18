cmake_minimum_required(VERSION 3.1)

project(Terracotta)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/modules/")
set(CMAKE_BUILD_TYPE Release)
set(CMAKE_CXX_STANDARD 14)

include(GNUInstallDirs)

add_executable(terracotta
    terracotta/assets/AssetCache.cpp
    terracotta/assets/AssetCache.h
    terracotta/assets/AssetLoader.cpp
    terracotta/assets/AssetLoader.h
    terracotta/assets/stb_image.h
    terracotta/assets/TextureArray.cpp
    terracotta/assets/TextureArray.h
    terracotta/assets/zip/miniz.cpp
    terracotta/assets/zip/miniz.h
    terracotta/assets/zip/ZipArchive.cpp
    terracotta/assets/zip/ZipArchive.h
    terracotta/block/BlockElement.h
    terracotta/block/BlockFace.cpp
    terracotta/block/BlockFace.h
    terracotta/block/BlockModel.cpp
    terracotta/block/BlockModel.h
    terracotta/block/BlockState.cpp
    terracotta/block/BlockState.h
    terracotta/Camera.cpp
    terracotta/Camera.h
    terracotta/ChatWindow.cpp
    terracotta/ChatWindow.h
    terracotta/Chunk.cpp
    terracotta/Chunk.h
    terracotta/Game.cpp
    terracotta/Game.h
    terracotta/GameWindow.cpp
    terracotta/GameWindow.h
    terracotta/lib/imgui/imconfig.h
    terracotta/lib/imgui/imgui.cpp
    terracotta/lib/imgui/imgui.h
    terracotta/lib/imgui/imgui_draw.cpp
    terracotta/lib/imgui/imgui_impl_glfw.cpp
    terracotta/lib/imgui/imgui_impl_glfw.h
    terracotta/lib/imgui/imgui_impl_opengl3.cpp
    terracotta/lib/imgui/imgui_impl_opengl3.h
    terracotta/lib/imgui/imgui_internal.h
    terracotta/lib/imgui/imgui_widgets.cpp
    terracotta/lib/imgui/imstb_rectpack.h
    terracotta/lib/imgui/imstb_textedit.h
    terracotta/lib/imgui/imstb_truetype.h
    terracotta/main.cpp
    terracotta/math/Plane.cpp
    terracotta/math/Plane.h
    terracotta/math/TypeUtil.h
    terracotta/math/volumes/Frustum.cpp
    terracotta/math/volumes/Frustum.h
    terracotta/PriorityQueue.h
    terracotta/render/ChunkMesh.cpp
    terracotta/render/ChunkMesh.h
    terracotta/render/ChunkMeshGenerator.cpp
    terracotta/render/ChunkMeshGenerator.h
    terracotta/render/Shader.cpp
    terracotta/render/Shader.h
    terracotta/World.cpp
    terracotta/World.h
)

add_definitions(-DGLEW_STATIC -DIMGUI_IMPL_OPENGL_LOADER_GLEW)

find_package(glfw3 REQUIRED)
find_package(GLEW REQUIRED)
find_package(GLM REQUIRED)
find_package(mclib REQUIRED)

if (WIN32)
set(GL_LIBRARY opengl32)
else()
set(GL_LIBRARY GL)
endif()

target_include_directories(terracotta PRIVATE ${MCLIB_INCLUDE_DIR} ${GLFW3_INCLUDE_DIR} ${GLM_INCLUDE_DIRS} ${GLEW_INCLUDE_DIRS})
target_link_libraries(terracotta PRIVATE ${GL_LIBRARY} ${MCLIB_LIBRARY} ${GLFW3_LIBRARY} ${GLEW_LIBRARY})
