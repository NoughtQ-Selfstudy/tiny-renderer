#include <cmath>
#include <tuple>
#include "geometry.h"
#include "model.h"
#include "tgaimage.h"

constexpr int width  = 800;
constexpr int height = 800;
mat<4, 4> ModelView, Viewport, Perspective;

double signed_triangle_area(int ax, int ay, int bx, int by, int cx, int cy) {
    return .5*((by-ay)*(bx+ax) + (cy-by)*(cx+bx) + (ay-cy)*(ax+cx));
}

// void triangle(int ax, int ay, int az, int bx, int by, int bz, int cx, int cy, int cz, TGAImage &zbuffer, TGAImage &framebuffer, TGAColor color) {
void triangle(int ax, int ay, int az, int bx, int by, int bz, int cx, int cy, int cz, std::vector<std::vector<float>> &depth, TGAImage &framebuffer, TGAColor color) {
    int bbminx = std::max(0, std::min(std::min(ax, bx), cx)); // bounding box for the triangle clipped by the screen
    int bbminy = std::max(0, std::min(std::min(ay, by), cy)); // defined by its top left and bottom right corners
    int bbmaxx = std::min(framebuffer.width() -1, std::max(std::max(ax, bx), cx));
    int bbmaxy = std::min(framebuffer.height()-1, std::max(std::max(ay, by), cy));
    double total_area = signed_triangle_area(ax, ay, bx, by, cx, cy);
    if (total_area<1) return; // backface culling + discarding triangles that cover less than a pixel

#pragma omp parallel for
    for (int x=bbminx; x<=bbmaxx; x++) {
        for (int y=bbminy; y<=bbmaxy; y++) {
            double alpha = signed_triangle_area(x, y, bx, by, cx, cy) / total_area;
            double beta  = signed_triangle_area(x, y, cx, cy, ax, ay) / total_area;
            double gamma = signed_triangle_area(x, y, ax, ay, bx, by) / total_area;
            if (alpha<0 || beta<0 || gamma<0) continue; // negative barycentric coordinate => the pixel is outside the triangle
            float z = alpha * az + beta * bz + gamma * cz;
            // if (z <= zbuffer.get(x, y)[0]) continue;
            if (z <= depth[x][y]) continue;
            // zbuffer.set(x, y, {z});
            depth[x][y] = z;
            framebuffer.set(x, y, color);
        }
    }
}

void rasterize(const vec4 clip[3], std::vector<std::vector<double>> &zbuffer, TGAImage &framebuffer, TGAColor color) {
    vec4 ndc[3] = { clip[0] / clip[0].w, clip[1] / clip[1].w, clip[2] / clip[2].w };
    vec2 screen[3] = { (Viewport * ndc[0]).xy(), (Viewport * ndc[1]).xy(), (Viewport * ndc[2]).xy() };

    mat<3,3> ABC = {{ {screen[0].x, screen[0].y, 1.}, 
                      {screen[1].x, screen[1].y, 1.}, 
                      {screen[2].x, screen[2].y, 1.} 
                   }};
    if (ABC.det() < 1) return; // backface culling + discarding triangles that cover less than a pixel

    // bounding box for the triangle
    auto [bbminx, bbmaxx] = std::minmax({screen[0].x, screen[1].x, screen[2].x});
    // defined by its top left and bottom right corners
    auto [bbminy, bbmaxy] = std::minmax({screen[0].y, screen[1].y, screen[2].y}); 
#pragma omp parallel for
    // clip the bounding box by the screen
    for (int x = std::max<int>(bbminx, 0); x <= std::min<int>(bbmaxx, framebuffer.width() - 1); x++) { 
        for (int y = std::max<int>(bbminy, 0); y <= std::min<int>(bbmaxy, framebuffer.height() - 1); y++) {
            // barycentric coordinates of {x,y} w.r.t the triangle
            vec3 bc = ABC.invert_transpose() * vec3{static_cast<double>(x), static_cast<double>(y), 1.};
            // negative barycentric coordinate => the pixel is outside the triangle
            if (bc.x < 0 || bc.y < 0 || bc.z < 0) continue;
            double z = bc * vec3{ ndc[0].z, ndc[1].z, ndc[2].z };
            if (z <= zbuffer[x][y]) continue;
            zbuffer[x][y] = z;
            framebuffer.set(x, y, color);
        }
    }
}

vec3 rot(vec3 v) {
    constexpr double a = M_PI/6;
    static const mat<3,3> Ry = {{{std::cos(a), 0, std::sin(a)}, {0,1,0}, {-std::sin(a), 0, std::cos(a)}}};
    return Ry*v;
}

