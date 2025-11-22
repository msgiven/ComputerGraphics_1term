#pragma once
#include <vector>
#include <string>
#include <assimp/scene.h>
#include <assimp/Importer.hpp>

#include "geometry.h"   // твои Vec3f, Vec3i

class NewModel {
public:
    NewModel(const char* filename);
    ~NewModel();

    int nverts();
    int nfaces();

    Vec3f vert(int i);
    std::vector<int> face(int idx);

private:
    std::vector<Vec3f> verts_;
    std::vector<std::vector<Vec3i>> faces_;
};