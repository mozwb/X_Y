#pragma once

#include <string>
#include <vector>

namespace X_Y::Model {

/// @brief 三角面顶点索引（1-based OBJ → 0-based 存储）
struct Index {
    int vertex   = -1;  // v
    int texcoord = -1;  // vt
    int normal   = -1;  // vn
};

/// @brief 单个 Mesh（一个 obj 文件通常只有一个 mesh，如有多个 g/o 会分拆）
struct Mesh {
    std::string name;
    std::vector<Index> indices;  // 平铺的三角面索引，每 3 个 Index 为一个三角
};

/// @brief 平铺属性数组（参考 tinyobjloader 设计）
struct Model {
    std::vector<float> vertices;   // 每 3 个一组 (x, y, z)
    std::vector<float> normals;    // 每 3 个一组 (nx, ny, nz)
    std::vector<float> texcoords;  // 每 2 个一组 (u, v)
    std::vector<Mesh> meshes;

    void Clear() {
        vertices.clear();
        normals.clear();
        texcoords.clear();
        meshes.clear();
    }
};

/// @brief 从文件加载 .obj
/// @param filepath OBJ 文件路径
/// @param out_model 输出模型数据
/// @param err 错误信息
/// @return true 成功，false 失败
bool LoadObj(const std::string& filepath, Model& out_model, std::string& err);

/// @brief 从内存数据加载 .obj
/// @param data 内存中的数据指针
/// @param size 数据大小（字节）
/// @param out_model 输出模型数据
/// @param err 错误信息
/// @return true 成功，false 失败

bool LoadObjFromMemory(const char* data, size_t size, Model& out_model, std::string& err);

}// namespace X_Y::Model
