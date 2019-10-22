#pragma once

#ifndef TERRACOTTA_TRANSFORM_H_
#define TERRACOTTA_TRANSFORM_H_

#include <mclib/common/Vector.h>
#include <mclib/common/AABB.h>

namespace terra {

struct Transform {
    mc::Vector3d position;
    mc::Vector3d velocity;
    mc::Vector3d acceleration;
    float max_speed;
    float orientation;
    float rotation;

    mc::AABB bounding_box;
};

} // namespace terra

#endif
