#include "ChunkMeshGenerator.h"

#include "../assets/AssetCache.h"
#include "../math/TypeUtil.h"
#include "../block/BlockModel.h"
#include "../block/BlockState.h"
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <iostream>

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;
    u32 texture_index;
    glm::vec3 tint;
    unsigned char ambient_occlusion;

    Vertex(glm::vec3 pos, glm::vec3 normal, glm::vec2 uv, u32 tex_index, glm::vec3 tint, int ambient_occlusion) 
        : position(pos), normal(normal), uv(uv), texture_index(tex_index), tint(tint), ambient_occlusion(static_cast<unsigned char>(ambient_occlusion)) 
    {
    }
};

namespace terra {
namespace render {


ChunkMeshGenerator::ChunkMeshGenerator(mc::world::World* world) : m_World(world) {
    world->RegisterListener(this);
}

ChunkMeshGenerator::~ChunkMeshGenerator() {
    m_World->UnregisterListener(this);
}

void ChunkMeshGenerator::OnBlockChange(mc::Vector3i position, mc::block::BlockPtr newBlock, mc::block::BlockPtr oldBlock) {
    mc::world::ChunkColumnPtr column = m_World->GetChunk(position);
    const mc::Vector3i chunk_base(column->GetMetadata().x * 16, (position.y / 16) * 16, column->GetMetadata().z * 16);

    auto iter = m_ChunkMeshes.find(chunk_base);

    if (iter == m_ChunkMeshes.end()) return;

    int chunk_x = column->GetMetadata().x;
    int chunk_y = static_cast<int>(position.y / 16);
    int chunk_z = column->GetMetadata().z;

    // TODO: Incremental update somehow?
    DestroyChunk(chunk_x, chunk_y, chunk_z);
    GenerateMesh((*column)[chunk_y], chunk_x, chunk_y, chunk_z);
}

void ChunkMeshGenerator::OnChunkLoad(mc::world::ChunkPtr chunk, const mc::world::ChunkColumnMetadata& meta, u16 index_y) {
    GenerateMesh(chunk, meta.x, index_y, meta.z);
}

int ChunkMeshGenerator::GetAmbientOcclusion(int side1, int side2, int corner) {
    if (side1 && side2) {
        return 0;
    }

    return 3 - (side1 + side2 + corner);
}

void ChunkMeshGenerator::GenerateMesh(mc::world::ChunkPtr chunk, int chunk_x, int chunk_y, int chunk_z) {
    if (chunk == nullptr) return;

    std::vector<Vertex> vertices;

    mc::Vector3i world_base(chunk_x * 16, chunk_y * 16, chunk_z * 16);

    // Sweep through the blocks and generate vertices for the mesh
    for (int y = 0; y < 16; ++y) {
        for (int z = 0; z < 16; ++z) {
            for (int x = 0; x < 16; ++x) {
                const mc::Vector3i pos(x, y, z);
                mc::block::BlockPtr block = chunk->GetBlock(pos);
                // TODO: Remove once water is implemented properly.
                if (block == nullptr || (!block->IsSolid() && block->GetName() != "minecraft:water")) continue;

                terra::block::BlockState* state = g_AssetCache->GetBlockState(block->GetType());
                if (state == nullptr) continue;

                terra::block::BlockModel* model = g_AssetCache->GetVariantModel(block->GetName(), state->GetVariant());
                if (model == nullptr) continue;

                glm::vec3 tint(1.0f, 1.0f, 1.0f);
                glm::vec3 side_tint(1.0f, 1.0f, 1.0f);

                // TODO: Remove once water is implemented properly.
                bool is_water = block->GetName() == "minecraft:water";

                // Temporary until tint related data is parsed from model elements.
                if (block->GetName().find("grass_block") != std::string::npos || block->GetName().find("leaves") != std::string::npos) {
                    double r = 137 / 255.0;
                    double g = 191 / 255.0;
                    double b = 98 / 255.0;

                    tint = glm::vec3(r, g, b);

                    if (block->GetName().find("grass_block") == std::string::npos) {
                        side_tint = tint;
                    }
                } else if (is_water) {
                    tint = glm::vec3(0.1, 0.4, 0.8);
                    side_tint = glm::vec3(0.1, 0.4, 0.8);
                }

                mc::Vector3i mc_pos = pos + world_base;

                const glm::vec3 base = terra::math::VecToGLM(pos) + terra::math::VecToGLM(world_base);

                mc::block::BlockPtr above = chunk->GetBlock(pos + mc::Vector3i(0, 1, 0));
                if (above == nullptr || !above->IsSolid()) {
                    // Render the top face of the current block.
                    glm::vec3 bottom_left = base + glm::vec3(0, 1, 0);
                    glm::vec3 bottom_right = base + glm::vec3(0, 1, 1);
                    glm::vec3 top_left = base + glm::vec3(1, 1, 0);
                    glm::vec3 top_right = base + glm::vec3(1, 1, 1);

                    int obl = GetAmbientOcclusion(m_World->GetBlock(mc_pos + mc::Vector3i(-1, 1, 0))->IsSolid(), m_World->GetBlock(mc_pos + mc::Vector3i(0, 1, -1))->IsSolid(), m_World->GetBlock(mc_pos + mc::Vector3i(-1, 1, -1))->IsSolid());
                    int obr = GetAmbientOcclusion(m_World->GetBlock(mc_pos + mc::Vector3i(-1, 1, 0))->IsSolid(), m_World->GetBlock(mc_pos + mc::Vector3i(0, 1, 1))->IsSolid(), m_World->GetBlock(mc_pos + mc::Vector3i(-1, 1, 1))->IsSolid());
                    int otl = GetAmbientOcclusion(m_World->GetBlock(mc_pos + mc::Vector3i(1, 1, 0))->IsSolid(), m_World->GetBlock(mc_pos + mc::Vector3i(0, 1, -1))->IsSolid(), m_World->GetBlock(mc_pos + mc::Vector3i(1, 1, -1))->IsSolid());
                    int otr = GetAmbientOcclusion(m_World->GetBlock(mc_pos + mc::Vector3i(1, 1, 0))->IsSolid(), m_World->GetBlock(mc_pos + mc::Vector3i(0, 1, 1))->IsSolid(), m_World->GetBlock(mc_pos + mc::Vector3i(1, 1, 1))->IsSolid());

                    auto&& texture_path = model->GetTexturePath(terra::block::BlockFace::Top);
                    u32 texture = g_AssetCache->GetTextures().GetIndex(texture_path);

                    vertices.emplace_back(bottom_left, glm::vec3(0, 1, 0), glm::vec2(0, 1), texture, tint, obl);
                    vertices.emplace_back(bottom_right, glm::vec3(0, 1, 0), glm::vec2(0, 0), texture, tint, obr);
                    vertices.emplace_back(top_right, glm::vec3(0, 1, 0), glm::vec2(1, 0), texture, tint, otr);

                    vertices.emplace_back(top_right, glm::vec3(0, 1, 0), glm::vec2(1, 0), texture, tint, otr);
                    vertices.emplace_back(top_left, glm::vec3(0, 1, 0), glm::vec2(1, 1), texture, tint, otl);
                    vertices.emplace_back(bottom_left, glm::vec3(0, 1, 0), glm::vec2(0, 1), texture, tint, obl);
                }

                mc::block::BlockPtr below = chunk->GetBlock(pos - mc::Vector3i(0, 1, 0));
                if (below == nullptr || !below->IsSolid()) {
                    // Render the bottom face of the current block.
                    glm::vec3 bottom_left = base + glm::vec3(1, 0, 0);
                    glm::vec3 bottom_right = base + glm::vec3(1, 0, 1);
                    glm::vec3 top_left = base + glm::vec3(0, 0, 0);
                    glm::vec3 top_right = base + glm::vec3(0, 0, 1);

                    int obl = GetAmbientOcclusion(m_World->GetBlock(mc_pos + mc::Vector3i(1, -1, 0))->IsSolid(), m_World->GetBlock(mc_pos + mc::Vector3i(0, -1, -1))->IsSolid(), m_World->GetBlock(mc_pos + mc::Vector3i(1, -1, -1))->IsSolid());
                    int obr = GetAmbientOcclusion(m_World->GetBlock(mc_pos + mc::Vector3i(1, -1, 0))->IsSolid(), m_World->GetBlock(mc_pos + mc::Vector3i(0, -1, 1))->IsSolid(), m_World->GetBlock(mc_pos + mc::Vector3i(1, -1, 1))->IsSolid());
                    int otl = GetAmbientOcclusion(m_World->GetBlock(mc_pos + mc::Vector3i(-1, -1, 0))->IsSolid(), m_World->GetBlock(mc_pos + mc::Vector3i(0, -1, -1))->IsSolid(), m_World->GetBlock(mc_pos + mc::Vector3i(-1, -1, -1))->IsSolid());
                    int otr = GetAmbientOcclusion(m_World->GetBlock(mc_pos + mc::Vector3i(-1, -1, 0))->IsSolid(), m_World->GetBlock(mc_pos + mc::Vector3i(0, -1, 1))->IsSolid(), m_World->GetBlock(mc_pos + mc::Vector3i(-1, -1, 1))->IsSolid());

                    auto&& texture_path = model->GetTexturePath(terra::block::BlockFace::Bottom);
                    u32 texture = g_AssetCache->GetTextures().GetIndex(texture_path);

                    vertices.emplace_back(bottom_left, glm::vec3(0, -1, 0), glm::vec2(1, 0), texture, side_tint, obl);
                    vertices.emplace_back(bottom_right, glm::vec3(0, -1, 0), glm::vec2(1, 1), texture, side_tint, obr);
                    vertices.emplace_back(top_right, glm::vec3(0, -1, 0), glm::vec2(0, 1), texture, side_tint, otr);

                    vertices.emplace_back(top_right, glm::vec3(0, -1, 0), glm::vec2(0, 1), texture, side_tint, otr);
                    vertices.emplace_back(top_left, glm::vec3(0, -1, 0), glm::vec2(0, 0), texture, side_tint, otl);
                    vertices.emplace_back(bottom_left, glm::vec3(0, -1, 0), glm::vec2(1, 0), texture, side_tint, obl);
                }

                mc::block::BlockPtr north = chunk->GetBlock(pos + mc::Vector3i(0, 0, -1));
                if (north == nullptr || !north->IsSolid()) {
                    // Render the north face of the current block.
                    glm::vec3 bottom_left = base + glm::vec3(1, 0, 0);
                    glm::vec3 bottom_right = base + glm::vec3(0, 0, 0);
                    glm::vec3 top_left = base + glm::vec3(1, 1, 0);
                    glm::vec3 top_right = base + glm::vec3(0, 1, 0);

                    int obl = GetAmbientOcclusion(m_World->GetBlock(mc_pos + mc::Vector3i(1, 0, -1))->IsSolid(), m_World->GetBlock(mc_pos + mc::Vector3i(0, -1, -1))->IsSolid(), m_World->GetBlock(mc_pos + mc::Vector3i(1, -1, -1))->IsSolid());
                    int obr = GetAmbientOcclusion(m_World->GetBlock(mc_pos + mc::Vector3i(0, -1, -1))->IsSolid(), m_World->GetBlock(mc_pos + mc::Vector3i(-1, 0, -1))->IsSolid(), m_World->GetBlock(mc_pos + mc::Vector3i(-1, -1, -1))->IsSolid());
                    int otl = GetAmbientOcclusion(m_World->GetBlock(mc_pos + mc::Vector3i(0, 1, -1))->IsSolid(), m_World->GetBlock(mc_pos + mc::Vector3i(1, 0, -1))->IsSolid(), m_World->GetBlock(mc_pos + mc::Vector3i(1, 1, -1))->IsSolid());
                    int otr = GetAmbientOcclusion(m_World->GetBlock(mc_pos + mc::Vector3i(0, 1, -1))->IsSolid(), m_World->GetBlock(mc_pos + mc::Vector3i(-1, 0, -1))->IsSolid(), m_World->GetBlock(mc_pos + mc::Vector3i(-1, 1, -1))->IsSolid());

                    auto&& texture_path = model->GetTexturePath(terra::block::BlockFace::North);
                    u32 texture = g_AssetCache->GetTextures().GetIndex(texture_path);

                    vertices.emplace_back(bottom_left, glm::vec3(0, 0, -1), glm::vec2(0, 0), texture, side_tint, obl);
                    vertices.emplace_back(bottom_right, glm::vec3(0, 0, -1), glm::vec2(1, 0), texture, side_tint, obr);
                    vertices.emplace_back(top_right, glm::vec3(0, 0, -1), glm::vec2(1, 1), texture, side_tint, otr);

                    vertices.emplace_back(top_right, glm::vec3(0, 0, -1), glm::vec2(1, 1), texture, side_tint, otr);
                    vertices.emplace_back(top_left, glm::vec3(0, 0, -1), glm::vec2(0, 1), texture, side_tint, otl);
                    vertices.emplace_back(bottom_left, glm::vec3(0, 0, -1), glm::vec2(0, 0), texture, side_tint, obl);
                }

                mc::block::BlockPtr south = chunk->GetBlock(pos + mc::Vector3i(0, 0, 1));
                if (south == nullptr || !south->IsSolid()) {
                    // Render the south face of the current block.
                    glm::vec3 bottom_left = base + glm::vec3(0, 0, 1);
                    glm::vec3 bottom_right = base + glm::vec3(1, 0, 1);
                    glm::vec3 top_left = base + glm::vec3(0, 1, 1);
                    glm::vec3 top_right = base + glm::vec3(1, 1, 1);

                    int obl = GetAmbientOcclusion(m_World->GetBlock(mc_pos + mc::Vector3i(-1, 0, 1))->IsSolid(), m_World->GetBlock(mc_pos + mc::Vector3i(0, -1, 1))->IsSolid(), m_World->GetBlock(mc_pos + mc::Vector3i(-1, -1, 1))->IsSolid());
                    int obr = GetAmbientOcclusion(m_World->GetBlock(mc_pos + mc::Vector3i(1, 0, 1))->IsSolid(), m_World->GetBlock(mc_pos + mc::Vector3i(0, -1, 1))->IsSolid(), m_World->GetBlock(mc_pos + mc::Vector3i(1, -1, 1))->IsSolid());
                    int otl = GetAmbientOcclusion(m_World->GetBlock(mc_pos + mc::Vector3i(0, 1, 1))->IsSolid(), m_World->GetBlock(mc_pos + mc::Vector3i(-1, 0, 1))->IsSolid(), m_World->GetBlock(mc_pos + mc::Vector3i(-1, 1, 1))->IsSolid());
                    int otr = GetAmbientOcclusion(m_World->GetBlock(mc_pos + mc::Vector3i(0, 1, 1))->IsSolid(), m_World->GetBlock(mc_pos + mc::Vector3i(1, 0, 1))->IsSolid(), m_World->GetBlock(mc_pos + mc::Vector3i(1, 1, 1))->IsSolid());

                    auto&& texture_path = model->GetTexturePath(terra::block::BlockFace::South);
                    u32 texture = g_AssetCache->GetTextures().GetIndex(texture_path);

                    vertices.emplace_back(bottom_left, glm::vec3(0, 0, 1), glm::vec2(0, 0), texture, side_tint, obl);
                    vertices.emplace_back(bottom_right, glm::vec3(0, 0, 1), glm::vec2(1, 0), texture, side_tint, obr);
                    vertices.emplace_back(top_right, glm::vec3(0, 0, 1), glm::vec2(1, 1), texture, side_tint, otr);

                    vertices.emplace_back(top_right, glm::vec3(0, 0, 1), glm::vec2(1, 1), texture, side_tint, otr);
                    vertices.emplace_back(top_left, glm::vec3(0, 0, 1), glm::vec2(0, 1), texture, side_tint, otl);
                    vertices.emplace_back(bottom_left, glm::vec3(0, 0, 1), glm::vec2(0, 0), texture, side_tint, obl);
                }

                mc::block::BlockPtr east = chunk->GetBlock(pos + mc::Vector3i(1, 0, 0));
                if (east == nullptr || !east->IsSolid()) {
                    // Render the east face of the current block.
                    glm::vec3 bottom_left = base + glm::vec3(1, 0, 1);
                    glm::vec3 bottom_right = base + glm::vec3(1, 0, 0);
                    glm::vec3 top_left = base + glm::vec3(1, 1, 1);
                    glm::vec3 top_right = base + glm::vec3(1, 1, 0);

                    int obl = GetAmbientOcclusion(m_World->GetBlock(mc_pos + mc::Vector3i(1, 0, 1))->IsSolid(), m_World->GetBlock(mc_pos + mc::Vector3i(1, -1, 0))->IsSolid(), m_World->GetBlock(mc_pos + mc::Vector3i(1, -1, 1))->IsSolid());
                    int obr = GetAmbientOcclusion(m_World->GetBlock(mc_pos + mc::Vector3i(1, -1, 0))->IsSolid(), m_World->GetBlock(mc_pos + mc::Vector3i(1, 0, -1))->IsSolid(), m_World->GetBlock(mc_pos + mc::Vector3i(1, -1, -1))->IsSolid());
                    int otl = GetAmbientOcclusion(m_World->GetBlock(mc_pos + mc::Vector3i(1, 1, 0))->IsSolid(), m_World->GetBlock(mc_pos + mc::Vector3i(1, 0, 1))->IsSolid(), m_World->GetBlock(mc_pos + mc::Vector3i(1, 1, 1))->IsSolid());
                    int otr = GetAmbientOcclusion(m_World->GetBlock(mc_pos + mc::Vector3i(1, 1, 0))->IsSolid(), m_World->GetBlock(mc_pos + mc::Vector3i(1, 0, -1))->IsSolid(), m_World->GetBlock(mc_pos + mc::Vector3i(1, 1, -1))->IsSolid());

                    auto&& texture_path = model->GetTexturePath(terra::block::BlockFace::East);
                    u32 texture = g_AssetCache->GetTextures().GetIndex(texture_path);

                    vertices.emplace_back(bottom_left, glm::vec3(0, 0, -1), glm::vec2(0, 0), texture, side_tint, obl);
                    vertices.emplace_back(bottom_right, glm::vec3(0, 0, -1), glm::vec2(1, 0), texture, side_tint, obr);
                    vertices.emplace_back(top_right, glm::vec3(0, 0, -1), glm::vec2(1, 1), texture, side_tint, otr);

                    vertices.emplace_back(top_right, glm::vec3(0, 0, -1), glm::vec2(1, 1), texture, side_tint, otr);
                    vertices.emplace_back(top_left, glm::vec3(0, 0, -1), glm::vec2(0, 1), texture, side_tint, otl);
                    vertices.emplace_back(bottom_left, glm::vec3(0, 0, -1), glm::vec2(0, 0), texture, side_tint, obl);
                }

                mc::block::BlockPtr west = chunk->GetBlock(pos + mc::Vector3i(-1, 0, 0));
                if (west == nullptr || !west->IsSolid()) {
                    // Render the west face of the current block.
                    glm::vec3 bottom_left = base + glm::vec3(0, 0, 0);
                    glm::vec3 bottom_right = base + glm::vec3(0, 0, 1);
                    glm::vec3 top_left = base + glm::vec3(0, 1, 0);
                    glm::vec3 top_right = base + glm::vec3(0, 1, 1);

                    int obl = GetAmbientOcclusion(m_World->GetBlock(mc_pos + mc::Vector3i(-1, -1, 0))->IsSolid(), m_World->GetBlock(mc_pos + mc::Vector3i(-1, 0, -1))->IsSolid(), m_World->GetBlock(mc_pos + mc::Vector3i(-1, -1, -1))->IsSolid());
                    int obr = GetAmbientOcclusion(m_World->GetBlock(mc_pos + mc::Vector3i(-1, -1, 0))->IsSolid(), m_World->GetBlock(mc_pos + mc::Vector3i(-1, 0, 1))->IsSolid(), m_World->GetBlock(mc_pos + mc::Vector3i(-1, -1, 1))->IsSolid());
                    int otl = GetAmbientOcclusion(m_World->GetBlock(mc_pos + mc::Vector3i(-1, 1, 0))->IsSolid(), m_World->GetBlock(mc_pos + mc::Vector3i(-1, 0, -1))->IsSolid(), m_World->GetBlock(mc_pos + mc::Vector3i(-1, 1, -1))->IsSolid());
                    int otr = GetAmbientOcclusion(m_World->GetBlock(mc_pos + mc::Vector3i(-1, 1, 0))->IsSolid(), m_World->GetBlock(mc_pos + mc::Vector3i(-1, 0, 1))->IsSolid(), m_World->GetBlock(mc_pos + mc::Vector3i(-1, 1, 1))->IsSolid());

                    auto&& texture_path = model->GetTexturePath(terra::block::BlockFace::West);
                    u32 texture = g_AssetCache->GetTextures().GetIndex(texture_path);

                    vertices.emplace_back(bottom_left, glm::vec3(0, 0, -1), glm::vec2(0, 0), texture, side_tint, obl);
                    vertices.emplace_back(bottom_right, glm::vec3(0, 0, -1), glm::vec2(1, 0), texture, side_tint, obr);
                    vertices.emplace_back(top_right, glm::vec3(0, 0, -1), glm::vec2(1, 1), texture, side_tint, otr);

                    vertices.emplace_back(top_right, glm::vec3(0, 0, -1), glm::vec2(1, 1), texture, side_tint, otr);
                    vertices.emplace_back(top_left, glm::vec3(0, 0, -1), glm::vec2(0, 1), texture, side_tint, otl);
                    vertices.emplace_back(bottom_left, glm::vec3(0, 0, -1), glm::vec2(0, 0), texture, side_tint, obl);
                }
            }
        }
    }

    if (vertices.empty()) return;

    GLuint vao = 0;

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    g_AssetCache->GetTextures().Bind();

    GLuint vbo;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * vertices.size(), &vertices[0], GL_STATIC_DRAW);

