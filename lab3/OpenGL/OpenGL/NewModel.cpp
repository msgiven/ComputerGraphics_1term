#include "NewModel.h"
#include "model.h"
#include <iostream>
#include <assimp/postprocess.h>

NewModel::NewModel(const char* filename) : verts_(), faces_() {

    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(
        filename,
        aiProcess_Triangulate |
        aiProcess_JoinIdenticalVertices
    );

    if (!scene || !scene->mRootNode || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) {
        std::cerr << "ASSIMP ERROR: " << importer.GetErrorString() << std::endl;
        return;
    }

    for (unsigned int m = 0; m < scene->mNumMeshes; m++) {
        aiMesh* mesh = scene->mMeshes[m];

        size_t vert_offset = verts_.size();


        for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
            aiVector3D v = mesh->mVertices[i];
            verts_.push_back(Vec3f(v.x, v.y, v.z));
        }

        for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
            aiFace& f = mesh->mFaces[i];
            if (f.mNumIndices != 3) continue; 

            std::vector<Vec3i> face(3);
            for (int k = 0; k < 3; k++) {
                int idx = f.mIndices[k] + vert_offset;
                face[k] = Vec3i(idx, idx, idx);
            }
            faces_.push_back(face);
        }
    }

    std::cout << "Loaded: " << verts_.size()
        << " vertices, " << faces_.size()
        << " faces\n";
}

NewModel::~NewModel() {}

int NewModel::nverts() { return (int)verts_.size(); }
int NewModel::nfaces() { return (int)faces_.size(); }

Vec3f NewModel::vert(int i) { return verts_[i]; }

std::vector<int> NewModel::face(int idx) {
    std::vector<int> f;
    for (auto& v : faces_[idx])
        f.push_back(v[0]);
    return f;
}
