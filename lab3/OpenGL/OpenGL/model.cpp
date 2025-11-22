#include <iostream>
#include <fstream>
#include <sstream>
#include "model.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>


Model::Model(const char* filename) : verts_(), faces_(), norms_(), uv_(), diffusemap_(), normalmap_(), specularmap_() {

    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(filename,
        aiProcess_Triangulate |
        aiProcess_JoinIdenticalVertices |
        aiProcess_GenSmoothNormals |
        aiProcess_CalcTangentSpace |
        aiProcess_FlipUVs);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        std::cerr << "ERROR::ASSIMP::" << importer.GetErrorString() << std::endl;
        return;
    }


    if (scene->mRootNode->mName.length > 0) {
        object_name_ = scene->mRootNode->mName.C_Str();
        std::cout << object_name_;
    }


    for (unsigned int i = 0; i < scene->mNumMeshes; i++) {
        aiMesh* mesh = scene->mMeshes[i];


        size_t current_vert_offset = verts_.size();


        for (unsigned int j = 0; j < mesh->mNumVertices; j++) {
            aiVector3D pos = mesh->mVertices[j];
            verts_.push_back(Vec3f(pos.x, pos.y, pos.z));
        }

        if (mesh->HasNormals()) {
            for (unsigned int j = 0; j < mesh->mNumVertices; j++) {
                aiVector3D normal = mesh->mNormals[j];
                norms_.push_back(Vec3f(normal.x, normal.y, normal.z));
            }
        }


        if (mesh->HasTextureCoords(0)) {
            for (unsigned int j = 0; j < mesh->mNumVertices; j++) {
                aiVector3D uv = mesh->mTextureCoords[0][j];

                uv_.push_back(Vec2f(uv.x, uv.y));
            }
        }
        for (unsigned int j = 0; j < mesh->mNumFaces; j++) {
            aiFace face = mesh->mFaces[j];
            std::vector<Vec3i> f;
            for (unsigned int k = 0; k < face.mNumIndices; k++) {
                int global_index = face.mIndices[k] + current_vert_offset;

                Vec3i tmp;
                tmp[0] = global_index;
                tmp[1] = global_index;
                tmp[2] = global_index;

                f.push_back(tmp);
            }
            faces_.push_back(f);
        }
    }

    std::cerr << "# v# " << verts_.size() << " f# " << faces_.size() << " vt# " << uv_.size() << " vn# " << norms_.size() << std::endl;

    //load_texture(filename, "_diff.tga", diffusemap_);
    //load_texture(filename, "_ddn.tga", normalmap_);
    //load_texture(filename, ".tga", specularmap_);
}

Model::~Model() {}

int Model::nverts() {
    return (int)verts_.size();
}

int Model::nfaces() {
    return (int)faces_.size();
}

std::vector<int> Model::face(int idx) {
    std::vector<int> face;
    for (int i = 0; i < (int)faces_[idx].size(); i++) face.push_back(faces_[idx][i][0]);
    return face;
}

Vec3f Model::vert(int i) {
    return verts_[i];
}

Vec3f Model::vert(int iface, int nthvert) {
    return verts_[faces_[iface][nthvert][0]];
}

Vec2f Model::uv(int iface, int nthvert) {
    return uv_[faces_[iface][nthvert][1]];
}

Vec3f Model::normal(int iface, int nthvert) {
    int idx = faces_[iface][nthvert][2];
    return norms_[idx].normalize();
}

void Model::load_texture(std::string filename, const char* suffix, TGAImage& img) {
    std::string texfile(filename);
    size_t dot = texfile.find_last_of(".");
    if (dot != std::string::npos) {
        texfile = texfile.substr(0, dot) + std::string(suffix);
        std::cerr << "texture file " << texfile << " loading " << (img.read_tga_file(texfile.c_str()) ? "ok" : "failed") << std::endl;
        img.flip_vertically();
    }
}

TGAColor Model::diffuse(Vec2f uvf) {
    Vec2i uv(uvf[0] * diffusemap_.get_width(), uvf[1] * diffusemap_.get_height());
    return diffusemap_.get(uv[0], uv[1]);
}

Vec3f Model::normal(Vec2f uvf) {
    Vec2i uv(uvf[0] * normalmap_.get_width(), uvf[1] * normalmap_.get_height());
    TGAColor c = normalmap_.get(uv[0], uv[1]);
    Vec3f res;
    for (int i = 0; i < 3; i++)
        res[2 - i] = (float)c[i] / 255.f * 2.f - 1.f;
    return res;
}

float Model::specular(Vec2f uvf) {
    Vec2i uv(uvf[0] * specularmap_.get_width(), uvf[1] * specularmap_.get_height());
    return specularmap_.get(uv[0], uv[1])[0] / 1.f;
}