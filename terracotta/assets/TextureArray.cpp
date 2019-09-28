#include "TextureArray.h"

#include <GL/glew.h>

namespace terra {
namespace assets {

TextureArray::TextureArray() {
    glActiveTexture(GL_TEXTURE0);
    glGenTextures(1, &m_TextureId);
    glBindTexture(GL_TEXTURE_2D_ARRAY, m_TextureId);

    memset(m_Transparency, 0, sizeof(m_Transparency));
}

TextureHandle TextureArray::Append(const std::string& filename, const std::string& texture) {
    auto iter = m_Textures.find(filename);

    if (iter != m_Textures.end()) {
        return iter->second;
    }

    TextureHandle handle = static_cast<TextureHandle>(m_Textures.size());
    
    m_Textures[filename] = handle;
    m_TextureData.insert(m_TextureData.end(), texture.c_str(), texture.c_str() + texture.size());

    for (std::size_t i = 4; i < texture.size(); i += 4) {
        char alpha = texture[i];

        if (texture[i] == 0) {
            m_Transparency[handle] = true;
            break;
        }
    }

    return handle;
}

void TextureArray::Generate() {
    glBindTexture(GL_TEXTURE_2D_ARRAY, m_TextureId);

    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_LEVEL, 4);

    GLsizei size = static_cast<GLuint>(m_Textures.size());

    glTexStorage3D(GL_TEXTURE_2D_ARRAY, 5, GL_RGBA8, 16, 16, size);
    glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, 16, 16, size, GL_RGBA, GL_UNSIGNED_BYTE, &m_TextureData[0]);

    glGenerateMipmap(GL_TEXTURE_2D_ARRAY);

    m_TextureData.clear();
    std::vector<unsigned char>().swap(m_TextureData);
}

void TextureArray::Bind() {
    glBindTexture(GL_TEXTURE_2D_ARRAY, m_TextureId);
}

bool TextureArray::GetTexture(const std::string& filename, TextureHandle* handle) {
    auto iter = m_Textures.find(filename);

    if (iter == m_Textures.end()) {
        return false;
    }

    *handle = iter->second;
    return true;
}

bool TextureArray::IsTransparent(TextureHandle handle) const {
    return m_Transparency[handle];
}

} // ns assets
} // ns terra
