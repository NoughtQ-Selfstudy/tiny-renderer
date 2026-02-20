#include <fstream>
#include <iostream>
#include <string>
#include <sstream>
#include "model.hpp"

Model::Model(const char* filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cout << "file cannot be opened!" << std::endl;
    }

    std::string line, type, idx, f[3];
    float a, b, c;
    vec3 v;
    size_t pos;

    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;

        std::stringstream ss(line);
        ss >> type;

        if (type == "v") {
            ss >> a >> b >> c;
            v = {a, b, c};
            vertices.push_back(v);
        } else if (type == "f") {
            for (int i = 0; i < 3; ++i) {
                ss >> f[i];
                pos = f[i].find('/');
                idx = pos != std::string::npos ? f[i].substr(0, pos) : f[i];
                if (!idx.empty()) faces.push_back(std::stoi(idx) - 1);
            }
        }
    }

}   

int Model::n_verices(void) {
    return vertices.size();
}

int Model::n_faces(void) {
    return faces.size() / 3;
}

vec3 Model::vertex(const int i) {
    return vertices[i];
}

vec3 Model::vertex(const int iface, const int i) {
    return vertices[faces[iface * 3 + i]];
}