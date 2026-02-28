#include <fstream>
#include <sstream>
#include "model.h"
#include <algorithm>

Model::Model(const std::string filename) {
    std::ifstream in;
    in.open(filename, std::ifstream::in);
    if (in.fail()) return;
    std::string line;
    while (!in.eof()) {
        std::getline(in, line);
        std::istringstream iss(line.c_str());
        char trash;
        if (!line.compare(0, 2, "v ")) {
            iss >> trash;
            vec3 v;
            for (int i : {0,1,2}) iss >> v[i];
            verts.push_back(v);
        } else if (!line.compare(0, 3, "vt ")) {
            iss >> trash >> trash;  // skip two character 'v' and 't'
            vec3 vt;
            for (int i : {0,1,2}) iss >> vt[i];
            texts.push_back(vt);
        } else if (!line.compare(0, 3, "vn ")) {
            iss >> trash >> trash;  // skip two character 'v' and 'n'
            vec3 vn;
            for (int i : {0,1,2}) iss >> vn[i];
            norms.push_back(vn);
        } else if (!line.compare(0, 2, "f ")) {
            int f,t,n, cnt = 0;
            iss >> trash;
            while (iss >> f >> trash >> t >> trash >> n) {
                facet_vrt.push_back(--f);
                facet_txt.push_back(--t);
                facet_nml.push_back(--n);
                cnt++;
            }
            if (3!=cnt) {
                std::cerr << "Error: the obj file is supposed to be triangulated" << std::endl;
                return;
            }
        }
    }
    std::cerr << "# v# " << nverts() << " vt# "  << ntexts() 
              << " vn# "  << nnorms() << " f# "  << nfaces() << std::endl;
}

int Model::nverts() const { return verts.size(); }
int Model::nfaces() const { return facet_vrt.size()/3; }
int Model::nnorms() const {return norms.size(); }
int Model::ntexts() const {return texts.size(); }

vec3 Model::vert(const int i) const {
    return verts[i];
}

vec3 Model::vert(const int iface, const int nthvert) const {
    return verts[facet_vrt[iface*3+nthvert]];
}

vec3 Model::norm(const int i) const {
    return norms[i];
}

vec3 Model::norm(const int iface, const int nthnorm) const {
    return norms[facet_nml[iface*3+nthnorm]];
}

vec3 Model::text(const int i) const {
    return texts[i];
}

vec3 Model::text(const int iface, const int nthtext) const {
    return texts[facet_txt[iface*3+nthtext]];
}