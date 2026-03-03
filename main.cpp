#include <cmath>
#include <cstdlib>
#include <numbers>
#include <random>
#include "our_gl.h"
#include "model.h"

extern mat<4,4> ModelView, Perspective, Viewport; // "OpenGL" state matrices and
extern std::vector<double> zbuffer;     // the depth buffer
extern std::vector<double> shadowmap;
extern std::vector<std::vector<double>> AO_maps;
extern mat<4,4> VPM_light, VPM_camera;
extern std::vector<mat<4, 4>> VPM_AOs;

constexpr int shadow_w = 8000;   // shadowmap size
constexpr int shadow_h = 8000;

struct RandomShader : IShader {
    const Model &model;
    // TGAColor color = {};
    vec3 l;
    vec3 tri[3];  // triangle in eye coordinates
    TGAImage normal_map;
    TGAImage df_texture_map;
    TGAImage sp_texture_map;
    const TGAImage *fb = nullptr;  // framebuffer pointer for alpha blending

    RandomShader(const vec3 light, const Model &m) : model(m) {
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
        TGAColor color;
        vec4 f = { static_cast<double>(x), static_cast<double>(y), z, 1. };
        vec4 raw;
        vec3 p;
        double bias = .03;  // handle z-fighting problem
        
        // ambient
        double ka = .5;
        // double cnt = 0;
        // for (int i = 0; i < AO_maps.size(); ++i) {
        //     raw = VPM_AOs[i] * VPM_camera.invert() * f;
        //     p = (raw / raw.w).xyz();
        //     if (
        //         static_cast<int>(p.x) >= 0 && static_cast<int>(p.x) < shadow_w &&
        //         static_cast<int>(p.y) >= 0 && static_cast<int>(p.y) < shadow_h &&
        //         AO_maps[i][static_cast<int>(p.x) + static_cast<int>(p.y) * shadow_w] > p.z + bias
        //     ) ++cnt;
        // }
        // ka = (AO_maps.size() - cnt) / AO_maps.size();
        // ka = cnt / AO_maps.size();

        // diffuse
        // compute bases of tangent space
        vec3 n{0, 0, 0};
        for (int i = 0; i < 3; ++i) {
            n = n + bar[i] * nms[i];
        }
        vec3 e0 = tri[1] - tri[0], e1 = tri[2] - tri[0];
        vec2 u0 = uvs[1] - uvs[0], u1 = uvs[2] - uvs[0];
        mat<3, 2> E{vec2{e0.x, e1.x}, vec2{e0.y, e1.y}, vec2{e0.z, e1.z}};
        mat<2, 2> U{vec2{u0.x, u1.x}, vec2{u0.y, u1.y}};
        mat<3, 2> TB = E * U.invert();
        mat<3, 3> TBN{
            vec3{TB[0][0], TB[0][1], n[0]}, 
            vec3{TB[1][0], TB[1][1], n[1]}, 
            vec3{TB[2][0], TB[2][1], n[2]}
        };
        
        vec2 uv{0, 0};
        for (int i = 0; i < 3; ++i) {
            uv = uv + bar[i] * uvs[i];
        }
        TGAColor df_color = df_texture_map.get(
            uv.x * df_texture_map.width(), 
            (1 - uv.y) * df_texture_map.height()
        );
        // The growth direction of the Y-axis in TGA images 
        // is opposite to that of v coordinates
        TGAColor c = normal_map.get(uv.x * normal_map.width(), (1 - uv.y) * normal_map.height());
        vec3 nt{0, 0, 0};
        for (int i = 0; i < 3; ++i) {
            nt[i] = c[2 - i] / 255. * 2 - 1;
        }
        vec3 nw = TBN * nt;
        // to match eye coordinates
        nw = normalized(nw);
        double kd = std::max(0., nw * l);

        // specular
        TGAColor sp_color = sp_texture_map.get(
            uv.x * sp_texture_map.width(), 
            (1 - uv.y) * sp_texture_map.height()
        );
        vec3 r = normalized(2 * nw * (nw * l) - l);
        // Because we are in eye coordinates, camera is just in z-axis
        // double ks = std::pow(std::max(0., r.z), 35);
        double ks = std::pow(std::max(0., r.z), 10 + sp_color[0]);

        // transparent material (cornea): only add specular on top of existing pixel
        if (df_color.bytespp == 4 && fb) {
            TGAColor bg = fb->get(x, y);
            for (int i = 0; i < 3; ++i) {
                color[i] = std::min<int>(255, bg[i] + static_cast<int>(255. * ks));
            }
            return {false, color};
        }

        raw = VPM_light * VPM_camera.invert() * f;
        p = (raw / raw.w).xyz();
        if (
            static_cast<int>(p.x) >= 0 && static_cast<int>(p.x) < shadow_w &&
            static_cast<int>(p.y) >= 0 && static_cast<int>(p.y) < shadow_h &&
            shadowmap[static_cast<int>(p.x) + static_cast<int>(p.y) * shadow_w] > p.z + bias
        ) {    // shadow mapping
            for (int i = 0; i < 3; ++i) {
                color[i] = df_color[i] * ka;
            }
        } else {
            for (int i = 0; i < 3; ++i) {
                color[i] = std::min(255., df_color[i] * (ka + kd + .8 * ks));
            }
        }
        // for (int i = 0; i < 3; ++i) {
        //     color[i] = 255 * ka;
        // }

        return {false, color};
    }
};

