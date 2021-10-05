#include <anton/import.hpp>

#include <anton/string7_view.hpp>

namespace anton {
    using data_iterator = u8 const*;

    struct Face_Internal {
        Array<u32> vertex_indices;
        Array<u32> texture_coordinate_indices;
        Array<u32> normal_indices;
    };

    struct Mesh_Internal {
        String name;
        Array<Face_Internal> faces;
    };

    [[nodiscard]] static bool is_whitespace(u8 const c) {
        return c <= 32;
    }

    [[nodiscard]] static bool is_digit(u8 const c) {
        return c >= '0' && c <= '9';
    }

    static void ignore_whitespace(data_iterator& iter, data_iterator const& end) {
        while(iter != end && is_whitespace(*iter)) {
            ++iter;
        }
    }

    static void ignore_whitespace_until_newline(data_iterator& iter, data_iterator const& end) {
        while(iter != end && is_whitespace(*iter) && *iter != '\n') {
            ++iter;
        }
    }

    static void ignore_until_newline(data_iterator& iter, data_iterator const& end) {
        while(iter != end && *iter != '\n') {
            ++iter;
        }
    }

    [[nodiscard]] static bool match(data_iterator& iter, data_iterator const& end, String7_View const string) {
        data_iterator const backup = iter;
        for(char8 const c: string) {
            if(iter == end || *iter != c) {
                iter = backup;
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] static Expected<float, String> read_float(data_iterator& iter, data_iterator const& end) {
        ignore_whitespace(iter, end);

        float sign = 1.0f;
        if(match(iter, end, "-"_sv7)) {
            sign = -1.0f;
        }

        float number = 0.0f;
        while(iter != end && is_digit(*iter)) {
            number *= 10.0f;
            number += static_cast<float>(*iter - '0');
            ++iter;
        }

        // TODO: Is period not required?
        [[maybe_unused]] bool const matched_period = match(iter, end, "."_sv7);

        float divisor = 1.0f;
        while(iter != end && is_digit(*iter)) {
            divisor /= 10.0f;
            number += static_cast<float>(*iter - 48) * divisor;
            ++iter;
        }

        return {expected_value, sign * number};
    }

    [[nodiscard]] static Expected<i64, String> read_int64(data_iterator& iter, data_iterator const& end) {
        ignore_whitespace(iter, end);

        i64 sign = 1;
        if(match(iter, end, "-"_sv7)) {
            sign = -1;
        }

        i64 number = 0;
        while(iter != end && is_digit(*iter)) {
            number *= 10;
            number += static_cast<i64>(*iter - 48);
            ++iter;
        }

        return {expected_value, sign * number};
    }

    static Expected<void, String> parse(data_iterator iter, data_iterator end, Array<math::Vec3>& vertices, Array<math::Vec3>& normals,
                                        Array<math::Vec2>& texture_coordinates, Array<Mesh_Internal>& meshes_internal) {
        Mesh_Internal* current_mesh = nullptr;
        while(iter != end) {
            ignore_whitespace(iter, end);
            if(match(iter, end, "v "_sv7)) {
                // Geometric vertex
                // May have 4 (x, y, z, w) parameters if the object is a rational curve or a surface.
                // TODO: Add error when 4th component is present.
                Expected<f32, String> x = read_float(iter, end);
                if(!x) {
                    return {expected_error, ANTON_MOV(x.error())};
                }

                Expected<f32, String> y = read_float(iter, end);
                if(!y) {
                    return {expected_error, ANTON_MOV(y.error())};
                }

                Expected<f32, String> z = read_float(iter, end);
                if(!z) {
                    return {expected_error, ANTON_MOV(z.error())};
                }
                // Skip the w component because we don't support it.
                ignore_until_newline(iter, end);
                vertices.push_back(math::Vec3{x.value(), y.value(), z.value()});
            } else if(match(iter, end, "vn"_sv7)) {
                // Normal vector
                Expected<f32, String> x = read_float(iter, end);
                if(!x) {
                    return {expected_error, ANTON_MOV(x.error())};
                }

                Expected<f32, String> y = read_float(iter, end);
                if(!y) {
                    return {expected_error, ANTON_MOV(y.error())};
                }

                Expected<f32, String> z = read_float(iter, end);
                if(!z) {
                    return {expected_error, ANTON_MOV(z.error())};
                }
                normals.push_back(math::Vec3{x.value(), y.value(), z.value()});
            } else if(match(iter, end, "vt"_sv7)) {
                // Vertex texture coordinates uvw.
                Expected<f32, String> u = read_float(iter, end);
                if(!u) {
                    return {expected_error, ANTON_MOV(u.error())};
                }

                // v is an optional argument.
                Expected<f32, String> v = read_float(iter, end);
                // w is an optional argument. We ignore it since we don't support 3D textures.
                [[maybe_unused]] Expected<f32, String> w = read_float(iter, end);

                if(v) {
                    texture_coordinates.push_back(math::Vec2{u.value(), v.value()});
                } else {
                    texture_coordinates.push_back(math::Vec2{u.value(), 0.0f});
                }
            } else if(match(iter, end, "f"_sv7)) {
                // Face
                // Note: Reference numbers in OBJ format may be negative (relative to the current position).

                // We require mesh name ('o' statement) to be present before any faces are defined.
                if(current_mesh == nullptr) {
                    return {expected_error, "error"_s};
                }

                Face_Internal& face = current_mesh->faces.emplace_back();
                while(true) {
                    Expected<i64, String> vertex_index_result = read_int64(iter, end);
                    if(vertex_index_result) {
                        i64 vertex_index = vertex_index_result.value();
                        // We turn the index into an absolute value.
                        if(vertex_index < 0) {
                            // A negative index references attribute relative to the current end of the sequence,
                            // i.e. the end of the sequence that has been parsed at that point in the
                            // parsing process of the OBJ data.
                            // We don't have to subtract 1 because the greatest value is -1, i.e. the last element.
                            vertex_index = static_cast<i64>(vertices.size()) + vertex_index;
                        } else {
                            // OBJ uses 1 based arrays, thus subtract 1.
                            vertex_index -= 1;
                        }
                        face.vertex_indices.push_back(vertex_index);
                    } else {
                        return {expected_error, ANTON_MOV(vertex_index_result.error())};
                    }

                    // Detect v//vn patterns.
                    if(!match(iter, end, "//"_sv7)) {
                        if(!match(iter, end, "/"_sv7)) {
                            return {expected_error, "expected '/' in 'f' statement"_s};
                        }

                        Expected<i64, String> uv_index_result = read_int64(iter, end);
                        if(uv_index_result) {
                            i64 uv_index = uv_index_result.value();
                            // We turn the index into an absolute value.
                            if(uv_index < 0) {
                                // A negative index references attribute relative to the current end of the sequence,
                                // i.e. the end of the sequence that has been parsed at that point in the
                                // parsing process of the OBJ data.
                                // We don't have to subtract 1 because the greatest value is -1, i.e. the last element.
                                uv_index = static_cast<i64>(texture_coordinates.size()) + uv_index;
                            } else {
                                // OBJ uses 1 based arrays, thus subtract 1.
                                uv_index -= 1;
                            }
                            face.texture_coordinate_indices.push_back(uv_index);
                        } else {
                            return {expected_error, ANTON_MOV(uv_index_result.error())};
                        }

                        if(!match(iter, end, "/"_sv7)) {
                            return {expected_error, "expected '/' in 'f' statement"_s};
                        }
                    }

                    // We have to check for whitespace because otherwise we will match the index from the next group.
                    // Newline ends the statement, space signifies the end of the current group.
                    if(match(iter, end, "\n"_sv7)) {
                        break;
                    }

                    if(match(iter, end, " "_sv7)) {
                        continue;
                    }

                    Expected<i64, String> normal_index_result = read_int64(iter, end);
                    if(normal_index_result) {
                        i64 normal_index = normal_index_result.value();
                        // We turn the index into an absolute value.
                        if(normal_index < 0) {
                            // A negative index references attribute relative to the current end of the sequence,
                            // i.e. the end of the sequence that has been parsed at that point in the
                            // parsing process of the OBJ data.
                            // We don't have to subtract 1 because the greatest value is -1, i.e. the last element.
                            normal_index = static_cast<i64>(texture_coordinates.size()) + normal_index;
                        } else {
                            // OBJ uses 1 based arrays, thus subtract 1.
                            normal_index -= 1;
                        }
                        face.normal_indices.push_back(normal_index);
                    } else {
                        return {expected_error, ANTON_MOV(normal_index_result.error())};
                    }
                }
            } else if(match(iter, end, "o"_sv7)) {
                // Object name
                // We create a new mesh whenver we find a name.
                ignore_whitespace_until_newline(iter, end);

                if(*iter == '\n') {
                    return {expected_error, "error"_s};
                }

                data_iterator const begin = iter;
                while(iter != end && !is_whitespace(*iter)) {
                    ++iter;
                }
                current_mesh = &meshes_internal.emplace_back();
                current_mesh->name = String((char8 const*)begin, (char8 const*)iter);
            } else {
                // Skip comments and unsupported or unknown statements.
                ignore_until_newline(iter, end);
            }
        }

        return {expected_value};
    }

    Expected<Array<Mesh>, String> import_obj(Slice<u8 const> const data, [[maybe_unused]] Mesh_Import_Options const options) {
        // TODO: Assumes triangulated faces. Triangulate faces ourselves.

        Array<math::Vec3> vertices;
        Array<math::Vec3> normals;
        Array<math::Vec2> texture_coordinates;
        Array<Mesh_Internal> meshes_internal;
        Expected<void, String> parse_result = parse(data.begin(), data.end(), vertices, normals, texture_coordinates, meshes_internal);
        if(!parse_result) {
            return {expected_error, ANTON_MOV(parse_result.error())};
        }

        Array<Mesh> meshes(reserve, meshes_internal.size());
        for(Mesh_Internal& mesh_internal: meshes_internal) {
            Mesh& mesh = meshes.emplace_back();
            mesh.name = ANTON_MOV(mesh_internal.name);
            for(Face_Internal const& face_internal: mesh_internal.faces) {
                for(u32 const index: face_internal.vertex_indices) {
                    mesh.vertices.push_back(vertices[index]);
                    u32 new_index = mesh.vertices.size() - 1;
                    mesh.indices.push_back(new_index);
                }

                for(u32 const index: face_internal.normal_indices) {
                    mesh.normals.push_back(normals[index]);
                }

                for(u32 const index: face_internal.texture_coordinate_indices) {
                    mesh.texture_coordinates.push_back(texture_coordinates[index]);
                }
            }
        }

        // TODO: Calculate normals is options.calculate_normals is true

        return {expected_value, ANTON_MOV(meshes)};
    }
} // namespace anton