void lookat(const vec3 eye, const vec3 center, const vec3 up) {
    vec3 n = normalized(eye - center);
    vec3 l = normalized(cross(up, n));
    vec3 m = normalized(cross(n, l));
    ModelView = 
        mat<4, 4>{{ {l.x, l.y, l.z, 0}, {m.x, m.y, m.z, 0}, {n.x, n.y, n.z, 0}, {0, 0, 0, 1} }} * 
        mat<4, 4>{{ {1, 0, 0, -center.x}, {0, 1, 0, -center.y}, {0, 0, 1, -center.z}, {0, 0, 0, 1} }};
}

vec3 persp(vec3 v) {
    constexpr double c = 3.;
    return v / (1-v.z/c);
}

void perspective(double f) {
    Perspective = {{ {1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, -1/f, 1} }};
}

std::tuple<int,int,int> project(vec3 v) { // First of all, (x,y) is an orthogonal projection of the vector (x,y,z).
    return { (v.x + 1.) *  width/2,       // Second, since the input models are scaled to have fit in the [-1,1]^3 world coordinates,
             (v.y + 1.) * height/2,       // we want to shift the vector (x,y) and then scale it to span the entire screen.
             (v.z + 1.) *   255./2 };
}

void viewport(const int x, const int y, const int w, const int h) {
    Viewport = {{ {w/2., 0, 0, x + w/2.}, {0, h/2., 0, y + h/2.}, {0, 0, 1, 0}, {0, 0, 0, 1} }};
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " obj/model.obj" << std::endl;
        return 1;
    }

    constexpr vec3 eye{-1, 0, 2};    // camera position
    constexpr vec3 center{0, 0, 0};  // camera direction
    constexpr vec3 up{0, 1, 0};      // camera up vector

    lookat(eye, center, up);
    perspective(norm(eye - center));
    viewport(width / 16, height / 16, width * 7 / 8, height * 7 / 8);

    
    TGAImage framebuffer(width, height, TGAImage::RGB);
    TGAImage     zbuffer(width, height, TGAImage::GRAYSCALE);
    // std::vector<double> zbuffer(width * height, -std::numeric_limits<double>::max());
    // fix the bug about depth (use floating-point array)
    std::vector<std::vector<double>> depth(width, std::vector<double>(height, -std::numeric_limits<double>::max()));

    mat<4, 4> ComposedTransformation = Perspective * ModelView; 
    for (int i = 1; i < argc; ++i) {  // iterate through all input objects
        Model model(argv[i]);
        for (int j = 0; j < model.nfaces(); j++) { // iterate through all triangles
            // auto [ax, ay, az] = project(persp(rot(model.vert(i, 0))));
            // auto [bx, by, bz] = project(persp(rot(model.vert(i, 1))));
            // auto [cx, cy, cz] = project(persp(rot(model.vert(i, 2))));
            vec4 clip[3];
            for (int k = 0; k < 3; ++k) {
                vec3 v = model.vert(j, k);
                clip[k] = ComposedTransformation * vec4{v.x, v.y, v.z, 1.};
            }
            TGAColor rnd;
            for (int c=0; c<3; c++) rnd[c] = std::rand()%255;
            // triangle(ax, ay, az, bx, by, bz, cx, cy, cz, zbuffer, framebuffer, rnd);
            // triangle(ax, ay, az, bx, by, bz, cx, cy, cz, depth, framebuffer, rnd);
            rasterize(clip, depth, framebuffer, rnd);
        }
    }

    // fix the bug about depth
    // skip background pixels when computing min/max
    constexpr double bg = -std::numeric_limits<double>::max();
    double min_depth = std::numeric_limits<double>::max();
    double max_depth = -std::numeric_limits<double>::max();
    for (int i = 0; i < width; ++i) {
        for (int j = 0; j < height; ++j) {
            if (depth[i][j] == bg) continue;
            min_depth = std::min(min_depth, depth[i][j]);
            max_depth = std::max(max_depth, depth[i][j]);
        }
    }
    for (int i = 0; i < width; ++i) {
        for (int j = 0; j < height; ++j) {
            if (depth[i][j] == bg) {
                zbuffer.set(i, j, {0});
            } else {
                zbuffer.set(i, j, {
                    static_cast<unsigned char>((depth[i][j] - min_depth) / (max_depth - min_depth) * 255.)
                });
            }
        }
    }

    framebuffer.write_tga_file("framebuffer.tga");
    zbuffer.write_tga_file("zbuffer.tga");
    return 0;
}