std::string texture_name_converter(const std::string filename, const std::string suffix) {
    size_t pos = filename.rfind('.');
    if (pos == std::string::npos) {
        std::cerr << "Failed to convert!" << std::endl;
    }
    return filename.substr(0, pos) + suffix;
}

// void generate_sphere_lights(std::vector<vec3>& lights, const int n, const vec3 center, const vec3 up) {
//     std::random_device rd;
//     std::mt19937 gen(rd());
//     std::normal_distribution<double> dist(0., 1.);
//     for (int i = 0; i < n; ++i) {
//         vec3 l;
//         // avoid the case where (l - center) is nearly parallel to up
//         do {
//             l = vec3{dist(gen), dist(gen), dist(gen)};
//         } while (norm(cross(l - center, up)) / (norm(l - center) * norm(up)) < .1);
//         lights.push_back(normalized(l));
//     }
// }

void SSAO(TGAImage& framebuffer, const int sample_nums, const int R, const double d) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<double> distR(0, 1);
    std::uniform_real_distribution<double> distDepth(-d, d);
    std::uniform_real_distribution<double> distTheta(0, 2 * std::numbers::pi);
    double bias = .02;
    for (int x = 0; x < framebuffer.width(); ++x) {
        for (int y = 0; y < framebuffer.height(); ++y) {
            double z = zbuffer[x + y * framebuffer.width()];
            double cnt = 0;
            if (z == -1000.) continue;
            for (int i = 0; i < sample_nums; ++i) {
                double theta = distTheta(gen);
                double r = R * std::sqrt(distR(gen));
                // double dz = distDepth(gen);
                int sx = std::min(
                    framebuffer.width() - 1, 
                    std::max(0, x + static_cast<int>(r * std::cos(theta)))
                );
                int sy = std::min(
                    framebuffer.height() - 1, 
                    std::max(0, y + static_cast<int>(r * std::sin(theta)))
                );
                double sz = zbuffer[sx + sy * framebuffer.width()];
                if (sz == -1000.) continue;
                if (sz > z + bias) ++cnt;
            }
            double ratio = cnt / sample_nums;
            TGAColor c = framebuffer.get(x, y);
            for (int i = 0; i < 3; ++i) {
                c[i] *= (1 - ratio);
                // c[i] = 255 * (1 - ratio);
            }
            framebuffer.set(x, y, c);
        }
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " obj/model.obj" << std::endl;
        return 1;
    }

    constexpr int width  = 800;      // output image size
    constexpr int height = 800;
    constexpr int light_nums = 20;
    constexpr int R = 5;
    constexpr int sample_nums = 128;
    constexpr double R_depth = .08;
    constexpr vec3  light{1,  1, 1}; // light source
    constexpr vec3    eye{-1, 0, 2}; // camera position
    constexpr vec3 center{ 0, 0, 0}; // camera direction
    constexpr vec3     up{ 0, 1, 0}; // camera up vector
    
    // std::vector<vec3> lights;
    // generate_sphere_lights(lights, light_nums, center, up);
    TGAImage framebuffer(width, height, TGAImage::RGB, { 178, 195, 208, 128 });

    // light -> generate shadow map
    lookat(light, center, up);                                           // build the ModelView   matrix
    init_perspective(norm(light-center));                                // build the Perspective matrix
    init_viewport(shadow_w/16, shadow_h/16, shadow_w*7/8, shadow_h*7/8); // build the Viewport    matrix
    VPM_light = Viewport * Perspective * ModelView;
    init_buffer(shadowmap, shadow_w, shadow_h);
    
    for (int m=1; m<argc; m++) {                    // iterate through all input objects
        Model model(argv[m]);                       // load the data
        RandomShader shader(light, model);
        for (int f=0; f<model.nfaces(); f++) {      // iterate through all facets
            Triangle clip = { 
                shader.vertex(f, 0),  // assemble the primitive
                shader.vertex(f, 1),
                shader.vertex(f, 2) 
            };
            generate_depth_map(shadow_w, shadow_h, clip, shader, shadowmap);
        }
    }

    // AO
    // VPM_AOs.resize(light_nums);
    // AO_maps.resize(light_nums);
    // for (int i = 0; i < lights.size(); ++i) {
    //     vec3 l = lights[i];
    //     lookat(l, center, up);                               
    //     init_perspective(norm(l-center));
    //     VPM_AOs[i] = Viewport * Perspective * ModelView;
    //     init_buffer(AO_maps[i], shadow_w, shadow_h);
    //     std::cout << l.x << ", " << l.y << ", " << l.z << std::endl;
    //     for (int m=1; m<argc; m++) {                    // iterate through all input objects
    //         Model model(argv[m]);                       // load the data
    //         RandomShader shader(l, model);
    //         for (int f=0; f<model.nfaces(); f++) {      // iterate through all facets
    //             Triangle clip = { 
    //                 shader.vertex(f, 0),  // assemble the primitive
    //                 shader.vertex(f, 1),
    //                 shader.vertex(f, 2) 
    //             };
    //             generate_depth_map(shadow_w, shadow_h, clip, shader, AO_maps[i]);
    //         }
    //     }
    // }

    // camera
    lookat(eye, center, up);                                   // build the ModelView   matrix
    init_perspective(norm(eye-center));                        // build the Perspective matrix
    init_viewport(width/16, height/16, width*7/8, height*7/8); // build the Viewport    matrix
    VPM_camera = Viewport * Perspective * ModelView;
    init_buffer(zbuffer, width, height);

    for (int m=1; m<argc; m++) {                    // iterate through all input objects
        Model model(argv[m]);                       // load the data
        RandomShader shader(light, model);
        shader.fb = &framebuffer;
        const std::string nm_suffix = "_nm_tangent.tga";
        const std::string df_suffix = "_diffuse.tga";
        const std::string sp_suffix = "_spec.tga";
        shader.normal_map.read_tga_file(texture_name_converter(argv[m], nm_suffix));
        shader.df_texture_map.read_tga_file(texture_name_converter(argv[m], df_suffix));
        shader.sp_texture_map.read_tga_file(texture_name_converter(argv[m], sp_suffix));
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

    SSAO(framebuffer, sample_nums, R, R_depth);

    framebuffer.write_tga_file("framebuffer.tga");
    return 0;
}

