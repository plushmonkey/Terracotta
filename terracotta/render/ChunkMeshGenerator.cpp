#include "ChunkMeshGenerator.h"

#include "../assets/AssetCache.h"
#include "../math/TypeUtil.h"
#include "../block/BlockModel.h"
#include "../block/BlockState.h"
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <iostream>
#include <algorithm>
#include <thread>

namespace terra {
namespace render {

ChunkMeshGenerator::ChunkMeshGenerator(terra::World* world, const glm::vec3& camera_position) : m_World(world), m_ChunkBuildQueue(ChunkMeshBuildComparator(camera_position)) {
    world->RegisterListener(this);

    m_Working = true;
    unsigned int count = 3;

    for (unsigned int i = 0; i < count; ++i) {
        m_Workers.emplace_back(&ChunkMeshGenerator::WorkerUpdate, this);
    }
}

ChunkMeshGenerator::~ChunkMeshGenerator() {
    m_Working = false;

    for (auto& worker : m_Workers) {
        worker.join();
    }

    m_World->UnregisterListener(this);
}

void ChunkMeshGenerator::OnBlockChange(mc::Vector3i position, mc::block::BlockPtr newBlock, mc::block::BlockPtr oldBlock) {
    terra::ChunkColumnPtr column = m_World->GetChunk(position);
    const mc::Vector3i chunk_base(column->GetMetadata().x * 16, (position.y / 16) * 16, column->GetMetadata().z * 16);

    auto iter = m_ChunkMeshes.find(chunk_base);

    if (iter == m_ChunkMeshes.end()) return;

    int chunk_x = column->GetMetadata().x;
    int chunk_y = static_cast<int>(position.y / 16);
    int chunk_z = column->GetMetadata().z;

    // TODO: Incremental update somehow?
    DestroyChunk(chunk_x, chunk_y, chunk_z);
    GenerateMesh(chunk_x, chunk_y, chunk_z);
}

void ChunkMeshGenerator::OnChunkLoad(terra::ChunkPtr chunk, const terra::ChunkColumnMetadata& meta, u16 index_y) {
    std::lock_guard<std::mutex> lock(m_QueueMutex);

    m_ChunkPushQueue.emplace(meta.x * 16, index_y * 16, meta.z * 16);
}

void ChunkMeshGenerator::WorkerUpdate() {
    while (m_Working) {
        if (m_ChunkBuildQueue.Empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        std::shared_ptr<ChunkMeshBuildContext> ctx;

        {
            std::lock_guard<std::mutex> lock(m_QueueMutex);

            if (m_ChunkBuildQueue.Empty()) continue;

            ctx = m_ChunkBuildQueue.Pop();
        }

        GenerateMesh(*ctx);
    }
}

void ChunkMeshGenerator::ProcessChunks() {
    const std::size_t kMaxMeshesPerFrame = 4;

    // Push any new chunks that were added this frame into the work queue
    for (std::size_t i = 0; i < kMaxMeshesPerFrame && !m_ChunkPushQueue.empty(); ++i) {
        mc::Vector3i chunk_base = m_ChunkPushQueue.front();
        m_ChunkPushQueue.pop();

        auto ctx = std::make_shared<ChunkMeshBuildContext>();

        ctx->world_position = chunk_base;
        auto chunk_y = chunk_base.y / 16;

        terra::ChunkColumnPtr column = m_World->GetChunk(chunk_base);

        // Cache the world data for this chunk so it can be pushed to another thread and built.
        for (int y = 0; y < 18; ++y) {
            for (int z = 0; z < 18; ++z) {
                for (int x = 0; x < 18; ++x) {
                    mc::Vector3i offset(x - 1, y - 1, z - 1);
                    mc::block::BlockPtr block;

                    if (column == nullptr || (offset.x < 0 || offset.x > 15 || offset.y < 0 || offset.y > 15 || offset.z < 0 || offset.z > 15)) {
                        block = m_World->GetBlock(chunk_base + offset);
                    } else {
                        block = column->GetBlock(offset + mc::Vector3i(0, chunk_y * 16, 0));
                    }

                    ctx->chunk_data[y * 18 * 18 + z * 18 + x] = block;
                }
            }
        }

        std::lock_guard<std::mutex> lock(m_QueueMutex);
        m_ChunkBuildQueue.Push(ctx);
    }

    std::vector<std::unique_ptr<VertexPush>> pushes;

    {
        std::lock_guard<std::mutex> lock(m_PushMutex);
        pushes = std::move(m_VertexPushes);
        m_VertexPushes = std::vector<std::unique_ptr<VertexPush>>();
    }

    for (auto&& push : pushes) {
        auto&& vertices = push->vertices;

        GLuint vao = 0;

        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);

        g_AssetCache->GetTextures().Bind();

        GLuint vbo;
        glGenBuffers(1, &vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * vertices->size(), &(*vertices)[0], GL_STATIC_DRAW);

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

        std::unique_ptr<terra::render::ChunkMesh> mesh = std::make_unique<terra::render::ChunkMesh>(vao, vbo, vertices->size());

        m_ChunkMeshes[push->pos] = std::move(mesh);
    }
}

int ChunkMeshGenerator::GetAmbientOcclusion(ChunkMeshBuildContext& context, AOCache& cache, const mc::Vector3i& side1, const mc::Vector3i& side2, const mc::Vector3i& corner) {
    auto iter1 = cache.find(side1);
    auto iter2 = cache.find(side2);
    auto iter_corner = cache.find(corner);

    int value1, value2, value_corner;

    if (iter1 != cache.end()) {
        value1 = iter1->second;
    } else {
        value1 = context.GetBlock(side1)->IsSolid();
    }

    if (iter2 != cache.end()) {
        value2 = iter2->second;
    } else {
        value2 = context.GetBlock(side2)->IsSolid();
    }

    if (iter_corner != cache.end()) {
        value_corner = iter_corner->second;
    } else {
        value_corner = context.GetBlock(corner)->IsSolid();
    }

    if (value1 && value2) {
        return 0;
    }

    return 3 - (value1 + value2 + value_corner);
}

void ChunkMeshGenerator::GenerateMesh(ChunkMeshBuildContext& context) {
    std::unique_ptr<std::vector<Vertex>> vertices = std::make_unique<std::vector<Vertex>>();

    AOCache cache;

    // Sweep through the blocks and generate vertices for the mesh
    for (int y = 0; y < 16; ++y) {
        for (int z = 0; z < 16; ++z) {
            for (int x = 0; x < 16; ++x) {
                mc::Vector3i mc_pos = context.world_position + mc::Vector3i(x, y, z);
                mc::block::BlockPtr block = context.GetBlock(mc_pos);
                
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

                const glm::vec3 base = terra::math::VecToGLM(mc_pos);

                mc::block::BlockPtr above = context.GetBlock(mc_pos + mc::Vector3i(0, 1, 0));
                if (above == nullptr || !above->IsSolid()) {
                    // Render the top face of the current block.
                    glm::vec3 bottom_left = base + glm::vec3(0, 1, 0);
                    glm::vec3 bottom_right = base + glm::vec3(0, 1, 1);
                    glm::vec3 top_left = base + glm::vec3(1, 1, 0);
                    glm::vec3 top_right = base + glm::vec3(1, 1, 1);

                    int obl = GetAmbientOcclusion(context, cache, mc_pos + mc::Vector3i(-1, 1, 0), mc_pos + mc::Vector3i(0, 1, -1), mc_pos + mc::Vector3i(-1, 1, -1));
                    int obr = GetAmbientOcclusion(context, cache, mc_pos + mc::Vector3i(-1, 1, 0), mc_pos + mc::Vector3i(0, 1, 1), mc_pos + mc::Vector3i(-1, 1, 1));
                    int otl = GetAmbientOcclusion(context, cache, mc_pos + mc::Vector3i(1, 1, 0), mc_pos + mc::Vector3i(0, 1, -1), mc_pos + mc::Vector3i(1, 1, -1));
                    int otr = GetAmbientOcclusion(context, cache, mc_pos + mc::Vector3i(1, 1, 0), mc_pos + mc::Vector3i(0, 1, 1), mc_pos + mc::Vector3i(1, 1, 1));

                    auto&& texture_path = model->GetTexturePath(terra::block::BlockFace::Top);
                    u32 texture = g_AssetCache->GetTextures().GetIndex(texture_path);

                    vertices->emplace_back(bottom_left, glm::vec3(0, 1, 0), glm::vec2(0, 1), texture, tint, obl);
                    vertices->emplace_back(bottom_right, glm::vec3(0, 1, 0), glm::vec2(0, 0), texture, tint, obr);
                    vertices->emplace_back(top_right, glm::vec3(0, 1, 0), glm::vec2(1, 0), texture, tint, otr);

                    vertices->emplace_back(top_right, glm::vec3(0, 1, 0), glm::vec2(1, 0), texture, tint, otr);
                    vertices->emplace_back(top_left, glm::vec3(0, 1, 0), glm::vec2(1, 1), texture, tint, otl);
                    vertices->emplace_back(bottom_left, glm::vec3(0, 1, 0), glm::vec2(0, 1), texture, tint, obl);
                }

                mc::block::BlockPtr below = context.GetBlock(mc_pos - mc::Vector3i(0, 1, 0));
                if (below == nullptr || !below->IsSolid()) {
                    // Render the bottom face of the current block.
                    glm::vec3 bottom_left = base + glm::vec3(1, 0, 0);
                    glm::vec3 bottom_right = base + glm::vec3(1, 0, 1);
                    glm::vec3 top_left = base + glm::vec3(0, 0, 0);
                    glm::vec3 top_right = base + glm::vec3(0, 0, 1);

                    int obl = GetAmbientOcclusion(context, cache, mc_pos + mc::Vector3i(1, -1, 0), mc_pos + mc::Vector3i(0, -1, -1), mc_pos + mc::Vector3i(1, -1, -1));
                    int obr = GetAmbientOcclusion(context, cache, mc_pos + mc::Vector3i(1, -1, 0), mc_pos + mc::Vector3i(0, -1, 1), mc_pos + mc::Vector3i(1, -1, 1));
                    int otl = GetAmbientOcclusion(context, cache, mc_pos + mc::Vector3i(-1, -1, 0), mc_pos + mc::Vector3i(0, -1, -1), mc_pos + mc::Vector3i(-1, -1, -1));
                    int otr = GetAmbientOcclusion(context, cache, mc_pos + mc::Vector3i(-1, -1, 0), mc_pos + mc::Vector3i(0, -1, 1), mc_pos + mc::Vector3i(-1, -1, 1));

                    auto&& texture_path = model->GetTexturePath(terra::block::BlockFace::Bottom);
                    u32 texture = g_AssetCache->GetTextures().GetIndex(texture_path);

                    vertices->emplace_back(bottom_left, glm::vec3(0, -1, 0), glm::vec2(1, 0), texture, side_tint, obl);
                    vertices->emplace_back(bottom_right, glm::vec3(0, -1, 0), glm::vec2(1, 1), texture, side_tint, obr);
                    vertices->emplace_back(top_right, glm::vec3(0, -1, 0), glm::vec2(0, 1), texture, side_tint, otr);

                    vertices->emplace_back(top_right, glm::vec3(0, -1, 0), glm::vec2(0, 1), texture, side_tint, otr);
                    vertices->emplace_back(top_left, glm::vec3(0, -1, 0), glm::vec2(0, 0), texture, side_tint, otl);
                    vertices->emplace_back(bottom_left, glm::vec3(0, -1, 0), glm::vec2(1, 0), texture, side_tint, obl);
                }

                mc::block::BlockPtr north = context.GetBlock(mc_pos + mc::Vector3i(0, 0, -1));
                if (north == nullptr || !north->IsSolid()) {
                    // Render the north face of the current block.
                    glm::vec3 bottom_left = base + glm::vec3(1, 0, 0);
                    glm::vec3 bottom_right = base + glm::vec3(0, 0, 0);
                    glm::vec3 top_left = base + glm::vec3(1, 1, 0);
                    glm::vec3 top_right = base + glm::vec3(0, 1, 0);

                    int obl = GetAmbientOcclusion(context, cache, mc_pos + mc::Vector3i(1, 0, -1), mc_pos + mc::Vector3i(0, -1, -1), mc_pos + mc::Vector3i(1, -1, -1));
                    int obr = GetAmbientOcclusion(context, cache, mc_pos + mc::Vector3i(0, -1, -1), mc_pos + mc::Vector3i(-1, 0, -1), mc_pos + mc::Vector3i(-1, -1, -1));
                    int otl = GetAmbientOcclusion(context, cache, mc_pos + mc::Vector3i(0, 1, -1), mc_pos + mc::Vector3i(1, 0, -1), mc_pos + mc::Vector3i(1, 1, -1));
                    int otr = GetAmbientOcclusion(context, cache, mc_pos + mc::Vector3i(0, 1, -1), mc_pos + mc::Vector3i(-1, 0, -1), mc_pos + mc::Vector3i(-1, 1, -1));

                    auto&& texture_path = model->GetTexturePath(terra::block::BlockFace::North);
                    u32 texture = g_AssetCache->GetTextures().GetIndex(texture_path);

                    vertices->emplace_back(bottom_left, glm::vec3(0, 0, -1), glm::vec2(0, 0), texture, side_tint, obl);
                    vertices->emplace_back(bottom_right, glm::vec3(0, 0, -1), glm::vec2(1, 0), texture, side_tint, obr);
                    vertices->emplace_back(top_right, glm::vec3(0, 0, -1), glm::vec2(1, 1), texture, side_tint, otr);

                    vertices->emplace_back(top_right, glm::vec3(0, 0, -1), glm::vec2(1, 1), texture, side_tint, otr);
                    vertices->emplace_back(top_left, glm::vec3(0, 0, -1), glm::vec2(0, 1), texture, side_tint, otl);
                    vertices->emplace_back(bottom_left, glm::vec3(0, 0, -1), glm::vec2(0, 0), texture, side_tint, obl);
                }

                mc::block::BlockPtr south = context.GetBlock(mc_pos + mc::Vector3i(0, 0, 1));
                if (south == nullptr || !south->IsSolid()) {
                    // Render the south face of the current block.
                    glm::vec3 bottom_left = base + glm::vec3(0, 0, 1);
                    glm::vec3 bottom_right = base + glm::vec3(1, 0, 1);
                    glm::vec3 top_left = base + glm::vec3(0, 1, 1);
                    glm::vec3 top_right = base + glm::vec3(1, 1, 1);

                    int obl = GetAmbientOcclusion(context, cache, mc_pos + mc::Vector3i(-1, 0, 1), mc_pos + mc::Vector3i(0, -1, 1), mc_pos + mc::Vector3i(-1, -1, 1));
                    int obr = GetAmbientOcclusion(context, cache, mc_pos + mc::Vector3i(1, 0, 1), mc_pos + mc::Vector3i(0, -1, 1), mc_pos + mc::Vector3i(1, -1, 1));
                    int otl = GetAmbientOcclusion(context, cache, mc_pos + mc::Vector3i(0, 1, 1), mc_pos + mc::Vector3i(-1, 0, 1), mc_pos + mc::Vector3i(-1, 1, 1));
                    int otr = GetAmbientOcclusion(context, cache, mc_pos + mc::Vector3i(0, 1, 1), mc_pos + mc::Vector3i(1, 0, 1), mc_pos + mc::Vector3i(1, 1, 1));

                    auto&& texture_path = model->GetTexturePath(terra::block::BlockFace::South);
                    u32 texture = g_AssetCache->GetTextures().GetIndex(texture_path);

                    vertices->emplace_back(bottom_left, glm::vec3(0, 0, 1), glm::vec2(0, 0), texture, side_tint, obl);
                    vertices->emplace_back(bottom_right, glm::vec3(0, 0, 1), glm::vec2(1, 0), texture, side_tint, obr);
                    vertices->emplace_back(top_right, glm::vec3(0, 0, 1), glm::vec2(1, 1), texture, side_tint, otr);

                    vertices->emplace_back(top_right, glm::vec3(0, 0, 1), glm::vec2(1, 1), texture, side_tint, otr);
                    vertices->emplace_back(top_left, glm::vec3(0, 0, 1), glm::vec2(0, 1), texture, side_tint, otl);
                    vertices->emplace_back(bottom_left, glm::vec3(0, 0, 1), glm::vec2(0, 0), texture, side_tint, obl);
                }

                mc::block::BlockPtr east = context.GetBlock(mc_pos + mc::Vector3i(1, 0, 0));
                if (east == nullptr || !east->IsSolid()) {
                    // Render the east face of the current block.
                    glm::vec3 bottom_left = base + glm::vec3(1, 0, 1);
                    glm::vec3 bottom_right = base + glm::vec3(1, 0, 0);
                    glm::vec3 top_left = base + glm::vec3(1, 1, 1);
                    glm::vec3 top_right = base + glm::vec3(1, 1, 0);

                    int obl = GetAmbientOcclusion(context, cache, mc_pos + mc::Vector3i(1, 0, 1), mc_pos + mc::Vector3i(1, -1, 0), mc_pos + mc::Vector3i(1, -1, 1));
                    int obr = GetAmbientOcclusion(context, cache, mc_pos + mc::Vector3i(1, -1, 0), mc_pos + mc::Vector3i(1, 0, -1), mc_pos + mc::Vector3i(1, -1, -1));
                    int otl = GetAmbientOcclusion(context, cache, mc_pos + mc::Vector3i(1, 1, 0), mc_pos + mc::Vector3i(1, 0, 1), mc_pos + mc::Vector3i(1, 1, 1));
                    int otr = GetAmbientOcclusion(context, cache, mc_pos + mc::Vector3i(1, 1, 0), mc_pos + mc::Vector3i(1, 0, -1), mc_pos + mc::Vector3i(1, 1, -1));

                    auto&& texture_path = model->GetTexturePath(terra::block::BlockFace::East);
                    u32 texture = g_AssetCache->GetTextures().GetIndex(texture_path);

                    vertices->emplace_back(bottom_left, glm::vec3(0, 0, -1), glm::vec2(0, 0), texture, side_tint, obl);
                    vertices->emplace_back(bottom_right, glm::vec3(0, 0, -1), glm::vec2(1, 0), texture, side_tint, obr);
                    vertices->emplace_back(top_right, glm::vec3(0, 0, -1), glm::vec2(1, 1), texture, side_tint, otr);

                    vertices->emplace_back(top_right, glm::vec3(0, 0, -1), glm::vec2(1, 1), texture, side_tint, otr);
                    vertices->emplace_back(top_left, glm::vec3(0, 0, -1), glm::vec2(0, 1), texture, side_tint, otl);
                    vertices->emplace_back(bottom_left, glm::vec3(0, 0, -1), glm::vec2(0, 0), texture, side_tint, obl);
                }

                mc::block::BlockPtr west = context.GetBlock(mc_pos + mc::Vector3i(-1, 0, 0));
                if (west == nullptr || !west->IsSolid()) {
                    // Render the west face of the current block.
                    glm::vec3 bottom_left = base + glm::vec3(0, 0, 0);
                    glm::vec3 bottom_right = base + glm::vec3(0, 0, 1);
                    glm::vec3 top_left = base + glm::vec3(0, 1, 0);
                    glm::vec3 top_right = base + glm::vec3(0, 1, 1);

                    int obl = GetAmbientOcclusion(context, cache, mc_pos + mc::Vector3i(-1, -1, 0), mc_pos + mc::Vector3i(-1, 0, -1), mc_pos + mc::Vector3i(-1, -1, -1));
                    int obr = GetAmbientOcclusion(context, cache, mc_pos + mc::Vector3i(-1, -1, 0), mc_pos + mc::Vector3i(-1, 0, 1), mc_pos + mc::Vector3i(-1, -1, 1));
                    int otl = GetAmbientOcclusion(context, cache, mc_pos + mc::Vector3i(-1, 1, 0), mc_pos + mc::Vector3i(-1, 0, -1), mc_pos + mc::Vector3i(-1, 1, -1));
                    int otr = GetAmbientOcclusion(context, cache, mc_pos + mc::Vector3i(-1, 1, 0), mc_pos + mc::Vector3i(-1, 0, 1), mc_pos + mc::Vector3i(-1, 1, 1));

                    auto&& texture_path = model->GetTexturePath(terra::block::BlockFace::West);
                    u32 texture = g_AssetCache->GetTextures().GetIndex(texture_path);

                    vertices->emplace_back(bottom_left, glm::vec3(0, 0, -1), glm::vec2(0, 0), texture, side_tint, obl);
                    vertices->emplace_back(bottom_right, glm::vec3(0, 0, -1), glm::vec2(1, 0), texture, side_tint, obr);
                    vertices->emplace_back(top_right, glm::vec3(0, 0, -1), glm::vec2(1, 1), texture, side_tint, otr);

                    vertices->emplace_back(top_right, glm::vec3(0, 0, -1), glm::vec2(1, 1), texture, side_tint, otr);
                    vertices->emplace_back(top_left, glm::vec3(0, 0, -1), glm::vec2(0, 1), texture, side_tint, otl);
                    vertices->emplace_back(bottom_left, glm::vec3(0, 0, -1), glm::vec2(0, 0), texture, side_tint, obl);
                }
            }
        }
    }

    if (vertices->empty()) return;

    std::unique_ptr<VertexPush> push = std::make_unique<VertexPush>(context.world_position, std::move(vertices));

    std::lock_guard<std::mutex> lock(m_PushMutex);
    m_VertexPushes.push_back(std::move(push));
}

void ChunkMeshGenerator::GenerateMesh(s64 chunk_x, s64 chunk_y, s64 chunk_z) {
    ChunkMeshBuildContext ctx;

    ctx.world_position = mc::Vector3i(chunk_x * 16, chunk_y * 16, chunk_z * 16);

    for (int y = 0; y < 18; ++y) {
        for (int z = 0; z < 18; ++z) {
            for (int x = 0; x < 18; ++x) {
                mc::Vector3i offset(x - 1, y - 1, z - 1);
                mc::block::BlockPtr block = m_World->GetBlock(ctx.world_position + offset);

                ctx.chunk_data[y * 18 * 18 + z * 18 + x] = block;
            }
        }
    }

    GenerateMesh(ctx);
}

void ChunkMeshGenerator::OnChunkUnload(terra::ChunkColumnPtr chunk) {
    if (chunk == nullptr) return;

    for (int y = 0; y < 16; ++y) {
        DestroyChunk(chunk->GetMetadata().x, y, chunk->GetMetadata().z);
    }
}

void ChunkMeshGenerator::DestroyChunk(s64 chunk_x, s64 chunk_y, s64 chunk_z) {
    mc::Vector3i key(chunk_x * 16, chunk_y * 16, chunk_z * 16);

    auto iter = m_ChunkMeshes.find(key);
    if (iter != m_ChunkMeshes.end()) {
        iter->second->Destroy();
        m_ChunkMeshes.erase(key);
    }
}

} // ns render
} // ns terra
