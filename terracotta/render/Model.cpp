#include "Model.h"
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace terra {
namespace render {

Model::Model(Shader& shader, GLuint modelUniform) 
    : m_Shader(shader), m_ModelUniform(modelUniform)
{

}

void Model::Render(mc::Vector3d position, float yaw) {
    glm::mat4 modelMatrix(1.0);
    modelMatrix = glm::translate(modelMatrix, glm::vec3(position.x, position.y, position.z));
    modelMatrix = glm::rotate(modelMatrix, yaw, glm::vec3(0, 1, 0));

    glUniformMatrix4fv(m_ModelUniform, 1, GL_FALSE, glm::value_ptr(modelMatrix));

    glDrawArrays(GL_TRIANGLES, 0, 36);
}

} // ns render
} // ns terra
