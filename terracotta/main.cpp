#include <iostream>
#include <vector>
#include <iterator>
#include <memory>
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <fstream>
#include <unordered_map>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <mclib/core/Client.h>
#include <mclib/util/Utility.h>
#include "render/Shader.h"
#include "Camera.h"
#include <GLFW/glfw3.h>
#include "Game.h"
#include "math/TypeUtil.h"

#include "block/BlockFace.h"
#include "block/BlockModel.h"
#include "block/BlockState.h"
#include "render/ChunkMesh.h"
#include "render/ChunkMeshGenerator.h"
#include "assets/AssetCache.h"

#define STB_IMAGE_IMPLEMENTATION
#include "assets/stb_image.h"

const std::string server = "127.0.0.1";
const u16 port = 25565;
const std::string username = "terracotta";
const std::string password = "";

GLuint CreateBlockVAO();
GLFWwindow* InitializeWindow();

std::unique_ptr<terra::GameWindow> g_GameWindow;

std::unique_ptr<terra::assets::AssetCache> g_AssetCache;

int main(int argc, char* argvp[]) {
    mc::protocol::Version version = mc::protocol::Version::Minecraft_1_13_2;
    mc::block::BlockRegistry::GetInstance()->RegisterVanillaBlocks(version);
    GLFWwindow* window = InitializeWindow();

    if (window == nullptr) {
        return 1;
    }

    std::cout << "Checking layers" << std::endl;

    GLint max_layers;
    glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &max_layers);
    std::cout << "Layers: " << max_layers << std::endl;

    g_AssetCache = std::make_unique<terra::assets::AssetCache>();

    if (!g_AssetCache->Initialize("1.13.2.jar", "blocks.json")) {
        std::cerr << "Failed to initialize asset cache." << std::endl;
        return 1;
    }

    int width, height;
    glfwGetFramebufferSize(window, &width, &height);

    glViewport(0, 0, width, height);

    glClearColor(184.0f / 255, 210.0f / 255, 1.0f, 1.0f);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    terra::render::Shader shader;

    if (!shader.Initialize("shaders/block.vert", "shaders/block.frag")) {
        std::cerr << "Failed to initialize shader program.\n";
        return 1;
    }

    shader.Use();

    GLuint model_uniform = shader.GetUniform("model");
    GLuint view_uniform = shader.GetUniform("view");
    GLuint proj_uniform = shader.GetUniform("projection");
    GLuint sampler_uniform = shader.GetUniform("texarray");

    glUniform1i(sampler_uniform, 0);

    GLuint block_vao = CreateBlockVAO();

    float aspect_ratio = width / (float)height;
    float fov = glm::radians(80.0f);

    float view_distance = 264.0f;

    terra::Camera camera(glm::vec3(0.0f, 0.0f, 9.0f), fov, aspect_ratio, 0.1f, view_distance);
    terra::Game game(*g_GameWindow, camera);

    try {
        std::cout << "Logging in." << std::endl;
        if (!game.GetNetworkClient().Login(server, port, username, password, mc::core::UpdateMethod::Manual)) {
            std::wcout << "Failed to login." << std::endl;
            return 1;
        }
    } catch (std::exception& e) {
        std::wcout << e.what() << std::endl;
        return 1;
    }

    terra::render::ChunkMeshGenerator mesh_gen(game.GetNetworkClient().GetWorld());

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        game.Update();

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        auto&& camera = game.GetCamera();
        glm::mat4 viewMatrix = camera.GetViewMatrix();
        terra::math::volumes::Frustum frustum = camera.GetFrustum();

        shader.Use();

        glUniformMatrix4fv(view_uniform, 1, GL_FALSE, glm::value_ptr(viewMatrix));
        glUniformMatrix4fv(proj_uniform, 1, GL_FALSE, glm::value_ptr(camera.GetPerspectiveMatrix()));

        auto player_chunk = game.GetNetworkClient().GetWorld()->GetChunk(mc::ToVector3i(game.GetPosition()));
        if (player_chunk == nullptr) continue;
        
        for (auto&& kv : mesh_gen) {
            terra::render::ChunkMesh* mesh = kv.second.get();
            mc::Vector3i chunk_base = kv.first;

            mc::Vector3d min = mc::ToVector3d(chunk_base);
            mc::Vector3d max = mc::ToVector3d(chunk_base) + mc::Vector3d(16, 16, 16);
            mc::AABB chunk_bounds(min, max);

            if (!frustum.Intersects(chunk_bounds)) continue;

            mesh->Render(model_uniform);
        }

        glBindVertexArray(block_vao);

        auto entity_manager = game.GetNetworkClient().GetEntityManager();
        for (auto f = entity_manager->begin(); f != entity_manager->end(); ++f) {
            auto&& entity = f->second;
            if (entity_manager->GetPlayerEntity() == entity) continue;
            mc::Vector3d position = entity->GetPosition() + mc::Vector3d(0, 0.5, 0);

            glm::mat4 model(1.0);
            model = glm::translate(model, glm::vec3(position.x, position.y + 0.5, position.z));
            model = glm::rotate(model, entity->GetYaw(), glm::vec3(0, 1, 0));
            model = glm::scale(model, glm::vec3(1, 2, 1));

            glUniformMatrix4fv(model_uniform, 1, GL_FALSE, glm::value_ptr(model));

            glDrawArrays(GL_TRIANGLES, 0, 36);
        }

        glBindVertexArray(0);

        glUseProgram(0);
        glfwSwapBuffers(window);
    }

    return 0;
}

