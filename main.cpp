#include <cmath>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iostream>
#include "model.hpp"
#include "tgaimage.h"

constexpr int width  = 800;
constexpr int height = 800;

constexpr TGAColor white   = {255, 255, 255, 255}; // attention, BGRA order
constexpr TGAColor green   = {  0, 255,   0, 255};
constexpr TGAColor red     = {  0,   0, 255, 255};
constexpr TGAColor blue    = {255, 128,  64, 255};
constexpr TGAColor yellow  = {  0, 200, 255, 255};

// Bresenham line drawing algorithm (all-integer rasterization)
void line(int ax, int ay, int bx, int by, TGAImage& framebuffer, TGAColor color) {
    bool steep = std::abs(ax - bx) < std::abs(ay - by);
    if (steep) {     // if line is steep, transpose the image
        std::swap(ax, ay);
        std::swap(bx, by);
    }
    if (ax > bx) {   // to satisfy for loop condition
        std::swap(ax, bx);
        std::swap(ay, by);
    }
    int y = ay;
    int ierror = 0;
    for (int x = ax; x <= bx; ++x) {
        // float t = (x - ax) / static_cast<float>(bx - ax);
        // int y = std::round(ay + (by - ay) * t);
        // float error = 0;
        if (steep) {  // if transposed, de−transpose
            framebuffer.set(y, x, color);
        } else {
            framebuffer.set(x, y, color);
        }
        // y += (by - ay) / static_cast<float>(bx - ax);
        ierror += 2 * std::abs(by - ay);
        // if (ierror > bx - ax) {
        //     y += by > ay ? 1 : -1;
        //     // error -= 1.;
        //     ierror -= 2 * (bx - ax);
        // }
        // if -> ?: (jcc -> cmov (in x86))
        y += (by > ay ? 1 : -1) * (ierror > bx - ax);
        ierror -= 2 * std::abs(bx - ax) * (ierror > bx - ax);
    }
}

std::tuple<int, int> project(vec3 v) {
    int x = (v.x + 1.) / 2 * width;
    int y = (v.y + 1.) / 2 * height;
    return {x, y};
}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " obj/model.obj" << std::endl;
        return 1;
    }

    Model model(argv[1]);
    TGAImage framebuffer(width, height, TGAImage::RGB);

    // int ax =  7, ay =  3;
    // int bx = 12, by = 37;
    // int cx = 62, cy = 53;

    // line(ax, ay, bx, by, framebuffer, blue);
    // line(cx, cy, bx, by, framebuffer, green);
    // line(cx, cy, ax, ay, framebuffer, yellow);
    // line(ax, ay, cx, cy, framebuffer, red);

    // framebuffer.set(ax, ay, white);
    // framebuffer.set(bx, by, white);
    // framebuffer.set(cx, cy, white);

    for (int i = 0; i < model.n_faces(); ++i) {
        auto [ax, ay] = project(model.vertex(i, 0));
        auto [bx, by] = project(model.vertex(i, 1));
        auto [cx, cy] = project(model.vertex(i, 2));
        line(ax, ay, bx, by, framebuffer, red);
        line(bx, by, cx, cy, framebuffer, red);
        line(cx, cy, ax, ay, framebuffer, red);
    }

    for (int i = 0; i < model.n_verices(); ++i) {
        auto [x, y] = project(model.vertex(i));
        framebuffer.set(x, y, white);
    }


    framebuffer.write_tga_file("framebuffer.tga");
    return 0;
}

