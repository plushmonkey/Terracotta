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
    const std::size_t kMaxMeshesPerFrame = 32;

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

int ChunkMeshGenerator::GetAmbientOcclusion(ChunkMeshBuildContext& context, const mc::Vector3i& side1, const mc::Vector3i& side2, const mc::Vector3i& corner) {
    int value1, value2, value_corner;

    value1 = context.GetBlock(side1)->IsSolid();
    value2 = context.GetBlock(side2)->IsSolid();
    value_corner = context.GetBlock(corner)->IsSolid();

    if (value1 && value2) {
        return 0;
    }

    return 3 - (value1 + value2 + value_corner);
}

bool ChunkMeshGenerator::IsOccluding(terra::block::BlockModel* from, terra::block::BlockFace face, mc::block::BlockPtr test_block) {
    for (auto& element : from->GetElements()) {
        if (element.GetFace(face).cull_face == block::BlockFace::None) {
            return false;
        }
    }

    terra::block::BlockModel* model = g_AssetCache->GetVariant(test_block);
    if (model == nullptr) return false;

    bool is_full = false;

    block::BlockFace opposite = block::get_opposite_face(face);
    for (auto& element : model->GetElements()) {
        if (element.GetFrom() == glm::vec3(0, 0, 0) && element.GetTo() == glm::vec3(1, 1, 1)) {
            is_full = true;
        }

        if (g_AssetCache->GetTextures().IsTransparent(element.GetFace(opposite).texture)) {
            return false;
        }
    }
    
    return is_full;
}

