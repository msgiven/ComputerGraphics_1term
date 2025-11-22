#include <vector>
#include <cmath>
#include <limits>
#include <algorithm>
#include <iostream>
#include "tgaimage.h"
#include "model.h"
#include "geometry.h"

const int WIDTH = 800;
const int HEIGHT = 800;
const int DEPTH = 255;

Model* model = NULL;
Vec3f LIGHT_DIR(1, 1, 1);
Vec3f CAMERA(0, 5, 10);
Vec3f CENTER(0, 0, 0);
Vec3f UP(0, 1, 0);

Matrix modelView;
Matrix projection;
Matrix viewPort;

Vec3f cross(const Vec3f& v1, const Vec3f& v2) {
    return Vec3f(v1.y * v2.z - v1.z * v2.y,
        v1.z * v2.x - v1.x * v2.z,
        v1.x * v2.y - v1.y * v2.x);
}

Matrix lookAt(Vec3f camera, Vec3f center, Vec3f up) {
    Vec3f z = (camera - center).normalize();
    Vec3f x = cross(up, z).normalize();
    Vec3f y = cross(z, x).normalize();

    Matrix invM = Matrix::identity();
    Matrix trM = Matrix::identity();

    for (int i = 0; i < 3; i++) {
        invM[0][i] = x[i];
        invM[1][i] = y[i];
        invM[2][i] = z[i];
        trM[i][3] = -center[i];
    }
    return invM * trM;
}

Matrix ViewPort(int x, int y, int width, int height) {
    Matrix view = Matrix::identity();
    view[0][0] = width / 2;
    view[1][1] = height / 2;
    view[2][2] = DEPTH / 2;

    view[0][3] = x + width / 2;
    view[1][3] = y + height / 2;
    view[2][3] = DEPTH / 2;
    return view;
}

struct Shader {
    mat<3, 3, float> varying_normals;

    Vec4f vertex(int nface, int nvert, Model* model) {
        Vec3f n = model->normal(nface, nvert);
        varying_normals.set_col(nvert, n);

        Vec3f v = model->vert(nface, nvert);
        v = v * 0.001f;

        Vec4f gl_Vertex = embed<4>(v);
        return viewPort * projection * modelView * gl_Vertex;
    }

    bool fragment(Vec3f bar, TGAColor& color) {
        Vec3f n = (varying_normals * bar).normalize();
        float intensity = std::max(0.f, n * LIGHT_DIR.normalize());
        color = TGAColor(255 * intensity, 255 * intensity, 255 * intensity);
        return false;
    }
};

Vec3f barycentric(Vec2f A, Vec2f B, Vec2f C, Vec2f P) {
    Vec3f s[2];
    for (int i = 2; i--; ) {
        s[i][0] = C[i] - A[i];
        s[i][1] = B[i] - A[i];
        s[i][2] = A[i] - P[i];
    }
    Vec3f u = cross(s[0], s[1]);
    if (std::abs(u[2]) > 1e-2)
        return Vec3f(1.f - (u.x + u.y) / u.z, u.y / u.z, u.x / u.z);
    return Vec3f(-1, 1, 1);
}

void barTriangle(Vec4f* abc, Shader& shader, TGAImage& zbuffer, TGAImage& image) {
    Vec2f areaMin(std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
    Vec2f areaMax(-std::numeric_limits<float>::max(), -std::numeric_limits<float>::max());
    float wInv[3];
    for (int i = 0; i < 3; i++) {
        wInv[i] = 1.0 / abc[i][3];
        for (int j = 0; j < 2; j++) {
            areaMin[j] = std::min(areaMin[j], abc[i][j] * wInv[i]);
            areaMax[j] = std::max(areaMax[j], abc[i][j] * wInv[i]);
        }
    }

    Vec2i P;
    TGAColor color;
    for (P.x = areaMin.x; P.x <= areaMax.x; P.x++) {
        for (P.y = areaMin.y; P.y <= areaMax.y; P.y++) {
            Vec3f baryc = barycentric(proj<2>(abc[0] * wInv[0]), proj<2>(abc[1] * wInv[1]), proj<2>(abc[2] * wInv[2]), proj<2>(P));

            if (baryc.x >= 0 && baryc.y >= 0 && baryc.z >= 0) {
                float z = baryc.x * abc[0][2] + baryc.y * abc[1][2] + baryc.z * abc[2][2];
                float w = baryc.x * abc[0][3] + baryc.y * abc[1][3] + baryc.z * abc[2][3];
                int frag_depth = std::max(0, std::min(255, int(z / w + .5)));

                if (zbuffer.get(P.x, P.y)[0] > frag_depth) continue;

                bool discard = shader.fragment(baryc, color);
                if (!discard) {
                    zbuffer.set(P.x, P.y, TGAColor(frag_depth));
                    image.set(P.x, P.y, color);
                }
            }
        }
    }
}

int main(int argc, char** argv) {
    model = new Model("Sponza/sponza.obj");

    if (model->nfaces() == 0) {
        std::cerr << "Failed to load model" << std::endl;
        return 1;
    }

    TGAImage image(WIDTH, HEIGHT, TGAImage::RGB);
    TGAImage zbuffer(WIDTH, HEIGHT, TGAImage::GRAYSCALE);

    Shader shader;
    modelView = lookAt(CAMERA, CENTER, UP);
    projection = Matrix::identity();
    projection[3][2] = -1. / (CENTER - CAMERA).norm();
    viewPort = ViewPort(WIDTH / 8, HEIGHT / 8, WIDTH * 3 / 4, HEIGHT * 3 / 4);

    for (int i = 0; i < model->nfaces(); i++) {
        Vec4f screen_coords[3];
        for (int j = 0; j < 3; j++) {
            screen_coords[j] = shader.vertex(i, j, model);
        }
        barTriangle(screen_coords, shader, zbuffer, image);
    }

    image.flip_vertically();
    image.write_tga_file("output.tga");

    delete model;
    return 0;
}