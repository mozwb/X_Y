#include "ModelLoader.h"
#include "Core/FilesSystem/include/FilesSystem.h"

#include <cstdio>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

namespace X_Y::Model {

// ----------------------------------------------------------------
// Internal helpers
// ----------------------------------------------------------------

static inline bool isWhitespace(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static inline bool isEndOfLine(char c) {
    return c == '\n' || c == '\r' || c == '\0';
}

/// Skip whitespace, return pointer to first non-whitespace (or '\0')
static const char* skipWhitespace(const char* p) {
    while (*p && isWhitespace(*p)) ++p;
    return p;
}

/// Skip to end of line, return pointer to '\0' or next line
static const char* skipLine(const char* p) {
    while (*p && *p != '\n' && *p != '\r') ++p;
    if (*p == '\r') ++p;
    if (*p == '\n') ++p;
    return p;
}

/// Parse a float from string, advance p past it.
/// Returns false on failure.
static bool parseFloat(const char*& p, float& out) {
    p = skipWhitespace(p);
    char* end = nullptr;
    out = static_cast<float>(std::strtod(p, &end));
    if (end == p) return false; // no digits consumed
    p = end;
    return true;
}

/// Parse an int from string, advance p past it.
/// Returns false on failure.
static bool parseInt(const char*& p, int& out) {
    p = skipWhitespace(p);
    // OBJ indices can be negative (relative indexing)
    char* end = nullptr;
    out = static_cast<int>(std::strtol(p, &end, 10));
    if (end == p) return false;
    p = end;
    return true;
}

/// Convert 1-based OBJ index to 0-based.
/// If index > 0: 1-based → 0-based (subtract 1)
/// If index < 0: relative to current count (negative)
/// If index == 0: invalid OBJ, treat as error
static int resolveIndex(int idx, int count) {
    if (idx > 0)  return idx - 1;
    if (idx < 0)  return count + idx; // relative: count + negative
    return -1; // 0 is invalid in OBJ
}

// ----------------------------------------------------------------
// Core parser
// ----------------------------------------------------------------

bool LoadObjFromMemory(const char* data, size_t size, Model& out_model, std::string& err) {
    out_model.Clear();

    const char* end = data + size;
    const char* p = data;

    // Temporary storage for face parsing
    std::vector<int> face_v, face_vt, face_vn;

    // Current mesh
    Mesh current_mesh;
    bool has_mesh = false;

    auto flushMesh = [&]() {
        if (has_mesh && !current_mesh.indices.empty()) {
            if (current_mesh.name.empty()) {
                current_mesh.name = "default";
            }
            out_model.meshes.push_back(std::move(current_mesh));
        }
        current_mesh = Mesh();
        has_mesh = false;
    };

    while (p < end && *p) {
        // Skip empty lines and comments
        if (*p == '#' || isEndOfLine(*p)) {
            p = skipLine(p);
            continue;
        }

        // First character determines the type
        // We need at least 2 characters to distinguish 'v' vs 'vn'/'vt'
        if (p + 1 >= end) break;

        const char* line_start = p;

        switch (*p) {
        case 'v': {
            // v, vn, vt
            char type = p[1];
            if (isWhitespace(type) || isEndOfLine(type)) {
                // 'v' - vertex position: x y z [w]
                p = skipWhitespace(p + 1);
                float x, y, z;
                if (!parseFloat(p, x) || !parseFloat(p, y) || !parseFloat(p, z)) {
                    // skip malformed line
                    p = skipLine(line_start);
                    continue;
                }
                out_model.vertices.push_back(x);
                out_model.vertices.push_back(y);
                out_model.vertices.push_back(z);
            } else if (type == 'n') {
                // 'vn' - normal: nx ny nz
                p = skipWhitespace(p + 2);
                float nx, ny, nz;
                if (!parseFloat(p, nx) || !parseFloat(p, ny) || !parseFloat(p, nz)) {
                    p = skipLine(line_start);
                    continue;
                }
                out_model.normals.push_back(nx);
                out_model.normals.push_back(ny);
                out_model.normals.push_back(nz);
            } else if (type == 't') {
                // 'vt' - texcoord: u v [w]
                p = skipWhitespace(p + 2);
                float u, v;
                if (!parseFloat(p, u) || !parseFloat(p, v)) {
                    p = skipLine(line_start);
                    continue;
                }
                out_model.texcoords.push_back(u);
                out_model.texcoords.push_back(v);
            }
            // skip remaining tokens on the line (w component, etc.)
            p = skipLine(p);
            break;
        }

        case 'f': {
            // 'f' - face: v1/vt1/vn1 v2/vt2/vn2 ...
            if (!isWhitespace(p[1]) && p[1] != '\0') {
                // 'f' prefixes something else like 'f' in a word, skip
                p = skipLine(p);
                break;
            }

            if (!has_mesh) {
                has_mesh = true;
            }

            p = skipWhitespace(p + 1);
            face_v.clear();
            face_vt.clear();
            face_vn.clear();

            // Parse all vertex references on this line
            while (*p && !isEndOfLine(*p)) {
                p = skipWhitespace(p);
                if (isEndOfLine(*p)) break;

                // Parse triple: v, v/vt, v/vt/vn, v//vn
                int vi = 0, ti = -1, ni = -1;

                // First number is always the vertex index
                if (!parseInt(p, vi)) break;

                if (*p == '/') {
                    ++p; // skip '/'
                    // texcoord index (may be empty: v//vn)
                    if (*p != '/') {
                        int parsed_ti = 0;
                        if (parseInt(p, parsed_ti)) {
                            ti = parsed_ti;
                        }
                    }
                    if (*p == '/') {
                        ++p; // skip '/'
                        // normal index
                        if (!isEndOfLine(*p) && *p != ' ' && *p != '\t') {
                            int parsed_ni = 0;
                            if (parseInt(p, parsed_ni)) {
                                ni = parsed_ni;
                            }
                        }
                    }
                }

                int vert_count = static_cast<int>(out_model.vertices.size() / 3);
                int tex_count  = static_cast<int>(out_model.texcoords.size() / 2);
                int norm_count = static_cast<int>(out_model.normals.size() / 3);

                face_v.push_back(resolveIndex(vi, vert_count));
                face_vt.push_back(resolveIndex(ti, tex_count));
                face_vn.push_back(resolveIndex(ni, norm_count));

                // Skip trailing spaces before next token
                p = skipWhitespace(p);
            }

            // Triangulate the face
            size_t verts = face_v.size();
            if (verts >= 3) {//仅使用于凸面的裁剪
                // Fan triangulation: (0,1,2), (0,2,3), (0,3,4), ...
                for (size_t i = 1; i + 1 < verts; ++i) {
                    // Triangle: face[0], face[i], face[i+1]
                    current_mesh.indices.push_back({face_v[0], face_vt[0], face_vn[0]});
                    current_mesh.indices.push_back({face_v[i], face_vt[i], face_vn[i]});
                    current_mesh.indices.push_back({face_v[i + 1], face_vt[i + 1], face_vn[i + 1]});
                }
            }

            p = skipLine(p);
            break;
        }

        case 'g': // group name
        case 'o': { // object name
            if (!isWhitespace(p[1]) && p[1] != '\0') {
                p = skipLine(p);
                break;
            }
            if (has_mesh && !current_mesh.indices.empty()) {
                out_model.meshes.push_back(std::move(current_mesh));
                current_mesh = Mesh();
            }

            p = skipWhitespace(p + 1);
            const char* name_start = p;
            while (*p && !isWhitespace(*p) && !isEndOfLine(*p)) ++p;
            if (name_start < p) {
                current_mesh.name.assign(name_start, p);
            } else if (*p == 'g') {
                current_mesh.name = "default";
            }
            // 'o' without a name is fine, will use "default"
            has_mesh = true;
            p = skipLine(p);
            break;
        }

        case 'u': {
            // 'usemtl' - skip material switches for now
            // If we encounter usemtl, flush current mesh and start a new one
            if (p[1] == 's' && p[2] == 'e' && p[3] == 'm' && p[4] == 't' && p[5] == 'l' &&
                (isWhitespace(p[6]) || p[6] == '\0')) {
                if (has_mesh && !current_mesh.indices.empty()) {
                    out_model.meshes.push_back(std::move(current_mesh));
                    current_mesh = Mesh();
                }
                // Extract material name as mesh name
                p = skipWhitespace(p + 6);
                const char* mtl_start = p;
                while (*p && !isWhitespace(*p) && !isEndOfLine(*p)) ++p;
                if (mtl_start < p) {
                    current_mesh.name.assign(mtl_start, p);
                }
                has_mesh = true;
            }
            p = skipLine(p);
            break;
        }

        case 's': // smoothing group - skip
        case '#': // comment
        case 'm': // mtllib - skip
        default:
            p = skipLine(p);
            break;
        }
    }

    // Flush last mesh
    if (has_mesh && !current_mesh.indices.empty()) {
        if (current_mesh.name.empty()) {
            current_mesh.name = "default";
        }
        out_model.meshes.push_back(std::move(current_mesh));
    }

    // If no meshes were created but we have vertices, create a default mesh
    if (out_model.meshes.empty() && !out_model.vertices.empty()) {
        Mesh default_mesh;
        default_mesh.name = "default";
        out_model.meshes.push_back(std::move(default_mesh));
    }

    return true;
}

bool LoadObj(const std::string& filepath, Model& out_model, std::string& err) {
    Buffer buf = FilesSystem::ReadFileBinary(filepath);
    if (!buf.Size) {
        err = "Failed to read file: " + filepath;
        return false;
    }

    return LoadObjFromMemory(
        reinterpret_cast<const char*>(buf.Data),
        buf.Size,
        out_model,
        err
    );
}

} // namespace X_Y::Model
