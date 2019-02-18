#pragma once

#ifndef TERRACOTTA_BLOCK_BLOCKELEMENT_H_
#define TERRACOTTA_BLOCK_BLOCKELEMENT_H_

#include "BlockFace.h"
#include <vector>
#include <glm/glm.hpp>
#include "../assets/TextureArray.h"

namespace terra {
namespace block {

struct RenderableFace {
    assets::TextureHandle texture;
    int tint_index;
    BlockFace face;
    BlockFace cull_face;
};

class BlockElement {
public:
    BlockElement(glm::vec3 from, glm::vec3 to) : m_From(from), m_To(to) {
        m_Faces.resize(6);

        for (RenderableFace& renderable : m_Faces) {
            renderable.face = BlockFace::None;
        }
    }

    std::vector<RenderableFace>& GetFaces() { return m_Faces; }

    const RenderableFace& GetFace(BlockFace face) const { return m_Faces[static_cast<std::size_t>(face)]; }
    void AddFace(RenderableFace face) { m_Faces[static_cast<std::size_t>(face.face)] = face; }

    const glm::vec3& GetFrom() const { return m_From; }
    const glm::vec3& GetTo() const { return m_To; }

private:
    std::vector<RenderableFace> m_Faces;
    glm::vec3 m_From;
    glm::vec3 m_To;
};

} // ns block
} // ns terra

#endif

