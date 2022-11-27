#include "obj.h"
#include "common.h"
#include <iostream>
#include "r_mesh.h"

Mesh load_obj_from_file(const char* filepath)
{
    std::string inputfile = filepath;
    tinyobj::ObjReaderConfig reader_config;
    reader_config.mtl_search_path = "D:\\Projects\\GigaRayV0\\data"; // Path to material files

    tinyobj::ObjReader reader;

    if (!reader.ParseFromFile(inputfile, reader_config)) {
        if (!reader.Error().empty()) {
            std::cerr << "TinyObjReader: " << reader.Error();
        }
        exit(1);
    }

    if (!reader.Warning().empty()) {
        std::cout << "TinyObjReader: " << reader.Warning();
    }

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    glm::vec3 min = glm::vec3(INFINITY);
    glm::vec3 max = glm::vec3(-INFINITY);

    auto& attrib = reader.GetAttrib();
    auto& shapes = reader.GetShapes();
    auto& materials = reader.GetMaterials();

    for (size_t v = 0; v < attrib.vertices.size() / 3; ++v)
    {
        float x = attrib.vertices[3 * v + 0];
        float y = attrib.vertices[3 * v + 1];
        float z = attrib.vertices[3 * v + 2];
        glm::vec3 new_vert = glm::vec3(x, y, z);
        min = glm::min(min, new_vert);
        max = glm::max(max, new_vert);
        //float nx = attrib.normals[3 * v + 0];
        //float ny = attrib.normals[3 * v + 1];
        //float nz = attrib.normals[3 * v + 2];
        //float tx = attrib.texcoords[2 * v + 0];
        //float ty = attrib.texcoords[2 * v + 1];
        //vertices.push_back({ new_vert /*glm::vec3(nx, ny, nz), glm::vec2(tx, ty)*/});
    }

    uint32_t idx_ = 0;
    // Loop over shapes
    for (size_t s = 0; s < shapes.size(); s++) {
        // Loop over faces(polygon)
        size_t index_offset = 0;
        for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++) {
            size_t fv = size_t(shapes[s].mesh.num_face_vertices[f]);
            assert(fv == 3);
            // Loop over vertices in the face.
            for (size_t v = 0; v < fv; v++) {
                // access to vertex
                tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];
                tinyobj::real_t vx = attrib.vertices[3 * size_t(idx.vertex_index) + 0];
                tinyobj::real_t vy = attrib.vertices[3 * size_t(idx.vertex_index) + 1];
                tinyobj::real_t vz = attrib.vertices[3 * size_t(idx.vertex_index) + 2];
                //indices.push_back(idx.vertex_index);
                indices.push_back(idx_++);

                Vertex new_vert;
                new_vert.pos = glm::vec3(vx, vy, vz);
                // Check if `normal_index` is zero or positive. negative = no normal data
                if (idx.normal_index >= 0) {
                    tinyobj::real_t nx = attrib.normals[3 * size_t(idx.normal_index) + 0];
                    tinyobj::real_t ny = attrib.normals[3 * size_t(idx.normal_index) + 1];
                    tinyobj::real_t nz = attrib.normals[3 * size_t(idx.normal_index) + 2];
                    new_vert.normal = glm::vec3(nx, ny, nz);
                }

                // Check if `texcoord_index` is zero or positive. negative = no texcoord data
                if (idx.texcoord_index >= 0) {
                    tinyobj::real_t tx = attrib.texcoords[2 * size_t(idx.texcoord_index) + 0];
                    tinyobj::real_t ty = attrib.texcoords[2 * size_t(idx.texcoord_index) + 1];
                    new_vert.texcoord = glm::vec2(tx, ty);
                }
                vertices.push_back(new_vert);
                // Optional: vertex colors
                // tinyobj::real_t red   = attrib.colors[3*size_t(idx.vertex_index)+0];
                // tinyobj::real_t green = attrib.colors[3*size_t(idx.vertex_index)+1];
                // tinyobj::real_t blue  = attrib.colors[3*size_t(idx.vertex_index)+2];
            }
            index_offset += fv;

            // per-face material
            (void)shapes[s].mesh.material_ids[f];
        }
    }
    Mesh ret;
    ret.vertices = std::move(vertices);
    ret.indices = std::move(indices);

    return ret;
}
