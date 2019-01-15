#ifndef TERRACOTTA_GAME_H_
#define TERRACOTTA_GAME_H_

#include <mclib/core/Client.h>

#include "Camera.h"
#include "GameWindow.h"

#include <memory>

namespace terra {

class Game {
public:
    Game(GameWindow& window, const Camera& camera);

    void OnMouseChange(double x, double y);
    void OnMouseScroll(double offset_x, double offset_y);

    void Update();

    Camera& GetCamera() { return m_Camera; }
    mc::Vector3d GetPosition();
    mc::core::Client& GetNetworkClient() { return m_NetworkClient; }

private:
    mc::protocol::packets::PacketDispatcher m_Dispatcher;
    mc::core::Client m_NetworkClient;
    GameWindow& m_Window;
    Camera m_Camera;
    float m_DeltaTime;
    float m_LastFrame;
};

} // ns terra

#endif
