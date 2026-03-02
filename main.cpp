#include <cmath>
#include <tuple>
#include "geometry.h"
#include "model.h"
#include "tgaimage.h"

constexpr int width  = 800;
constexpr int height = 800;

constexpr TGAColor white   = {255, 255, 255, 255}; // attention, BGRA order
constexpr TGAColor green   = {  0, 255,   0, 255};
constexpr TGAColor red     = {  0,   0, 255, 255};
constexpr TGAColor blue    = {255, 128,  64, 255};
constexpr TGAColor yellow  = {  0, 200, 255, 255};

void line(int ax, int ay, int bx, int by, TGAImage &framebuffer, TGAColor color) {
    bool steep = std::abs(ax-bx) < std::abs(ay-by);
    if (steep) { // if the line is steep, we transpose the image
        std::swap(ax, ay);
        std::swap(bx, by);
    }
    if (ax>bx) { // make it left−to−right
        std::swap(ax, bx);
        std::swap(ay, by);
    }
    int y = ay;
    int ierror = 0;
    for (int x=ax; x<=bx; x++) {
        if (steep) // if transposed, de−transpose
            framebuffer.set(y, x, color);
        else
            framebuffer.set(x, y, color);
        ierror += 2 * std::abs(by-ay);
        if (ierror > bx - ax) {
            y += by > ay ? 1 : -1;
            ierror -= 2 * (bx-ax);
        }
    }
}

bool compareByY(const std::pair<int, int>& a, const std::pair<int, int>& b) {
    return a.second < b.second;
}

// Method 1: Scanline Rendering
void scanlineRendering(int ax, int ay, int bx, int by, int cx, int cy, 
                       TGAImage &framebuffer, TGAColor color) {
    std::vector<std::pair<int, int>> points{ {ax, ay}, {bx, by}, {cx, cy} };
    std::sort(points.begin(), points.end(), compareByY);

    int min_x = points[0].first, min_y = points[0].second;
    int mid_x = points[1].first, mid_y = points[1].second;
    int max_x = points[2].first, max_y = points[2].second;
    int lx, rx;
    for (int y = min_y; y <= max_y; ++y) {
        if (y <= mid_y) {      // bottom-half
            lx = min_x + (y - min_y) * (mid_x - min_x) / (mid_y - min_y);
            rx = min_x + (y - min_y) * (max_x - min_x) / (max_y - min_y);
        } else {               // top-half
            lx = mid_x + (y - mid_y) * (max_x - mid_x) / (max_y - mid_y);
            rx = min_x + (y - min_y) * (max_x - min_x) / (max_y - min_y);
        }
        if (lx > rx) std::swap(lx, rx);
        for (int x = lx; x <= rx; ++x) {
            framebuffer.set(x, y, color);
        }
    }
}

// Test if (x, y) is within the triangle composed of (ax, ay), (bx, by) and (cx, cy)
bool isInside(int ax, int ay, int bx, int by, int cx, int cy, int x, int y) {
    // cross product
    int crossAB = (x - ax) * (by - ay) - (bx - ax) * (y - ay);
    int crossBC = (x - bx) * (cy - by) - (cx - bx) * (y - by);
    int crossCA = (x - cx) * (ay - cy) - (ax - cx) * (y - cy);
    return (crossAB >= 0 && crossBC >= 0 && crossCA >= 0) ||
           (crossAB <= 0 && crossBC <= 0 && crossCA <= 0);
}

double signedTriangleArea(int ax, int ay, int bx, int by, int cx, int cy) {
    return .5 * ((bx - ax) * (cy - ay) - (cx - ax) * (by - ay));
}

vec3 barycentricCoords(
    int ax, int ay, int bx, int by, int cx, int cy, int x, int y
) {
    double total_area = signedTriangleArea(ax, ay, bx, by, cx, cy);
    double alpha = signedTriangleArea(x, y, bx, by, cx, cy) / total_area;
    double beta = signedTriangleArea(x, y, cx, cy, ax, ay) / total_area;
    double gamma = signedTriangleArea(x, y, ax, ay, bx, by) / total_area;
    return {alpha, beta, gamma};
}

// Method 2: Triangle Rasterization, Modern Apporach
void modernRasterizer(    
    int ax, int ay, // int az, 
    int bx, int by, // int bz, 
    int cx, int cy, // int cz, 
    TGAImage &framebuffer, TGAColor color
) {
    // bounding box
    int bblx = std::min({ax, bx, cx}), bbrx = std::max({ax, bx, cx});
    int bbby = std::min({ay, by, cy}), bbty = std::max({ay, by, cy});

    for (int x = bblx; x <= bbrx; ++x) {
        for (int y = bbby; y <= bbty; ++y) {
            // if (x, y) is inside the triangle
            auto [alpha, beta, gamma] = barycentricCoords(ax, ay, bx, by, cx, cy, x, y);
            if (alpha >= 0 && beta >= 0 && gamma >= 0) {
                framebuffer.set(x, y, color);
            }
            
        }
    }
}

void triangle(
    int ax, int ay, // int az, 
    int bx, int by, // int bz, 
    int cx, int cy, // int cz, 
    TGAImage &framebuffer, TGAColor color
) {
    // back-face culling
    if (signedTriangleArea(ax, ay, bx, by, cx, cy) < 1) return;
    modernRasterizer(ax, ay, bx, by, cx, cy, framebuffer, color);
}

std::tuple<int, int> project(vec3 v) {
    int x = (v.x + 1.) / 2 * width;
    int y = (v.y + 1.) / 2 * height;
    return {x, y};
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " obj/model.obj" << std::endl;
        return 1;
    }

    std::srand(std::time(NULL));
    
    TGAImage framebuffer(width, height, TGAImage::RGB);

    for (int m = 1; m < argc; ++m) {
        Model model(argv[m]);
        for (int i = 0; i < model.nfaces(); ++i) {
            auto [ax, ay] = project(model.vert(i, 0));
            auto [bx, by] = project(model.vert(i, 1));
            auto [cx, cy] = project(model.vert(i, 2));
            TGAColor color;
            for (int j = 0; j < 3; ++j) color[j] = std::rand() % 255;
            triangle(ax, ay, bx, by, cx, cy, framebuffer, color);
        }
    }


    framebuffer.write_tga_file("framebuffer.tga");
    return 0;
}