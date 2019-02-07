#include "TextureArray.h"

#include <GL/glew.h>

namespace terra {
namespace assets {

TextureArray::TextureArray() {
    glActiveTexture(GL_TEXTURE0);
    glGenTextures(1, &m_TextureId);
    glBindTexture(GL_TEXTURE_2D_ARRAY, m_TextureId);
}

std::size_t TextureArray::Append(const std::string& filename, const std::string& texture) {
    auto iter = m_Filenames.find(filename);

    if (iter != m_Filenames.end()) {
        return iter->second;
    }

    unsigned int index = m_Filenames.size();
    m_Filenames[filename] = index;
    m_TextureData.insert(m_TextureData.end(), texture.c_str(), texture.c_str() + texture.size());

    return m_Filenames.size();
}

void TextureArray::Generate() {
    glBindTexture(GL_TEXTURE_2D_ARRAY, m_TextureId);

    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 3);

    glTexStorage3D(GL_TEXTURE_2D_ARRAY, 4, GL_RGBA8, 16, 16, m_Filenames.size());
    glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, 16, 16, m_Filenames.size(), GL_RGBA, GL_UNSIGNED_BYTE, &m_TextureData[0]);

    glGenerateMipmap(GL_TEXTURE_2D_ARRAY);

    m_TextureData.clear();
    std::vector<unsigned char>().swap(m_TextureData);
}

void TextureArray::Bind() {
    glBindTexture(GL_TEXTURE_2D_ARRAY, m_TextureId);
}

unsigned int TextureArray::GetIndex(const std::string& filename) {
    return m_Filenames[filename];
}

} // ns assets
} // ns terra
