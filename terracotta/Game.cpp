#include "Game.h"

#include <GLFW/glfw3.h>
#include <mclib/util/Utility.h>
#include <mclib/util/Utility.h>

namespace terra {

Game::Game(GameWindow& window, const Camera& camera) 
    : m_Dispatcher(),
      m_NetworkClient(&m_Dispatcher, mc::protocol::Version::Minecraft_1_13_2), 
      m_Window(window),
      m_Camera(camera)
{
    window.RegisterMouseChange(std::bind(&Game::OnMouseChange, this, std::placeholders::_1, std::placeholders::_2));
    window.RegisterMouseScroll(std::bind(&Game::OnMouseScroll, this, std::placeholders::_1, std::placeholders::_2));

    m_NetworkClient.GetPlayerController()->SetHandleFall(true);
    m_NetworkClient.GetConnection()->GetSettings()
        .SetMainHand(mc::MainHand::Right)
        .SetViewDistance(4);
}

mc::Vector3d Game::GetPosition() {
    return m_NetworkClient.GetPlayerController()->GetPosition();
}


void Game::Update() {
    float current_frame = (float)glfwGetTime();
    m_DeltaTime = current_frame - m_LastFrame;
    m_LastFrame = current_frame;
    
#ifdef WIN32
    std::string s = "Frame time: " + std::to_string(m_DeltaTime * 1000.0) + "ms\n";
    OutputDebugStringA(s.c_str());
#endif

    auto controller = m_NetworkClient.GetPlayerController();
    double speed = 4.3;

    auto pos = controller->GetPosition();

    mc::Vector3d target_pos = pos;

    if (m_Window.IsKeyDown(GLFW_KEY_W)) {
        m_Camera.ProcessMovement(terra::CameraMovement::Forward, m_DeltaTime);
        glm::vec3 front = m_Camera.GetFront();

        front.y = 0;
        front = glm::normalize(front);

        target_pos += mc::Vector3Normalize(mc::Vector3d(front.x, front.y, front.z)) * speed;
    }

    if (m_Window.IsKeyDown(GLFW_KEY_S)) {
        m_Camera.ProcessMovement(terra::CameraMovement::Backward, m_DeltaTime);
        glm::vec3 front = m_Camera.GetFront();

        front.y = 0;
        front = glm::normalize(front);

        target_pos -= mc::Vector3Normalize(mc::Vector3d(front.x, front.y, front.z)) * speed;
    }

    if (m_Window.IsKeyDown(GLFW_KEY_A)) {
        m_Camera.ProcessMovement(terra::CameraMovement::Left, m_DeltaTime);
        glm::vec3 right = m_Camera.GetRight();

        target_pos -= mc::Vector3Normalize(mc::Vector3d(right.x, right.y, right.z)) * speed;
    }

    if (m_Window.IsKeyDown(GLFW_KEY_D)) {
        m_Camera.ProcessMovement(terra::CameraMovement::Right, m_DeltaTime);
        glm::vec3 right = m_Camera.GetRight();

        target_pos += mc::Vector3Normalize(mc::Vector3d(right.x, right.y, right.z)) * speed;
    }

    controller->SetMoveSpeed(4.3f);
    controller->SetTargetPosition(target_pos);

    float yaw = m_Camera.GetYaw();
    float pitch = -m_Camera.GetPitch();

    controller->SetYaw(yaw - glm::radians(90.0f));
    controller->SetPitch(pitch);

    mc::Vector3d position = controller->GetPosition();
    glm::vec3 eye(position.x, position.y + 1.6, position.z);

    m_Camera.SetPosition(eye);

    m_NetworkClient.Update();
}

void Game::OnMouseChange(double offset_x, double offset_y) {
    m_Camera.ProcessRotation((float)offset_x, (float)offset_y);
}

void Game::OnMouseScroll(double offset_x, double offset_y) {
    m_Camera.ProcessZoom((float)offset_y);
}

} // ns terra
