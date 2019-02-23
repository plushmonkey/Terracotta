#pragma once

#ifndef TERRACOTTA_BLOCK_BLOCKVARIANT_H_
#define TERRACOTTA_BLOCK_BLOCKVARIANT_H_

#include "BlockModel.h"

#include <mclib/block/Block.h>
#include <glm/glm.hpp>

namespace terra {
namespace block {

class BlockVariant {
public:
    BlockVariant(BlockModel* model, const std::string& properties, mc::block::BlockPtr block) : m_Model(model), m_Properties(properties), m_Block(block), m_Rotations(0, 0, 0), m_LockUV(true) { }

    BlockModel* GetModel() { return m_Model; }
    mc::block::BlockPtr GetBlock() { return m_Block; }
    const std::string& GetProperties() const { return m_Properties; }

    const glm::vec3& GetRotations() const { return m_Rotations; }
    void SetRotation(const glm::vec3& rotations) { m_Rotations = rotations; }
    bool HasRotation() {
        return m_Rotations.x != 0 || m_Rotations.y != 0 || m_Rotations.z != 0;
    }

private:
    std::string m_Properties;
    BlockModel* m_Model;
    mc::block::BlockPtr m_Block;
    glm::vec3 m_Rotations;
    bool m_LockUV;
};

} // ns block
} // ns terra

#endif
