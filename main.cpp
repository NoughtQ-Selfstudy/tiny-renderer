#include <cmath>
#include <cstdlib>
#include <numbers>
#include <random>
#include "our_gl.h"
#include "model.h"

extern mat<4,4> ModelView, Perspective, Viewport; // "OpenGL" state matrices and
extern std::vector<double> zbuffer;     // the depth buffer

struct RandomShader : IShader {
    const Model &model;
    // TGAColor color = {};
    vec3 l;
    vec3 tri[3];  // triangle in eye coordinates
    vec4 color;
    TGAImage normal_map;
    TGAImage df_texture_map;
    TGAImage sp_texture_map;
    const TGAImage *fb = nullptr;  // framebuffer pointer for alpha blending

    RandomShader(const vec3 light, const vec4 c, const Model &m) : model(m), color(c) {
        l = normalized((ModelView * vec4{light.x, light.y, light.z, 0.}).xyz());
    }

    virtual vec4 vertex(const int face, const int vert) {
        vec3 v = model.vert(face, vert);                          // current vertex in object coordinates
        vec4 gl_Position = ModelView * vec4{v.x, v.y, v.z, 1.};
        tri[vert] = gl_Position.xyz();                            // in eye coordinates
        return Perspective * gl_Position;                         // in clip coordinates
    }

    virtual vec3 normal(const int face, const int norm) {
        vec3 vn = model.norm(face, norm);
        return normalized((ModelView * vec4{vn.x, vn.y, vn.z, 0.}).xyz());
    }

    virtual vec2 texture(const int face, const int text) {
        vec3 vt = model.text(face, text);
        return vec2{vt.x, vt.y};
    }

    virtual std::pair<bool,TGAColor> fragment(
        const vec3 bar, 
        const std::vector<vec3> nms,
        const std::vector<vec2> uvs,
        const int x,
        const int y,
        const double z
    ) const {
        TGAColor gl_color;
        
        // ambient
        double ka = .15;

        // diffuse
        // compute bases of tangent space
        vec3 n{0, 0, 0};
        for (int i = 0; i < 3; ++i) {
            n = n + bar[i] * nms[i];
        }
        n = normalized(n);
        double kd = std::max(0., n * l);

        // specular
        // vec3 r = normalized(2 * n * (n * l) - l);
        // Because we are in eye coordinates, camera is just in z-axis
        // double ks = std::pow(std::max(0., r.z), 35);

        // double intensity = ka + .4 * kd + 1.2 * ks;
        double intensity = ka + kd;
        if (intensity > .66) {
            intensity = 1;
        } else if (intensity > .33) {
            intensity = .66;
        } else {
            intensity = .33;
        }
        for (int i = 0; i < 3; ++i) {
            gl_color[i] = std::min(255., intensity * color[i]);
        }

        return {false, gl_color};
    }
};

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " obj/model.obj" << std::endl;
        return 1;
    }

    constexpr int width  = 800;      // output image size
    constexpr int height = 800;
    constexpr vec3  light{1,  1, 1}; // light source
    constexpr vec3    eye{-1, 0, 2}; // camera position
    constexpr vec3 center{ 0, 0, 0}; // camera direction
    constexpr vec3     up{ 0, 1, 0}; // camera up vector
    constexpr vec4  color{22*4, 56*4, 147*4, 255};
    
    TGAImage framebuffer(width, height, TGAImage::RGB, { 178, 195, 208, 128 });

    lookat(eye, center, up);                                   // build the ModelView   matrix
    init_perspective(norm(eye-center));                        // build the Perspective matrix
    init_viewport(width/16, height/16, width*7/8, height*7/8); // build the Viewport    matrix
    init_buffer(zbuffer, width, height);

    for (int m=1; m<argc; m++) {                    // iterate through all input objects
        Model model(argv[m]);                       // load the data
        RandomShader shader(light, color, model);
        shader.fb = &framebuffer;
        for (int f=0; f<model.nfaces(); f++) {      // iterate through all facets
            Triangle clip = { shader.vertex(f, 0),  // assemble the primitive
                              shader.vertex(f, 1),
                              shader.vertex(f, 2) };
            std::vector<vec3> normals = {
                shader.normal(f, 0),
                shader.normal(f, 1),
                shader.normal(f, 2)
            };
            std::vector<vec2> uvs = {
                shader.texture(f, 0),
                shader.texture(f, 1),
                shader.texture(f, 2)
            };
            rasterize(clip, normals, uvs, shader, framebuffer);   // rasterize the primitive
        }
    }

    // edge detection
    constexpr mat<3, 3> kx = {vec3{-1, 0, 1}, vec3{-2, 0, 2}, vec3{-1, 0, 1}};
    constexpr mat<3, 3> ky = {vec3{-1, -2, -1}, vec3{0, 0, 0}, vec3{1, 2, 1}};
    double threshold = .2;
    for (int x = 0; x < framebuffer.width(); ++x) {
        for (int y = 0; y < framebuffer.height(); ++y) {
            if (zbuffer[x + y * framebuffer.width()] == -1000.) continue;
            double gx = 0, gy = 0;
            int cnt = 0;
            for (int i = -1; i <= 1; ++i) {
                for (int j = -1; j <= 1; ++j) {
                    if (
                        x + i < 0 || x + i >= framebuffer.width() || 
                        y + j < 0 || y + j >= framebuffer.height()
                    ) continue;
                    gx += kx[i + 1][j + 1] * zbuffer[(x + i) + (y + j) * framebuffer.width()];
                    gy += ky[i + 1][j + 1] * zbuffer[(x + i) + (y + j) * framebuffer.width()];
                    ++cnt;
                }
            }
            // gx /= cnt; gy /= cnt;
            double es = std::sqrt(gx * gx + gy * gy);
            if (es > threshold) {
                framebuffer.set(x, y, {0, 0, 0, 0});
            }
        }
    }

    framebuffer.write_tga_file("framebuffer.tga");
    return 0;
}

