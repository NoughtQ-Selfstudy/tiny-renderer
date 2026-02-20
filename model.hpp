#include <vector>
#include "geometry.hpp"

class Model {
private:
    std::vector<vec3> vertices = {};
    std::vector<int> faces = {};
public:
    Model(const char* filename);
    int n_verices(void);
    int n_faces(void);
    vec3 vertex(const int i);
    vec3 vertex(const int iface, const int i);
};
