#ifndef TERRACOTTA_RENDER_MODEL_H_
#define TERRACOTTA_RENDER_MODEL_H_

#include "Shader.h"
#include <mclib/common/Vector.h>
#include <mclib/common/Json.h>

namespace terra {
namespace render {

class Model {
public:
    Model(Shader& shader, GLuint modelUniform);

    void Render(mc::Vector3d position, float yaw = 0.0f);

    static Model FromJSON(const mc::json& root);
private:
    GLuint m_ModelUniform;
    Shader& m_Shader;
};

} // ns render
} // ns terra

#endif
