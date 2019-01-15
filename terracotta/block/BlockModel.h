#ifndef TERRACOTTA_BLOCK_BLOCKMODEL_H_
#define TERRACOTTA_BLOCK_BLOCKMODEL_H_

#include <string>
#include <memory>

#include <mclib/common/JsonFwd.h>
#include "BlockFace.h"

namespace terra {
namespace block {

class BlockModel {
public:
    static std::unique_ptr<BlockModel> FromJSON(const mc::json& model_json);

    const std::string& GetAllTexturePath() const;
    const std::string& GetEndTexturePath() const;
    const std::string& GetTexturePath(BlockFace face) const;

private:
    const std::string& GetWestTexturePath() const;
    const std::string& GetEastTexturePath() const;
    const std::string& GetNorthTexturePath() const;
    const std::string& GetSouthTexturePath() const;
    const std::string& GetSideTexturePath() const;
    const std::string& GetTopTexturePath() const;
    const std::string& GetBottomTexturePath() const;

    std::string m_AllPath;
    std::string m_EndPath;
    std::string m_SidePath;
    std::string m_NorthPath;
    std::string m_WestPath;
    std::string m_SouthPath;
    std::string m_EastPath;
    std::string m_TopPath;
    std::string m_BottomPath;
};

} // ns block
} // ns terra

#endif
