#include <cstdlib>
#include "our_gl.h"
#include "model.h"

extern mat<4,4> ModelView, Perspective; // "OpenGL" state matrices and
extern std::vector<double> zbuffer;     // the depth buffer

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
        const int y
    ) const {
        TGAColor color;
        
        // ambient
        double ka = .5;

        // diffuse
        // method 1: trivial method
        // vec3 n = normalized(cross(tri[2] - tri[0], tri[1] - tri[0]));
        vec3 n{0, 0, 0};
        // method 2: smooth shading
        // for (int i = 0; i < 3; ++i) {
        //     n = n + bar[i] * nms[i];
        // }
        // method 3: normal mapping
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
        for (int i = 0; i < 3; ++i) {
            n[i] = c[2 - i] / 255. * 2 - 1;
        }
        // to match eye coordinates
        n = normalized((ModelView * vec4{n.x, n.y, n.z, 0.}).xyz());
        double kd = std::max(0., n * l);

        // specular
        TGAColor sp_color = sp_texture_map.get(
            uv.x * sp_texture_map.width(), 
            (1 - uv.y) * sp_texture_map.height()
        );
        vec3 r = normalized(2 * n * (n * l) - l);
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

        for (int i = 0; i < 3; ++i) {
            color[i] = std::min(255., df_color[i] * (ka + kd + 1.2 * ks));
        }

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

    lookat(eye, center, up);                                   // build the ModelView   matrix
    init_perspective(norm(eye-center));                        // build the Perspective matrix
    init_viewport(width/16, height/16, width*7/8, height*7/8); // build the Viewport    matrix
    init_zbuffer(width, height);
    // TGAImage framebuffer(width, height, TGAImage::RGB, {177, 195, 209, 255});
    TGAImage framebuffer(width, height, TGAImage::RGB, {0, 0, 0, 0});
    

    for (int m=1; m<argc; m++) {                    // iterate through all input objects
        Model model(argv[m]);                       // load the data
        RandomShader shader(light, model);
        shader.fb = &framebuffer;
        const std::string nm_suffix = "_nm.tga";
        const std::string df_suffix = "_diffuse.tga";
        const std::string sp_suffix = "_spec.tga";
        shader.normal_map.read_tga_file(texture_name_converter(argv[m], nm_suffix));
        shader.df_texture_map.read_tga_file(texture_name_converter(argv[m], df_suffix));
        shader.sp_texture_map.read_tga_file(texture_name_converter(argv[m], sp_suffix));
        for (int f=0; f<model.nfaces(); f++) {      // iterate through all facets
            // shader.color = { 
            //     static_cast<unsigned char>(std::rand()%255), 
            //     static_cast<unsigned char>(std::rand()%255), 
            //     static_cast<unsigned char>(std::rand()%255), 
            //     255 
            // };
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

    framebuffer.write_tga_file("framebuffer.tga");
    return 0;
}

