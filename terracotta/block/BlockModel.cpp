#include "BlockModel.h"

#include <mclib/common/Json.h>

namespace terra {
namespace block {

std::unique_ptr<BlockModel> BlockModel::FromJSON(const mc::json& model_json) {
    std::unique_ptr<BlockModel> model = std::make_unique<BlockModel>();

    mc::json textures_node = model_json.value("textures", mc::json());

    if (!textures_node.is_object()) return nullptr;

    model->m_AllPath = textures_node.value("all", "");
    model->m_SidePath = textures_node.value("side", "");
    model->m_NorthPath = textures_node.value("front", "");
    model->m_TopPath = textures_node.value("top", "");
    model->m_BottomPath = textures_node.value("bottom", "");
    model->m_WestPath = textures_node.value("west", "");
    model->m_SouthPath = textures_node.value("south", "");
    model->m_EastPath = textures_node.value("east", "");
    model->m_EndPath = textures_node.value("end", "");

    if (model->m_NorthPath.empty()) {
        model->m_NorthPath = textures_node.value("north", "");
    }

    if (model->m_TopPath.empty()) {
        model->m_TopPath = textures_node.value("up", "");
    }

    if (model->m_BottomPath.empty()) {
        model->m_BottomPath = textures_node.value("down", "");
    }

    return model;
}

const std::string& BlockModel::GetTexturePath(BlockFace face) const {
    switch (face) {
        case BlockFace::Bottom:
            return GetBottomTexturePath();
        case BlockFace::Top:
            return GetTopTexturePath();
        case BlockFace::North:
            return GetNorthTexturePath();
        case BlockFace::South:
            return GetSouthTexturePath();
        case BlockFace::East:
            return GetEastTexturePath();
        case BlockFace::West:
            return GetWestTexturePath();
        default:
            break;
    }

    return GetAllTexturePath();
}

const std::string& BlockModel::GetAllTexturePath() const { return m_AllPath; }

const std::string& BlockModel::GetEndTexturePath() const {
    if (m_EndPath.empty()) return GetAllTexturePath();

    return m_EndPath;
}

const std::string& BlockModel::GetWestTexturePath() const {
    if (m_WestPath.empty()) return GetSideTexturePath();

    return m_WestPath;
}

const std::string& BlockModel::GetEastTexturePath() const {
    if (m_EastPath.empty()) return GetSideTexturePath();

    return m_EastPath;
}

const std::string& BlockModel::GetNorthTexturePath() const {
    if (m_NorthPath.empty()) return GetSideTexturePath();

    return m_NorthPath;
}

const std::string& BlockModel::GetSouthTexturePath() const {
    if (m_SouthPath.empty()) return GetSideTexturePath();

    return m_SouthPath;
}

const std::string& BlockModel::GetSideTexturePath() const {
    if (m_SidePath.empty()) return m_AllPath;

    return m_SidePath;
}

const std::string& BlockModel::GetTopTexturePath() const {
    if (m_TopPath.empty()) return m_AllPath;

    return m_TopPath;
}

const std::string& BlockModel::GetBottomTexturePath() const {
    if (m_BottomPath.empty()) return m_AllPath;
    return m_BottomPath;
}

} // ns block
} // ns terra
