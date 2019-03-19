#include "Game.h"

#include <GLFW/glfw3.h>
#include <mclib/util/Utility.h>
#include <mclib/util/Utility.h>
#include <mclib/world/World.h>
#include <mclib/common/AABB.h>
#include <mclib/inventory/Inventory.h>
#include <mclib/protocol/packets/Packet.h>
#include "math/Plane.h"
#include "math/TypeUtil.h"
#include <iostream>
#include <limits>

namespace terra {

Game::Game(GameWindow& window, const Camera& camera) 
    : m_Dispatcher(),
      m_NetworkClient(&m_Dispatcher, mc::protocol::Version::Minecraft_1_13_2), 
      m_Window(window),
      m_Camera(camera),
      m_Sprinting(false)
{
    window.RegisterMouseChange(std::bind(&Game::OnMouseChange, this, std::placeholders::_1, std::placeholders::_2));
    window.RegisterMouseScroll(std::bind(&Game::OnMouseScroll, this, std::placeholders::_1, std::placeholders::_2));
    window.RegisterMouseButton(std::bind(&Game::OnMousePress, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

    m_NetworkClient.GetPlayerController()->SetHandleFall(true);
    m_NetworkClient.GetConnection()->GetSettings()
        .SetMainHand(mc::MainHand::Right)
        .SetViewDistance(4);
}

mc::Vector3d Game::GetPosition() {
    return m_NetworkClient.GetPlayerController()->GetPosition();
}

void Game::Update() {
    float movement_speed = 4.3f;

    float current_frame = (float)glfwGetTime();

    m_DeltaTime = current_frame - m_LastFrame;
    m_LastFrame = current_frame;

    mc::Vector3d front(
        std::cos(m_Camera.GetYaw()) * std::cos(0),
        std::sin(0),
        std::sin(m_Camera.GetYaw()) * std::cos(0)
    );

    mc::Vector3d direction;

    if (m_Window.IsKeyDown(GLFW_KEY_W)) {
        direction += front;

        if (m_Window.IsKeyDown(GLFW_KEY_LEFT_CONTROL)) {
            m_Sprinting = true;
        }
    }

    if (m_Window.IsKeyDown(GLFW_KEY_S)) {
        direction -= front;
        m_Sprinting = false;
    }

    if (m_Window.IsKeyDown(GLFW_KEY_A)) {
        mc::Vector3d right = mc::Vector3Normalize(front.Cross(mc::Vector3d(0, 1, 0)));

        direction -= right;
    }

    if (m_Window.IsKeyDown(GLFW_KEY_D)) {
        mc::Vector3d right = mc::Vector3Normalize(front.Cross(mc::Vector3d(0, 1, 0)));

        direction += right;
    }

    if (m_Sprinting) {
        if (direction.LengthSq() <= 0.0) {
            m_Sprinting = false;
        } else {
            movement_speed *= 1.3f;
        }
        m_Camera.SetFov(glm::radians(90.0f));
    } else {
        m_Camera.SetFov(glm::radians(80.0f));
    }

    auto controller = m_NetworkClient.GetPlayerController();

    mc::Vector3d target_pos = controller->GetPosition() + mc::Vector3Normalize(direction) * movement_speed * m_DeltaTime;

    // TODO: Stop using player controller eventually and move to physics system.
    controller->SetMoveSpeed(movement_speed);
    controller->SetYaw(m_Camera.GetYaw() - glm::radians(90.0f));
    controller->SetPitch(-m_Camera.GetPitch());
    controller->SetTargetPosition(target_pos);
    // Manually call move so it's updated outside of the PlayerController 20 ticks per second.
    controller->Move(target_pos - controller->GetPosition());


    m_NetworkClient.Update();

    glm::vec3 eye = math::VecToGLM(controller->GetPosition()) + glm::vec3(0, 1.6, 0);

    m_Camera.SetPosition(eye);
}

void Game::OnMouseChange(double offset_x, double offset_y) {
    m_Camera.ProcessRotation((float)offset_x, (float)offset_y);
}

void Game::OnMouseScroll(double offset_x, double offset_y) {
    m_Camera.ProcessZoom((float)offset_y);
}

// TODO: Temporary fun code
template <typename T>
inline T Sign(T val) {
    return std::signbit(val) ? static_cast<T>(-1) : static_cast<T>(1);
}

// TODO: Temporary fun code
inline mc::Vector3d BasisAxis(int basisIndex) {
    static const mc::Vector3d axes[3] = { mc::Vector3d(1, 0, 0), mc::Vector3d(0, 1, 0), mc::Vector3d(0, 0, 1) };
    return axes[basisIndex];
}

// TODO: Temporary fun code
std::pair<mc::Vector3d, mc::Face> GetClosestNormal(const mc::Vector3d& pos, mc::AABB bounds) {
    mc::Vector3d center = bounds.min + (bounds.max - bounds.min) / 2;
    mc::Vector3d dim = bounds.max - bounds.min;
    mc::Vector3d offset = pos - center;

    double minDist = std::numeric_limits<double>::max();
    mc::Vector3d normal;

    for (int i = 0; i < 3; ++i) {
        double dist = dim[i] - std::abs(offset[i]);
        if (dist < minDist) {
            minDist = dist;
            normal = BasisAxis(i) * Sign(offset[i]);
        }
    }

    mc::Face face = mc::Face::North;

    if (normal.x == 1) {
        face = mc::Face::East;
    } else if (normal.x == -1) {
        face = mc::Face::West;
    } else if (normal.y == 1) {
        face = mc::Face::Top;
    } else if (normal.y == -1) {
        face = mc::Face::Bottom;
    } else if (normal.z == 1) {
        face = mc::Face::South;
    }

    return std::make_pair<>(normal, face);
}

// TODO: Temporary fun code
bool RayCast(mc::world::World& world, mc::Vector3d from, mc::Vector3d direction, double range, mc::Vector3d& hit, mc::Vector3d& normal, mc::Face& face) {
    static const std::vector<mc::Vector3d> directions = {
        mc::Vector3d(0, 0, 0), 
        mc::Vector3d(1, 0, 0), mc::Vector3d(-1, 0, 0), 
        mc::Vector3d(0, 1, 0), mc::Vector3d(0, -1, 0), 
        mc::Vector3d(0, 0, 1), mc::Vector3d(0, 0, -1),
        mc::Vector3d(0, 1, 1), mc::Vector3d(0, 1, -1),
        mc::Vector3d(1, 1, 0), mc::Vector3d(1, 1, 1), mc::Vector3d(1, 1, -1),
        mc::Vector3d(-1, 1, 0), mc::Vector3d(-1, 1, 1), mc::Vector3d(-1, 1, -1),

        mc::Vector3d(0, -1, 1), mc::Vector3d(0, -1, -1),
        mc::Vector3d(1, -1, 0), mc::Vector3d(1, -1, 1), mc::Vector3d(1, -1, -1),
        mc::Vector3d(-1, -1, 0), mc::Vector3d(-1, -1, 1), mc::Vector3d(-1, -1, -1)
    };

    mc::Ray ray(from, direction);

    double closest_distance = std::numeric_limits<double>::max();
    mc::Vector3d closest_pos;
    mc::AABB closest_aabb;
    bool collided = false;

    for (double i = 0; i < range + 1; ++i) {
        mc::Vector3d position = from + direction * i;

        for (mc::Vector3d checkDirection : directions) {
            mc::Vector3d checkPos = position + checkDirection;
            mc::block::BlockPtr block = world.GetBlock(checkPos);

            if (block->IsOpaque()) {
                mc::AABB bounds = block->GetBoundingBox(checkPos);
                double distance;

                if (bounds.Intersects(ray, &distance)) {
                    if (distance < 0 || distance > range) continue;

                    if (distance < closest_distance) {
                        closest_distance = distance;
                        closest_pos = from + direction * closest_distance;
                        closest_aabb = bounds;
                        collided = true;
                    }
                }
            }
        }
    }

    if (collided) {
        hit = closest_pos;
        std::tie(normal, face) = GetClosestNormal(hit, closest_aabb);
        return true;
    }

    return false;
}

// TODO: Temporary fun code
void Game::OnMousePress(int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_1 && action == GLFW_PRESS) {
        auto& world = *m_NetworkClient.GetWorld();
        mc::Vector3d position(m_Camera.GetPosition().x, m_Camera.GetPosition().y, m_Camera.GetPosition().z);
        mc::Vector3d forward(m_Camera.GetFront().x, m_Camera.GetFront().y, m_Camera.GetFront().z);
        mc::Vector3d hit;
        mc::Vector3d normal;
        mc::Face face;

        if (RayCast(world, position, forward, 5.0, hit, normal, face)) {
            using namespace mc::protocol::packets::out;

            {
                PlayerDiggingPacket::Status status = PlayerDiggingPacket::Status::StartedDigging;
                PlayerDiggingPacket packet(status, mc::ToVector3i(hit + forward * 0.1), mc::Face::West);

                m_NetworkClient.GetConnection()->SendPacket(&packet);
            }

            {
                PlayerDiggingPacket::Status status = PlayerDiggingPacket::Status::FinishedDigging;
                PlayerDiggingPacket packet(status, mc::ToVector3i(hit + forward * 0.1), mc::Face::West);

                m_NetworkClient.GetConnection()->SendPacket(&packet);
            }
        }
    } else if (button == GLFW_MOUSE_BUTTON_2 && action == GLFW_PRESS) {
        auto& world = *m_NetworkClient.GetWorld();
        mc::Vector3d position(m_Camera.GetPosition().x, m_Camera.GetPosition().y, m_Camera.GetPosition().z);
        mc::Vector3d forward(m_Camera.GetFront().x, m_Camera.GetFront().y, m_Camera.GetFront().z);
        mc::Vector3d hit;
        mc::Vector3d normal;
        mc::Face face;

        if (RayCast(world, position, forward, 5.0, hit, normal, face)) {
            mc::inventory::Inventory& inventory = *m_NetworkClient.GetInventoryManager()->GetPlayerInventory();

            auto& hotbar = m_NetworkClient.GetHotbar();
            s32 slot_id = hotbar.GetSelectedSlot() + mc::inventory::Inventory::HOTBAR_SLOT_START;

            mc::inventory::Slot slot = inventory.GetItem(slot_id);

            // Item ids are separate from block ids.
            const u32 draw_block = 9;

            if (slot.GetItemId() != draw_block) {
                mc::protocol::packets::out::CreativeInventoryActionPacket packet(slot_id, mc::inventory::Slot(draw_block, 1, 0));
                m_NetworkClient.GetConnection()->SendPacket(&packet);
            }

            mc::Vector3i target = mc::ToVector3i(hit + forward * 0.1);

            mc::protocol::packets::out::PlayerBlockPlacementPacket packet(target, face, mc::Hand::Main, mc::Vector3f(1.0f, 0.0f, 0.0f));
            m_NetworkClient.GetConnection()->SendPacket(&packet);
        }
    }
}

} // ns terra
