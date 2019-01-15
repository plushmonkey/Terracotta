#include "AssetCache.h"
#include "zip/ZipArchive.h"

#include <mclib/common/Json.h>
#include <mclib/block/Block.h>
#include <fstream>
#include <iostream>

#include "stb_image.h"

namespace terra {
namespace assets {

bool AssetCache::Initialize(const std::string& archive_path, const std::string& blocks_path) {
    if (!LoadBlockStates(blocks_path)) {
        return false;
    }

    terra::assets::ZipArchive archive;

    if (!archive.Open("1.13.2.jar")) {
        std::cerr << "Failed to open archive.\n";
        return false;
    }

    if (!LoadTextures(archive)) {
        std::cerr << "Failed to load textures.\n";
        return false;
    }

    if (LoadBlockModels(archive) <= 0) {
        return false;
    }

    LoadBlockVariants(archive);

    return true;
}

terra::block::BlockState* AssetCache::GetBlockState(u32 block_id) const {
    auto iter = m_BlockStates.find(block_id);

    if (iter == m_BlockStates.end()) return nullptr;

    return iter->second.get();
}

terra::block::BlockModel* AssetCache::GetBlockModel(const std::string& model_path) const {
    auto iter = m_BlockModels.find(model_path);

    if (iter == m_BlockModels.end()) return nullptr;

    return iter->second.get();
}

terra::block::BlockModel* AssetCache::GetVariantModel(const std::string& block_name, const std::string& variant) {
    auto block_iter = m_BlockVariants.find(block_name);

    if (block_iter == m_BlockVariants.end()) return nullptr;

    for (auto&& kv : block_iter->second) {
        if (variant.find(kv.first) != std::string::npos) {
            return kv.second;
        }
    }

    return nullptr;
}

bool AssetCache::LoadBlockStates(const std::string& filename) {
    std::ifstream in(filename);

    if (!in) {
        std::cerr << "Failed to open " << filename << std::endl;
        return false;
    }

    std::string data;
    in.seekg(0, std::ios::end);
    std::size_t size = static_cast<std::size_t>(in.tellg());
    in.seekg(0, std::ios::beg);

    data.resize(size);

    in.read(&data[0], size);

    mc::json root;

    try {
        root = mc::json::parse(data);
    } catch (mc::json::parse_error& e) {
        std::cerr << e.what() << std::endl;
        return false;
    }

    for (auto& kv : root.items()) {
        std::string block_name = kv.key();
        mc::json states_node = kv.value().value("states", mc::json());

        if (!states_node.is_array()) continue;

        for (auto& state_node : states_node) {
            std::unique_ptr<terra::block::BlockState> state = terra::block::BlockState::FromJSON(state_node);

            m_BlockStates[state->GetId()] = std::move(state);
        }
    }

    return true;
}

bool AssetCache::LoadTextures(terra::assets::ZipArchive& archive) {
    m_TextureArray.Bind();

    for (std::string filename : archive.ListFiles("assets/minecraft/models/block/")) {
        size_t file_size;

        char* raw_data = archive.ReadFile(filename.c_str(), &file_size);
        if (raw_data) {
            std::string contents(raw_data, file_size);

            mc::json root;
            try {
                root = mc::json::parse(contents);
            } catch (mc::json::parse_error& e) {
                std::cerr << e.what() << std::endl;
                continue;
            }

            mc::json textures_node = root.value("textures", mc::json());

            if (textures_node.is_object()) {
                for (auto& kv : textures_node.items()) {
                    mc::json type_node = kv.value();

                    if (type_node.is_string()) {
                        std::string texture_path = "assets/minecraft/textures/" + type_node.get<std::string>() + ".png";

                        if (texture_path.find('#') == std::string::npos) {
                            int width, height, channels;
                            std::size_t texture_size;
                            char* texture_raw = archive.ReadFile(texture_path.c_str(), &texture_size);

                            unsigned char* image = stbi_load_from_memory(reinterpret_cast<unsigned char*>(texture_raw), texture_size, &width, &height, &channels, STBI_rgb_alpha);

                            if (image == nullptr) continue;

                            if (width == 16 && height == 16) {
                                std::size_t size = 16 * 16 * 4;
                                auto pos = texture_path.find_last_of('/');
                                auto filename = texture_path.substr(pos + 1);
                                auto block_name = filename.substr(0, filename.length() - 4);
                                std::string path = "block/" + block_name;

                                m_TextureArray.Append(path, std::string(reinterpret_cast<char*>(image), size));
                            }

                            stbi_image_free(image);
                        }
                    }
                }
            }
        }
    }

    m_TextureArray.Generate();
    return true;
}

std::size_t AssetCache::LoadBlockModels(terra::assets::ZipArchive& archive) {
    std::size_t count = 0;

    m_BlockModels.reserve(8600);

    for (std::string model_path : archive.ListFiles("assets/minecraft/models/block")) {
        size_t size;
        char* raw = archive.ReadFile(model_path.c_str(), &size);

        if (raw) {
            std::string contents(raw, size);

            mc::json root;
            try {
                root = mc::json::parse(contents);
            } catch (mc::json::parse_error& e) {
                std::cerr << e.what() << std::endl;
                continue;
            }

            std::unique_ptr<terra::block::BlockModel> model = terra::block::BlockModel::FromJSON(root);

            if (model != nullptr) {
                auto pos = model_path.find_last_of('/');
                auto filename = model_path.substr(pos + 1);
                auto block_name = filename.substr(0, filename.length() - 5);
                std::string path = "block/" + block_name;

                m_BlockModels[path] = std::move(model);
                ++count;
            }
        }
    }

    return count;
}

void AssetCache::LoadBlockVariants(terra::assets::ZipArchive& archive) {
    m_BlockVariants.reserve(1000);

    for (std::string block_state_path : archive.ListFiles("assets/minecraft/blockstates")) {
        size_t size;
        char* raw = archive.ReadFile(block_state_path.c_str(), &size);

        if (raw) {
            std::string contents(raw, size);

            mc::json root;
            try {
                root = mc::json::parse(contents);
            } catch (mc::json::parse_error& e) {
                std::cerr << e.what() << std::endl;
                continue;
            }

            mc::json variants_node = root.value("variants", mc::json());

            if (!variants_node.is_object()) continue;

            for (auto& kv : variants_node.items()) {
                std::string variant_name = kv.key();
                mc::json variant = kv.value();

                if (variant.is_array()) {
                    for (mc::json& variant_sub : variant) {
                        mc::json model_node = variant_sub.value("model", mc::json());
                        if (model_node.is_string()) {
                            std::string model_path = model_node.get<std::string>();

                            auto iter = m_BlockModels.find(model_path);
                            if (iter == m_BlockModels.end()) {
                                std::cerr << "Could not find block model " << model_path << std::endl;
                                continue;
                            }

                            auto pos = block_state_path.find_last_of('/');
                            auto filename = block_state_path.substr(pos + 1);
                            auto state = filename.substr(0, filename.length() - 5);

                            mc::block::BlockPtr block = mc::block::BlockRegistry::GetInstance()->GetBlock("minecraft:" + state);

                            if (block == nullptr) {
                                std::cerr << "Could not find block minecraft:" << state << std::endl;
                                continue;
                            }

                            m_BlockVariants[block->GetName()][variant_name] = iter->second.get();
                        }
                    }
                } else {
                    mc::json model_node = variant.value("model", mc::json());
                    if (model_node.is_string()) {
                        std::string model_path = model_node.get<std::string>();

                        auto iter = m_BlockModels.find(model_path);

                        if (iter == m_BlockModels.end()) {
                            std::cerr << "Could not find block model " << model_path << std::endl;
                            continue;
                        }

                        auto pos = block_state_path.find_last_of('/');
                        auto filename = block_state_path.substr(pos + 1);
                        auto state = filename.substr(0, filename.length() - 5);

                        mc::block::BlockPtr block = mc::block::BlockRegistry::GetInstance()->GetBlock("minecraft:" + state);

                        if (block == nullptr) {
                            std::cerr << "Could not find block minecraft:" << state << std::endl;
                            continue;
                        }

                        m_BlockVariants[block->GetName()][variant_name] = iter->second.get();
                    }
                }
            }
        }
    }
}

} // ns assets
} // ns terra
