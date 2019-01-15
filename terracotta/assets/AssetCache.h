#ifndef TERRACOTTA_ASSETS_ASSETCACHE_H_
#define TERRACOTTA_ASSETS_ASSETCACHE_H_

#include "TextureArray.h"

#include <unordered_map>
#include <string>
#include <memory>
#include "../block/BlockState.h"
#include "../block/BlockModel.h"

namespace terra {
namespace assets {

class ZipArchive;

class AssetCache {
public:
    // Initialize an asset cache with the minecraft jar and blocks json.
    bool Initialize(const std::string& archive_path, const std::string& blocks_path);

    TextureArray& GetTextures() { return m_TextureArray; }
    terra::block::BlockState* GetBlockState(u32 block_id) const;
    terra::block::BlockModel* GetBlockModel(const std::string& model_path) const;
    terra::block::BlockModel* GetVariantModel(const std::string& block_name, const std::string& variant);

private:
    bool LoadBlockStates(const std::string& filename);
    bool LoadTextures(terra::assets::ZipArchive& archive);
    std::size_t LoadBlockModels(terra::assets::ZipArchive& archive);
    void LoadBlockVariants(terra::assets::ZipArchive& archive);

    // Maps block id to the BlockState
    std::unordered_map<u32, std::unique_ptr<terra::block::BlockState>> m_BlockStates;
    // Maps model path to a BlockModel
    std::unordered_map<std::string, std::unique_ptr<terra::block::BlockModel>> m_BlockModels;
    // Block name -> (Variant String -> BlockModel)
    std::unordered_map<std::string, std::unordered_map<std::string, terra::block::BlockModel*>> m_BlockVariants;

    TextureArray m_TextureArray;
};

} // assets
} // terra

extern std::unique_ptr<terra::assets::AssetCache> g_AssetCache;

#endif