struct CubeVertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;
    u32 texture_index;
    glm::vec3 tint;

    CubeVertex(glm::vec3 pos, glm::vec3 normal, glm::vec2 uv, u32 tex_index, glm::vec3 tint) : position(pos), normal(normal), uv(uv), texture_index(tex_index), tint(tint) { }
};

GLuint CreateBlockVAO() {
    GLuint vao = 0;

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    GLfloat tex_index = 2;

    GLfloat vertices[] = {
        // Positions           // Normals           // Texture Coords // Texture Index // Tint
        -0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, tex_index, 0.0, 0.0, 0.0, // Front Bottom Left
        0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, tex_index, 0.0, 0.0, 0.0, // Front Bottom Right
        0.5f, 0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, tex_index, 0.0, 0.0, 0.0,// Front Top Right
        0.5f, 0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, tex_index, 0.0, 0.0, 0.0,// Front Top Right
        -0.5f, 0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, tex_index, 0.0, 0.0, 0.0,// Front Top Left
        -0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, tex_index, 0.0, 0.0, 0.0,// Front Bottom Left

        -0.5f, -0.5f, -0.5f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, tex_index, 0.0, 0.0, 0.0,// Back Bottom Left
        -0.5f, 0.5f, -0.5f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f, tex_index, 0.0, 0.0, 0.0,// Back Top Left
        0.5f, 0.5f, -0.5f, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f, tex_index, 0.0, 0.0, 0.0,// Back Top Right
        0.5f, 0.5f, -0.5f, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f, tex_index, 0.0, 0.0, 0.0,// Back Top Right
        0.5f, -0.5f, -0.5f, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, tex_index, 0.0, 0.0, 0.0,// Back Bottom Right
        -0.5f, -0.5f, -0.5f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, tex_index, 0.0, 0.0, 0.0, // Back Bottom Left

        -0.5f, 0.5f, 0.5f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f, tex_index, 0.0, 0.0, 0.0, // Front Top Left
        -0.5f, 0.5f, -0.5f, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f, tex_index, 0.0, 0.0, 0.0,// Back Top Left
        -0.5f, -0.5f, -0.5f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, tex_index, 0.0, 0.0, 0.0, // Back Bottom Left
        -0.5f, -0.5f, -0.5f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, tex_index, 0.0, 0.0, 0.0,// Back Bottom Left
        -0.5f, -0.5f, 0.5f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, tex_index, 0.0, 0.0, 0.0, // Front Bottom Left
        -0.5f, 0.5f, 0.5f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f, tex_index, 0.0, 0.0, 0.0, // Front Top Left

        0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, tex_index, 0.0, 0.0, 0.0,// Front Top Right
        0.5f, -0.5f, 0.5f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, tex_index, 0.0, 0.0, 0.0,// Front Bottom Right
        0.5f, -0.5f, -0.5f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, tex_index, 0.0, 0.0, 0.0,// Back Bottom Right
        0.5f, -0.5f, -0.5f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, tex_index, 0.0, 0.0, 0.0,// Back Bottom Right
        0.5f, 0.5f, -0.5f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, tex_index, 0.0, 0.0, 0.0,// Back Top Right
        0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, tex_index, 0.0, 0.0, 0.0,// Front Top Right

        -0.5f, -0.5f, -0.5f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f, tex_index, 0.0, 0.0, 0.0,// Back Bottom Left
        0.5f, -0.5f, -0.5f, 0.0f, -1.0f, 0.0f, 1.0f, 1.0f, tex_index, 0.0, 0.0, 0.0,// Back Bottom Right
        0.5f, -0.5f, 0.5f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, tex_index, 0.0, 0.0, 0.0,// Front Bottom Right
        0.5f, -0.5f, 0.5f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, tex_index, 0.0, 0.0, 0.0,// Front Bottom Right
        -0.5f, -0.5f, 0.5f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, tex_index, 0.0, 0.0, 0.0,// Front Bottom Left
        -0.5f, -0.5f, -0.5f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f, tex_index, 0.0, 0.0, 0.0,// Back Bottom Left

        -0.5f, 0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, tex_index, 0.0, 0.0, 0.0,// Back Top Left
        -0.5f, 0.5f, 0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, tex_index, 0.0, 0.0, 0.0,// Front Top Left
        0.5f, 0.5f, 0.5f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, tex_index, 0.0, 0.0, 0.0,// Front Top Right
        0.5f, 0.5f, 0.5f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, tex_index, 0.0, 0.0, 0.0,// Front Top Right
        0.5f, 0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, tex_index, 0.0, 0.0, 0.0,// Back Top Right
        -0.5f, 0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, tex_index, 0.0, 0.0, 0.0,// Back Top Left 
    };

    GLuint vbo;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // Position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(CubeVertex), (void*)offsetof(CubeVertex, position));
    glEnableVertexAttribArray(0);

    // Normal
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(CubeVertex), (void*)offsetof(CubeVertex, normal));
    glEnableVertexAttribArray(1);

    // TextureCoord
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(CubeVertex), (void*)offsetof(CubeVertex, uv));
    glEnableVertexAttribArray(2);

    // TextureCoord
    glVertexAttribIPointer(3, 1, GL_UNSIGNED_INT, sizeof(CubeVertex), (void*)offsetof(CubeVertex, texture_index));
    glEnableVertexAttribArray(3);

    // Tint
    glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(CubeVertex), (void*)offsetof(CubeVertex, tint));
    glEnableVertexAttribArray(4);
    
    return vao;
}

