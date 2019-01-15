#ifndef TERRACOTTA_RENDER_CHUNKMESHGENERATOR_H_
#define TERRACOTTA_RENDER_CHUNKMESHGENERATOR_H_

#include "ChunkMesh.h"
#include <mclib/world/World.h>
#include <unordered_map>
#include <memory>
#include <utility>
#include <mclib/common/Vector.h>

namespace std {
template <> struct hash<mc::Vector3i> {
    std::size_t operator()(const mc::Vector3i& s) const noexcept {
        std::size_t seed = 3;
        for (int i = 0; i < 3; ++i) {
            seed ^= i + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        }
        return seed;
    }
};
}

namespace terra {
namespace render {

class ChunkMeshGenerator : public mc::world::WorldListener {
public:
    using iterator = std::unordered_map<mc::Vector3i, std::unique_ptr<terra::render::ChunkMesh>>::iterator;

    ChunkMeshGenerator(mc::world::World* world);
    ~ChunkMeshGenerator();

    void OnBlockChange(mc::Vector3i position, mc::block::BlockPtr newBlock, mc::block::BlockPtr oldBlock);
    void OnChunkLoad(mc::world::ChunkPtr chunk, const mc::world::ChunkColumnMetadata& meta, u16 index_y);
    void OnChunkUnload(mc::world::ChunkColumnPtr chunk);

    void GenerateMesh(mc::world::ChunkPtr chunk, int chunk_x, int chunk_y, int chunk_z);
    void DestroyChunk(int chunk_x, int chunk_y, int chunk_z);

    iterator begin() { return m_ChunkMeshes.begin(); }
    iterator end() { return m_ChunkMeshes.end(); }

private:
    mc::world::World* m_World;
    std::unordered_map<mc::Vector3i, std::unique_ptr<terra::render::ChunkMesh>> m_ChunkMeshes;
};

} // ns render
} // ns terra

#endif
