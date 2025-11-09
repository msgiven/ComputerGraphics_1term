#include <vector>
#include <cmath>
#include "tgaimage.h"
#include "model.h"
#include "geometry.h"

const TGAColor white = TGAColor(255, 255, 255, 255);
const TGAColor red = TGAColor(255, 0, 0, 255);
const TGAColor green = TGAColor(0, 255, 0, 255);
Model* model = NULL;
const int width = 800;
const int height = 800;
const int depth = 255;
Vec3f light_dir(0, 0, -1);

void line(Vec2i p0, Vec2i p1, TGAImage& image, TGAColor color) {
    bool steep = false;
    if (std::abs(p0.x - p1.x) < std::abs(p0.y - p1.y)) {
        std::swap(p0.x, p0.y);
        std::swap(p1.x, p1.y);
        steep = true;
    }
    if (p0.x > p1.x) {
        std::swap(p0, p1);
    }

    for (int x = p0.x; x <= p1.x; x++) {
        float t = (x - p0.x) / (float)(p1.x - p0.x);
        int y = p0.y * (1. - t) + p1.y * t;
        if (steep) {
            image.set(y, x, color);
        }
        else {
            image.set(x, y, color);
        }
    }
}

void Rasterize(Vec3i& A, Vec3i& B, int* zbuffer, TGAImage& image, TGAColor& color)
{
    if (A.x > B.x) std::swap(A, B);
    for (int x = A.x; x <= B.x; x++) {
        float phi = (B.x == A.x) ? 0.0f : (float)(x - A.x) / (B.x - A.x);
        Vec3i P = Vec3f(A) + Vec3f(B - A) * phi;
        int idx = P.x + P.y * width;
        if (idx < width * height && zbuffer[idx] < P.z) {
            zbuffer[idx] = P.z;
            image.set(P.x, P.y, color);
        }
    }
}

void triangle(Vec3i t0, Vec3i t1, Vec3i t2, TGAImage& image, TGAColor color, int* zbuffer) {
    if (t0.y > t1.y) std::swap(t0, t1);
    if (t0.y > t2.y) std::swap(t0, t2);
    if (t1.y > t2.y) std::swap(t1, t2);

    int total_height = t2.y - t0.y;

    // Первая часть треугольника (от t0 до t1)
    for (int y = t0.y; y <= t1.y; y++) {
        int segment_height = t1.y - t0.y;
        if (segment_height == 0) continue;

        float alpha = (float)(y - t0.y) / total_height;
        float beta = (float)(y - t0.y) / segment_height;

        Vec3i A = Vec3f(t0) + Vec3f(t2 - t0) * alpha;
        Vec3i B = Vec3f(t0) + Vec3f(t1 - t0) * beta;

        Rasterize(A, B, zbuffer, image, color);
    }

    for (int y = t1.y; y <= t2.y; y++) {
        int segment_height = t2.y - t1.y;
        if (segment_height == 0) continue;

        float alpha = (float)(y - t0.y) / total_height;
        float beta = (float)(y - t1.y) / segment_height;

        Vec3i A = Vec3f(t0) + Vec3f(t2 - t0) * alpha;
        Vec3i B = Vec3f(t1) + Vec3f(t2 - t1) * beta;

        Rasterize(A, B, zbuffer, image, color);
    }
}


int main(int argc, char** argv) {
    if (2 == argc) {
        model = new Model(argv[1]);
    }
    else {
        model = new Model("obj/african_head.obj");
    }

    int* zbuffer = new int[width * height];
    for (int i = 0; i < width * height; i++) zbuffer[i] = INT_MIN;

    TGAImage image(width, height, TGAImage::RGB);
    for (int i = 0; i < model->nfaces(); i++) {
        std::vector<int> face = model->face(i);
        Vec3i screen_coords[3];
        Vec3f world_coords[3];
        for (int j = 0; j < 3; j++) {
            Vec3f v = model->vert(face[j]);
            screen_coords[j] = Vec3i((v.x + 1.) * width / 2., (v.y + 1.) * height / 2., (v.z + 1.) * depth / 2.);
            world_coords[j] = v;
        }
        Vec3f n = (world_coords[2] - world_coords[0]) ^ (world_coords[1] - world_coords[0]);
        n.normalize();
        float intensity = n * light_dir;
        if (intensity > 0) {
            triangle(screen_coords[0], screen_coords[1], screen_coords[2], image, TGAColor(intensity * 255, intensity * 255, intensity * 255, 255), zbuffer);
        }
    }

    image.flip_vertically();
    image.write_tga_file("output.tga");
    delete model;
    return 0;
}