void APIENTRY OpenGLDebugOutputCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam) {
    std::cerr << "OpenGL error: " << message << std::endl;
}

GLFWwindow* InitializeWindow() {
    glfwInit();

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, false);

    GLFWwindow* window = glfwCreateWindow(960, 540, "Terracotta", nullptr, nullptr);
    if (window == nullptr) {
        std::cerr << "Failed to create glfw window.\n";
        return nullptr;
    }

    std::cout << "Creating GameWindow" << std::endl;

    g_GameWindow = std::make_unique<terra::GameWindow>(window);

    std::cout << "Setting context" << std::endl;
    glfwMakeContextCurrent(window);
    std::cout << "Setting user pointer" << std::endl;
    glfwSetWindowUserPointer(window, g_GameWindow.get());
    std::cout << "Setting input mode" << std::endl;
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    std::cout << "Setting key callback" << std::endl;
    glfwSetKeyCallback(window, [](GLFWwindow* window, int key, int code, int action, int mode) {
        terra::GameWindow* game_window = static_cast<terra::GameWindow*>(glfwGetWindowUserPointer(window));
        game_window->OnKeyChange(key, code, action, mode);
    });

    std::cout << "Setting cursor callback" << std::endl;
    glfwSetCursorPosCallback(window, [](GLFWwindow* window, double x, double y) {
        terra::GameWindow* game_window = static_cast<terra::GameWindow*>(glfwGetWindowUserPointer(window));
        game_window->OnMouseMove(x, y);
    });

    std::cout << "Setting scroll callback" << std::endl;
    glfwSetScrollCallback(window, [](GLFWwindow* window, double offset_x, double offset_y) {
        terra::GameWindow* game_window = static_cast<terra::GameWindow*>(glfwGetWindowUserPointer(window));
        game_window->OnMouseScroll(offset_x, offset_y);
    });

    std::cout << "Setting swap interval" << std::endl;
    glfwSwapInterval(0);

    std::cout << "Inititializing glew" << std::endl;
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize glew\n";
        return nullptr;
    }

#ifdef _DEBUG
    std::cout << "Setting debug callback" << std::endl;
    glDebugMessageCallback(OpenGLDebugOutputCallback, nullptr);
    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
#endif
    GLenum error;

    std::cout << "Checking errors" << std::endl;
    while ((error = glGetError()) != GL_NO_ERROR) {
        
    }

    return window;
}
