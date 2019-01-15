#include "Shader.h"

#include <string>
#include <iostream>
#include <fstream>
#include <sstream>

bool CreateShader(GLenum type, const char* source, GLuint* shaderOut) {
    GLuint shader = glCreateShader(type);

    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint success;
    GLchar infoLog[512];
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(shader, sizeof(infoLog), nullptr, infoLog);
        std::cout << "Shader error: " << std::endl << infoLog << std::endl;
        return false;
    }

    *shaderOut = shader;
    return true;
}

bool CreateProgram(GLuint vertexShader, GLuint fragmentShader, GLuint* programOut) {
    GLuint program = glCreateProgram();

    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    GLint success;
    GLchar infoLog[512];
    glGetProgramiv(program, GL_LINK_STATUS, &success);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    if (!success) {
        glGetProgramInfoLog(program, sizeof(infoLog), nullptr, infoLog);
        std::cout << "Link error: " << std::endl << infoLog << std::endl;
        return false;
    }

    *programOut = program;
    return true;
}

namespace terra {
namespace render {

Shader::Shader() {

}

bool Shader::Initialize(const char* vPath, const char* fPath) {
    std::string vSource, fSource;
    std::ifstream vFile, fFile;

    vFile.exceptions(std::ifstream::badbit);
    fFile.exceptions(std::ifstream::badbit);
    try {
        vFile.open(vPath);
        fFile.open(fPath);

        std::stringstream vStream, fStream;
        vStream << vFile.rdbuf();
        fStream << fFile.rdbuf();

        vFile.close();
        fFile.close();

        vSource = vStream.str();
        fSource = fStream.str();
    } catch (std::ifstream::failure& e) {
        std::cerr << "Read error: " << e.what() << std::endl;
        return false;
    }

    GLuint vShader, fShader;
    if (!CreateShader(GL_VERTEX_SHADER, vSource.c_str(), &vShader)) {
        return false;
    }

    if (!CreateShader(GL_FRAGMENT_SHADER, fSource.c_str(), &fShader)) {
        glDeleteShader(vShader);
        return false;
    }

    return CreateProgram(vShader, fShader, &m_Program);
}

GLuint Shader::GetUniform(const char* name) {
    return glGetUniformLocation(m_Program, name);
}

void Shader::Use() {
    glUseProgram(m_Program);
}

void Shader::Stop() {
    glUseProgram(0);
}

} // ns render
} // ns terra
