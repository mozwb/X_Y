#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include "ModelLoader.h"

namespace X_Y::Model {

/// @brief 顶点属性布局枚举（以 float 为单位）
///        按声明顺序依次 packed 到 Vertices 数组
enum class VertexAttrib : uint8_t {
    Position = 0,   ///< pos.xyz, 3 floats
    Normal   = 1,   ///< normal.xyz, 3 floats
    Texcoord = 2,   ///< texcoord.uv, 2 floats (预留)
    Count    = 3    ///< 当前布局总属性数
};

/// @brief 每个属性分量的 float 数量
inline constexpr uint32_t kAttribSize[] = {
    3,  // Position
    3,  // Normal
    2   // Texcoord
};

/// @brief 当前布局：Position(3) + Normal(3) = 6 floats/vertex
inline constexpr uint32_t kVertexStride =
    kAttribSize[static_cast<uint32_t>(VertexAttrib::Position)] +
    kAttribSize[static_cast<uint32_t>(VertexAttrib::Normal)];

// Texcoord 预留但暂不写入，以后想启用时只需：
//   a) 将 kVertexStride 的定义改为包含 Texcoord (6→8)
//   b) PrepareMesh 中追加 texcoord 写入逻辑
// 不需要改任何硬编码数字。

/// @brief GPU-ready 数据块：平铺 float 数组 + uint32_t 索引
struct GPUMeshData {
    std::vector<float> Vertices;      ///< interleaved 顶点数据
    std::vector<uint32_t> Indices;    ///< triangle list 索引
};

/// @brief 从 Model 的某个 Mesh 生成 GPU 可用的顶点/索引数据
///
/// @param model        源模型数据（平铺属性数组）
/// @param mesh         要转换的 Mesh（triangle list indices）
/// @param out_data     输出：GPU-ready 数据块
/// @param err          错误信息
/// @return true 成功，false 失败
// 在头文件中提供内联实现，避免链接时找不到符号的问题。
inline bool PrepareMesh(const Model& model,
                        const Mesh& mesh,
                        GPUMeshData& out_data,
                        std::string& err)
{
    out_data.Vertices.clear();
    out_data.Indices.clear();

    if (mesh.indices.empty()) {
        err = "Mesh has no indices";
        return false;
    }

    const size_t vert_count = model.vertices.size() / 3;
    const size_t norm_count = model.normals.size()  / 3;

    if (vert_count == 0) {
        err = "Model has no vertex data";
        return false;
    }

    // 顶点去重表（使用三元组 key）
    struct VertexKey {
        int v;
        int vt;
        int vn;

        bool operator==(const VertexKey& o) const {
            return v == o.v && vt == o.vt && vn == o.vn;
        }
    };

    struct VertexKeyHash {
        size_t operator()(const VertexKey& k) const noexcept {
            size_t h1 = static_cast<size_t>(static_cast<int>(k.v) + 1);
            size_t h2 = static_cast<size_t>(static_cast<int>(k.vt) + 1);
            size_t h3 = static_cast<size_t>(static_cast<int>(k.vn) + 1);
            return h1 ^ (h2 << 7) ^ (h3 << 15);
        }
    };

    std::unordered_map<VertexKey, uint32_t, VertexKeyHash> unique_map;
    unique_map.reserve(mesh.indices.size());

    for (const auto& idx : mesh.indices) {
        VertexKey key{ idx.vertex, idx.texcoord, idx.normal };

        auto it = unique_map.find(key);
        if (it != unique_map.end()) {
            out_data.Indices.push_back(it->second);
            continue;
        }

        const uint32_t new_index =
            static_cast<uint32_t>(out_data.Vertices.size() / kVertexStride);

        // Position
        if (idx.vertex < 0 || static_cast<size_t>(idx.vertex) >= vert_count) {
            err = "Vertex index out of range";
            return false;
        }
        size_t v_off = static_cast<size_t>(idx.vertex) * 3;
        out_data.Vertices.push_back(model.vertices[v_off + 0]);
        out_data.Vertices.push_back(model.vertices[v_off + 1]);
        out_data.Vertices.push_back(model.vertices[v_off + 2]);

        // Normal
        if (idx.normal >= 0 && static_cast<size_t>(idx.normal) < norm_count) {
            size_t n_off = static_cast<size_t>(idx.normal) * 3;
            out_data.Vertices.push_back(model.normals[n_off + 0]);
            out_data.Vertices.push_back(model.normals[n_off + 1]);
            out_data.Vertices.push_back(model.normals[n_off + 2]);
        } else {
            out_data.Vertices.push_back(0.0f);
            out_data.Vertices.push_back(0.0f);
            out_data.Vertices.push_back(0.0f);
        }

        // Texcoord reserved: currently push nothing (reserved for future)

        unique_map[key] = new_index;
        out_data.Indices.push_back(new_index);
    }

    return true;
}

} // namespace X_Y::Model