    // Position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));
    glEnableVertexAttribArray(0);

    // Normal
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
    glEnableVertexAttribArray(1);

    // TextureCoord
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, uv));
    glEnableVertexAttribArray(2);

    // TextureIndex
    glVertexAttribIPointer(3, 1, GL_UNSIGNED_INT, sizeof(Vertex), (void*)offsetof(Vertex, texture_index));
    glEnableVertexAttribArray(3);

    // Tint
    glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, tint));
    glEnableVertexAttribArray(4);

    // Ambient occlusion
    glVertexAttribIPointer(5, 1, GL_UNSIGNED_BYTE, sizeof(Vertex), (void*)offsetof(Vertex, ambient_occlusion));
    glEnableVertexAttribArray(5);

    GLenum error;

    while ((error = glGetError()) != GL_NO_ERROR) {
        std::cout << "OpenGL error when creating mesh: " << error << std::endl;
    }

    std::unique_ptr<terra::render::ChunkMesh> mesh = std::make_unique<terra::render::ChunkMesh>(vao, vbo, vertices.size());

    m_ChunkMeshes[world_base] = std::move(mesh);
}

void ChunkMeshGenerator::OnChunkUnload(mc::world::ChunkColumnPtr chunk) {
    for (int y = 0; y < 16; ++y) {
        DestroyChunk(chunk->GetMetadata().x, y, chunk->GetMetadata().z);
    }
}

void ChunkMeshGenerator::DestroyChunk(int chunk_x, int chunk_y, int chunk_z) {
    mc::Vector3i key(chunk_x * 16, chunk_y * 16, chunk_z * 16);

    auto iter = m_ChunkMeshes.find(key);
    if (iter != m_ChunkMeshes.end()) {
        iter->second->Destroy();
        m_ChunkMeshes.erase(key);
    }
}

} // ns render
} // ns terra
