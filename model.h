#include <vector>
#include "geometry.h"
#include "tgaimage.h"

class Model {
    std::vector<vec3> verts = {};    // array of vertices
    std::vector<vec3> norms = {};    // array of vertex normals
    std::vector<vec3> texts = {};    // array of vertex textures
    std::vector<int> facet_vrt = {}; // per-triangle index in the above array
    std::vector<int> facet_nml = {};  // per-triangle index of vertex normals
    std::vector<int> facet_txt = {};  // per-triangle index of vertex textures
public:
    Model(const std::string filename);
    int nverts() const; // number of vertices
    int nfaces() const; // number of triangles
    int nnorms() const; // number of vertex normals
    int ntexts() const; // number of vertex textures
    vec3 vert(const int i) const;                          // 0 <= i < nverts()
    vec3 vert(const int iface, const int nthvert) const;   // 0 <= iface <= nfaces(), 0 <= nthvert < 3
    vec3 norm(const int i) const;                          // 0 <= i < nnorms()
    vec3 norm(const int iface, const int nthnorm) const;   // 0 <= iface <= nfaces(), 0 <= nthnorm < 3
    vec3 text(const int i) const;                          // 0 <= i < ntexts()
    vec3 text(const int iface, const int nthtext) const;   // 0 <= iface <= nfaces(), 0 <= nthtext < 3
};

