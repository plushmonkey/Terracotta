#ifndef TERRACOTTA_ASSETS_TEXTUREARRAY_H_
#define TERRACOTTA_ASSETS_TEXTUREARRAY_H_

#include <cstddef>
#include <vector>
#include <string>
#include <unordered_map>

namespace terra {
namespace assets {

class TextureArray {
public:
    TextureArray();

    std::size_t Append(const std::string& filename, const std::string& texture);

    unsigned int GetIndex(const std::string& filename);

    void Generate();
    void Bind();
private:
    unsigned int m_TextureId;

    std::vector<unsigned char> m_TextureData;
    std::unordered_map<std::string, unsigned int> m_Filenames;
};

} // ns assets
} // ns terra

#endif
