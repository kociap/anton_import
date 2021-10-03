#pragma once

#include <anton/array.hpp>
#include <anton/expected.hpp>
#include <anton/math/vec2.hpp>
#include <anton/math/vec3.hpp>
#include <anton/slice.hpp>
#include <anton/string.hpp>
#include <anton/types.hpp>

namespace anton {
    struct Mesh_Import_Options {
        // When normals are not provided in the mesh data, calculate them manually.
        bool calculate_normals = false;
    };

    struct Mesh {
        String name;
        Array<math::Vec3> vertices;
        Array<math::Vec2> texture_coordinates;
        Array<math::Vec3> normals;
        Array<u32> indices;
    };

    // import_obj
    // Import Wavefront OBJ mesh.
    //
    [[nodiscard]] Expected<Array<Mesh>, String> import_obj(Slice<u8 const> data, Mesh_Import_Options options);
} // namespace anton