void ChunkMeshGenerator::GenerateMesh(ChunkMeshBuildContext& context) {
    std::unique_ptr<std::vector<Vertex>> vertices = std::make_unique<std::vector<Vertex>>();

    static const glm::vec3 kTints[] = {
        glm::vec3(1.0, 1.0, 1.0),
        glm::vec3(137 / 255.0, 191 / 255.0, 98 / 255.0), // Grass
        glm::vec3(0.22, 0.60, 0.21), // Leaves
    };

    // Sweep through the blocks and generate vertices for the mesh
    for (int y = 0; y < 16; ++y) {
        for (int z = 0; z < 16; ++z) {
            for (int x = 0; x < 16; ++x) {
                mc::Vector3i mc_pos = context.world_position + mc::Vector3i(x, y, z);
                mc::block::BlockPtr block = context.GetBlock(mc_pos);
                terra::block::BlockModel* model = g_AssetCache->GetVariant(block);

                if (model == nullptr || model->GetElements().empty()) continue;

                const glm::vec3 base = terra::math::VecToGLM(mc_pos);

                mc::block::BlockPtr above = context.GetBlock(mc_pos + mc::Vector3i(0, 1, 0));
                if (!IsOccluding(model, block::BlockFace::Up, above)) {
                    // Render the top face of the current block.
                    int obl = GetAmbientOcclusion(context, mc_pos + mc::Vector3i(-1, 1, 0), mc_pos + mc::Vector3i(0, 1, -1), mc_pos + mc::Vector3i(-1, 1, -1));
                    int obr = GetAmbientOcclusion(context, mc_pos + mc::Vector3i(-1, 1, 0), mc_pos + mc::Vector3i(0, 1, 1), mc_pos + mc::Vector3i(-1, 1, 1));
                    int otl = GetAmbientOcclusion(context, mc_pos + mc::Vector3i(1, 1, 0), mc_pos + mc::Vector3i(0, 1, -1), mc_pos + mc::Vector3i(1, 1, -1));
                    int otr = GetAmbientOcclusion(context, mc_pos + mc::Vector3i(1, 1, 0), mc_pos + mc::Vector3i(0, 1, 1), mc_pos + mc::Vector3i(1, 1, 1));

                    for (const auto& element : model->GetElements()) {
                        block::RenderableFace renderable = element.GetFace(block::BlockFace::Up);
                        if (renderable.face == block::BlockFace::Up) {
                            assets::TextureHandle texture = renderable.texture;

                            const auto& from = element.GetFrom();
                            const auto& to = element.GetTo();

                            glm::vec3 bottom_left = base + glm::vec3(from.x, to.y, from.z);
                            glm::vec3 bottom_right = base + glm::vec3(from.x, to.y, to.z);
                            glm::vec3 top_left = base + glm::vec3(to.x, to.y, from.z);
                            glm::vec3 top_right = base + glm::vec3(to.x, to.y, to.z);

                            const glm::vec3& tint = kTints[renderable.tint_index + 1];

                            vertices->emplace_back(bottom_left, glm::vec3(0, 1, 0), glm::vec2(0, 1), texture, tint, obl);
                            vertices->emplace_back(bottom_right, glm::vec3(0, 1, 0), glm::vec2(0, 0), texture, tint, obr);
                            vertices->emplace_back(top_right, glm::vec3(0, 1, 0), glm::vec2(1, 0), texture, tint, otr);

                            vertices->emplace_back(top_right, glm::vec3(0, 1, 0), glm::vec2(1, 0), texture, tint, otr);
                            vertices->emplace_back(top_left, glm::vec3(0, 1, 0), glm::vec2(1, 1), texture, tint, otl);
                            vertices->emplace_back(bottom_left, glm::vec3(0, 1, 0), glm::vec2(0, 1), texture, tint, obl);
                        }
                    }
                }

                mc::block::BlockPtr below = context.GetBlock(mc_pos - mc::Vector3i(0, 1, 0));
                if (!IsOccluding(model, block::BlockFace::Down, below)) {
                    // Render the bottom face of the current block.
                    int obl = GetAmbientOcclusion(context, mc_pos + mc::Vector3i(1, -1, 0), mc_pos + mc::Vector3i(0, -1, -1), mc_pos + mc::Vector3i(1, -1, -1));
                    int obr = GetAmbientOcclusion(context, mc_pos + mc::Vector3i(1, -1, 0), mc_pos + mc::Vector3i(0, -1, 1), mc_pos + mc::Vector3i(1, -1, 1));
                    int otl = GetAmbientOcclusion(context, mc_pos + mc::Vector3i(-1, -1, 0), mc_pos + mc::Vector3i(0, -1, -1), mc_pos + mc::Vector3i(-1, -1, -1));
                    int otr = GetAmbientOcclusion(context, mc_pos + mc::Vector3i(-1, -1, 0), mc_pos + mc::Vector3i(0, -1, 1), mc_pos + mc::Vector3i(-1, -1, 1));

                    for (const auto& element : model->GetElements()) {
                        block::RenderableFace renderable = element.GetFace(block::BlockFace::Down);

                        if (renderable.face == block::BlockFace::Down) {
                            assets::TextureHandle texture = renderable.texture;

                            const auto& from = element.GetFrom();
                            const auto& to = element.GetTo();

                            glm::vec3 bottom_left = base + glm::vec3(to.x, from.y, from.z);
                            glm::vec3 bottom_right = base + glm::vec3(to.x, from.y, to.z);
                            glm::vec3 top_left = base + glm::vec3(from.x, from.y, from.z);
                            glm::vec3 top_right = base + glm::vec3(from.x, from.y, to.z);

                            const glm::vec3& tint = kTints[renderable.tint_index + 1];

                            vertices->emplace_back(bottom_left, glm::vec3(0, -1, 0), glm::vec2(1, 0), texture, tint, obl);
                            vertices->emplace_back(bottom_right, glm::vec3(0, -1, 0), glm::vec2(1, 1), texture, tint, obr);
                            vertices->emplace_back(top_right, glm::vec3(0, -1, 0), glm::vec2(0, 1), texture, tint, otr);

                            vertices->emplace_back(top_right, glm::vec3(0, -1, 0), glm::vec2(0, 1), texture, tint, otr);
                            vertices->emplace_back(top_left, glm::vec3(0, -1, 0), glm::vec2(0, 0), texture, tint, otl);
                            vertices->emplace_back(bottom_left, glm::vec3(0, -1, 0), glm::vec2(1, 0), texture, tint, obl);
                        }
                    }
                }

                mc::block::BlockPtr north = context.GetBlock(mc_pos + mc::Vector3i(0, 0, -1));
                if (!IsOccluding(model, block::BlockFace::North, north)) {
                    // Render the north face of the current block.
                    int obl = GetAmbientOcclusion(context, mc_pos + mc::Vector3i(1, 0, -1), mc_pos + mc::Vector3i(0, -1, -1), mc_pos + mc::Vector3i(1, -1, -1));
                    int obr = GetAmbientOcclusion(context, mc_pos + mc::Vector3i(0, -1, -1), mc_pos + mc::Vector3i(-1, 0, -1), mc_pos + mc::Vector3i(-1, -1, -1));
                    int otl = GetAmbientOcclusion(context, mc_pos + mc::Vector3i(0, 1, -1), mc_pos + mc::Vector3i(1, 0, -1), mc_pos + mc::Vector3i(1, 1, -1));
                    int otr = GetAmbientOcclusion(context, mc_pos + mc::Vector3i(0, 1, -1), mc_pos + mc::Vector3i(-1, 0, -1), mc_pos + mc::Vector3i(-1, 1, -1));

                    for (const auto& element : model->GetElements()) {
                        block::RenderableFace renderable = element.GetFace(block::BlockFace::North);

                        if (renderable.face == block::BlockFace::North) {
                            assets::TextureHandle texture = renderable.texture;

                            const auto& from = element.GetFrom();
                            const auto& to = element.GetTo();

                            glm::vec3 bottom_left = base + glm::vec3(to.x, from.y, from.z);
                            glm::vec3 bottom_right = base + glm::vec3(from.x, from.y, from.z);
                            glm::vec3 top_left = base + glm::vec3(to.x, to.y, from.z);
                            glm::vec3 top_right = base + glm::vec3(from.x, to.y, from.z);

                            const glm::vec3& tint = kTints[renderable.tint_index + 1];

                            vertices->emplace_back(bottom_left, glm::vec3(0, 0, -1), glm::vec2(0, 0), texture, tint, obl);
                            vertices->emplace_back(bottom_right, glm::vec3(0, 0, -1), glm::vec2(1, 0), texture, tint, obr);
                            vertices->emplace_back(top_right, glm::vec3(0, 0, -1), glm::vec2(1, 1), texture, tint, otr);

                            vertices->emplace_back(top_right, glm::vec3(0, 0, -1), glm::vec2(1, 1), texture, tint, otr);
                            vertices->emplace_back(top_left, glm::vec3(0, 0, -1), glm::vec2(0, 1), texture, tint, otl);
                            vertices->emplace_back(bottom_left, glm::vec3(0, 0, -1), glm::vec2(0, 0), texture, tint, obl);
                        }
                    }
                }

                mc::block::BlockPtr south = context.GetBlock(mc_pos + mc::Vector3i(0, 0, 1));
                if (!IsOccluding(model, block::BlockFace::South, south)) {
                    // Render the south face of the current block.
                    int obl = GetAmbientOcclusion(context, mc_pos + mc::Vector3i(-1, 0, 1), mc_pos + mc::Vector3i(0, -1, 1), mc_pos + mc::Vector3i(-1, -1, 1));
                    int obr = GetAmbientOcclusion(context, mc_pos + mc::Vector3i(1, 0, 1), mc_pos + mc::Vector3i(0, -1, 1), mc_pos + mc::Vector3i(1, -1, 1));
                    int otl = GetAmbientOcclusion(context, mc_pos + mc::Vector3i(0, 1, 1), mc_pos + mc::Vector3i(-1, 0, 1), mc_pos + mc::Vector3i(-1, 1, 1));
                    int otr = GetAmbientOcclusion(context, mc_pos + mc::Vector3i(0, 1, 1), mc_pos + mc::Vector3i(1, 0, 1), mc_pos + mc::Vector3i(1, 1, 1));

                    for (const auto& element : model->GetElements()) {
                        block::RenderableFace renderable = element.GetFace(block::BlockFace::South);

                        if (renderable.face == block::BlockFace::South) {
                            assets::TextureHandle texture = renderable.texture;

                            const auto& from = element.GetFrom();
                            const auto& to = element.GetTo();

                            glm::vec3 bottom_left = base + glm::vec3(from.x, from.y, to.z);
                            glm::vec3 bottom_right = base + glm::vec3(to.x, from.y, to.z);
                            glm::vec3 top_left = base + glm::vec3(from.x, to.y, to.z);
                            glm::vec3 top_right = base + glm::vec3(to.x, to.y, to.z);

                            const glm::vec3& tint = kTints[renderable.tint_index + 1];

                            vertices->emplace_back(bottom_left, glm::vec3(0, 0, 1), glm::vec2(0, 0), texture, tint, obl);
                            vertices->emplace_back(bottom_right, glm::vec3(0, 0, 1), glm::vec2(1, 0), texture, tint, obr);
                            vertices->emplace_back(top_right, glm::vec3(0, 0, 1), glm::vec2(1, 1), texture, tint, otr);

                            vertices->emplace_back(top_right, glm::vec3(0, 0, 1), glm::vec2(1, 1), texture, tint, otr);
                            vertices->emplace_back(top_left, glm::vec3(0, 0, 1), glm::vec2(0, 1), texture, tint, otl);
                            vertices->emplace_back(bottom_left, glm::vec3(0, 0, 1), glm::vec2(0, 0), texture, tint, obl);
                        }
                    }
                }

                mc::block::BlockPtr east = context.GetBlock(mc_pos + mc::Vector3i(1, 0, 0));
                if (!IsOccluding(model, block::BlockFace::East, east)) {
                    // Render the east face of the current block.
                    int obl = GetAmbientOcclusion(context, mc_pos + mc::Vector3i(1, 0, 1), mc_pos + mc::Vector3i(1, -1, 0), mc_pos + mc::Vector3i(1, -1, 1));
                    int obr = GetAmbientOcclusion(context, mc_pos + mc::Vector3i(1, -1, 0), mc_pos + mc::Vector3i(1, 0, -1), mc_pos + mc::Vector3i(1, -1, -1));
                    int otl = GetAmbientOcclusion(context, mc_pos + mc::Vector3i(1, 1, 0), mc_pos + mc::Vector3i(1, 0, 1), mc_pos + mc::Vector3i(1, 1, 1));
                    int otr = GetAmbientOcclusion(context, mc_pos + mc::Vector3i(1, 1, 0), mc_pos + mc::Vector3i(1, 0, -1), mc_pos + mc::Vector3i(1, 1, -1));

                    for (const auto& element : model->GetElements()) {
                        block::RenderableFace renderable = element.GetFace(block::BlockFace::East);

                        if (renderable.face == block::BlockFace::East) {
                            assets::TextureHandle texture = renderable.texture;

                            const auto& from = element.GetFrom();
                            const auto& to = element.GetTo();

                            glm::vec3 bottom_left = base + glm::vec3(to.x, from.y, to.z);
                            glm::vec3 bottom_right = base + glm::vec3(to.x, from.y, from.z);
                            glm::vec3 top_left = base + glm::vec3(to.x, to.y, to.z);
                            glm::vec3 top_right = base + glm::vec3(to.x, to.y, from.z);

                            const glm::vec3& tint = kTints[renderable.tint_index + 1];

                            vertices->emplace_back(bottom_left, glm::vec3(0, 0, -1), glm::vec2(0, 0), texture, tint, obl);
                            vertices->emplace_back(bottom_right, glm::vec3(0, 0, -1), glm::vec2(1, 0), texture, tint, obr);
                            vertices->emplace_back(top_right, glm::vec3(0, 0, -1), glm::vec2(1, 1), texture, tint, otr);

                            vertices->emplace_back(top_right, glm::vec3(0, 0, -1), glm::vec2(1, 1), texture, tint, otr);
                            vertices->emplace_back(top_left, glm::vec3(0, 0, -1), glm::vec2(0, 1), texture, tint, otl);
                            vertices->emplace_back(bottom_left, glm::vec3(0, 0, -1), glm::vec2(0, 0), texture, tint, obl);
                        }
                    }
                }

                mc::block::BlockPtr west = context.GetBlock(mc_pos + mc::Vector3i(-1, 0, 0));
                if (!IsOccluding(model, block::BlockFace::West, west)) {
                    // Render the west face of the current block.
                    int obl = GetAmbientOcclusion(context, mc_pos + mc::Vector3i(-1, -1, 0), mc_pos + mc::Vector3i(-1, 0, -1), mc_pos + mc::Vector3i(-1, -1, -1));
                    int obr = GetAmbientOcclusion(context, mc_pos + mc::Vector3i(-1, -1, 0), mc_pos + mc::Vector3i(-1, 0, 1), mc_pos + mc::Vector3i(-1, -1, 1));
                    int otl = GetAmbientOcclusion(context, mc_pos + mc::Vector3i(-1, 1, 0), mc_pos + mc::Vector3i(-1, 0, -1), mc_pos + mc::Vector3i(-1, 1, -1));
                    int otr = GetAmbientOcclusion(context, mc_pos + mc::Vector3i(-1, 1, 0), mc_pos + mc::Vector3i(-1, 0, 1), mc_pos + mc::Vector3i(-1, 1, 1));

                    for (const auto& element : model->GetElements()) {
                        block::RenderableFace renderable = element.GetFace(block::BlockFace::West);

                        if (renderable.face == block::BlockFace::West) {
                            assets::TextureHandle texture = renderable.texture;

                            const auto& from = element.GetFrom();
                            const auto& to = element.GetTo();

                            glm::vec3 bottom_left = base + glm::vec3(from.x, from.y, from.z);
                            glm::vec3 bottom_right = base + glm::vec3(from.x, from.y, to.z);
                            glm::vec3 top_left = base + glm::vec3(from.x, to.y, from.z);
                            glm::vec3 top_right = base + glm::vec3(from.x, to.y, to.z);

                            const glm::vec3& tint = kTints[renderable.tint_index + 1];

                            vertices->emplace_back(bottom_left, glm::vec3(0, 0, -1), glm::vec2(0, 0), texture, tint, obl);
                            vertices->emplace_back(bottom_right, glm::vec3(0, 0, -1), glm::vec2(1, 0), texture, tint, obr);
                            vertices->emplace_back(top_right, glm::vec3(0, 0, -1), glm::vec2(1, 1), texture, tint, otr);

                            vertices->emplace_back(top_right, glm::vec3(0, 0, -1), glm::vec2(1, 1), texture, tint, otr);
                            vertices->emplace_back(top_left, glm::vec3(0, 0, -1), glm::vec2(0, 1), texture, tint, otl);
                            vertices->emplace_back(bottom_left, glm::vec3(0, 0, -1), glm::vec2(0, 0), texture, tint, obl);
                        }
                    }
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
