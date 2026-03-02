#include "tgaimage.h"
#include "geometry.h"

void lookat(const vec3 eye, const vec3 center, const vec3 up);
void init_perspective(const double f);
void init_viewport(const int x, const int y, const int w, const int h);
void init_zbuffer(const int width, const int height);
void init_shadowmap(const int width, const int height);

struct IShader {
    virtual std::pair<bool,TGAColor> fragment(
        const vec3 bar, 
        const std::vector<vec3> nms,
        const std::vector<vec2> uvs,
        const int x,
        const int y,
        const double z
    ) const = 0;
};

typedef vec4 Triangle[3]; // a triangle primitive is made of three ordered points
void rasterize(
    const Triangle &clip, 
    const std::vector<vec3> &normals, 
    const std::vector<vec2> &uvs,
    const IShader &shader, 
    TGAImage &framebuffer
);
void generate_shadow_map(
    const int w,
    const int h,
    const Triangle &clip, 
    const IShader &shader
);

TGAColor phong(const Triangle t, const vec3 eye, const vec3 center);