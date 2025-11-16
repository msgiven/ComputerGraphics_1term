#include <vector>
#include <cmath>
#include <algorithm>
#include "tgaimage.h"
#include "model.h"
#include "geometry.h"

const TGAColor white = TGAColor(255, 255, 255, 255);
TGAColor red = TGAColor(255, 0, 0, 255);
const TGAColor green = TGAColor(0, 255, 0, 255);
Model* model = NULL;

const int WIDTH = 800;
const int HEIGHT = 800;
const int DEPTH = 255;
const Vec3f LIGHT_DIR(1, 1, 1);
const Vec3f CAMERA(0, 1, 3);
const Vec3f CENTER(0, 0, 0);
const Vec3f UP(0, 1, 0);

Matrix modelView;
Matrix projection;
Matrix viewPort;


Vec3f cross(const Vec3f& v1, const Vec3f& v2) {
    return Vec3f(v1.y * v2.z - v1.z * v2.y,
        v1.z * v2.x - v1.x * v2.z,
        v1.x * v2.y - v1.y * v2.x);
}


Matrix lookAt(Vec3f camera, Vec3f center, Vec3f up)
{
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


struct PhongShaderr {
    mat<2, 3, float> variyng_uv;
    mat<3, 3, float> varying_normals;

    Vec4f vertex(int nface, int nvert) {
        variyng_uv.set_col(nvert, model->uv(nface, nvert));

        Vec3f n = model->normal(nface, nvert).normalize();
        varying_normals.set_col(nvert, n);

        Vec4f gl_Vertex = embed<4>(model->vert(nface, nvert));
        return viewPort * projection * modelView * gl_Vertex;
    }

    bool fragment(Vec3f bar, TGAColor& color) {
        Vec3f n = (varying_normals * bar).normalize();
        float intensity = std::max(0.f, n * LIGHT_DIR);

        Vec2f uv = variyng_uv * bar;
        color = model->diffuse(uv) * intensity;

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

void barTriangle(Vec4f *abc, PhongShader &shader, TGAImage& zbuffer, TGAImage& image) {
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

                float alpha = baryc.x;
                float beta = baryc.y;
                float gamma = baryc.z;

                float z = alpha * abc[0][2] + beta * abc[1][2] + gamma * abc[2][2];
                float w = alpha * abc[0][3] + beta * abc[1][3] + gamma * abc[2][3];
                int frag_depth = std::max(0, std::min(255, int(z / w + .5)));
      
                if (alpha < 0 || beta < 0 || gamma < 0 || zbuffer.get(P.x, P.y)[0] > frag_depth) continue;
                    bool discard = shader.fragment(baryc, color);
                    if (discard) continue;

                    zbuffer.set(P.x, P.y, TGAColor(frag_depth));
                    image.set(P.x, P.y, color);

            }
        }
    }
}



int main(int argc, char** argv) {
    TGAImage image(WIDTH, HEIGHT, TGAImage::RGB);
    TGAImage zbuffer(WIDTH, HEIGHT, TGAImage::GRAYSCALE);
    if (2 == argc) {
        model = new Model(argv[1]);
    }
    else {
        model = new Model("obj/african_head.obj");
    }

   

    PhongShader shader;

    modelView = lookAt(CAMERA, CENTER, UP);
    projection = Matrix::identity();
    projection[3][2] = -1. / (CENTER - CAMERA).norm();
    viewPort = ViewPort(WIDTH / 8, HEIGHT / 8, WIDTH * 3 / 4, HEIGHT * 3 / 4);

    float intensity[3];


    for (int i = 0; i < model->nfaces(); i++) {
        Vec4f screen_coords[3];
        Vec3f world_coords[3];
        for (int j = 0; j < 3; j++) {
            screen_coords[j] = shader.vertex(i, j);
        }
        barTriangle(screen_coords, shader, zbuffer, image);
    }

    image.flip_vertically();
    image.write_tga_file("output.tga");
    delete model;
    return 0;
}



/*
void triangle(Vec3i t0, Vec3i t1, Vec3i t2, Vec2i uv0, Vec2i uv1, Vec2i uv2, TGAImage& image, float intensity, int* zbuffer)
{
    if (t0.y > t1.y) { std::swap(t0, t1); std::swap(uv0, uv1); }
    if (t0.y > t2.y) { std::swap(t0, t2); std::swap(uv0, uv2);}
    if (t1.y > t2.y) { std::swap(t1, t2); std::swap(uv1, uv2); }

    int total_height = t2.y - t0.y;

    for (int y = t0.y; y <= t1.y; y++) {
        int segment_height = t1.y - t0.y;
        if (segment_height == 0) continue;

        float alpha = (float)(y - t0.y) / total_height;
        float beta = (float)(y - t0.y) / segment_height;

        Vec3i A = Vec3f(t0) + Vec3f(t2 - t0) * alpha;
        Vec3i B = Vec3f(t0) + Vec3f(t1 - t0) * beta;

        Vec2i uvA = (uv0) + (uv2 - uv0) * alpha;
        Vec2i uvB = (uv0) + (uv1 - uv0) * beta;

        if (A.x > B.x) { std::swap(A, B); std::swap(uvA, uvB); }
        for (int x = A.x; x <= B.x; x++) {
            float phi = (B.x == A.x) ? 0.0f : (float)(x - A.x) / (B.x - A.x);
            Vec3i P = Vec3f(A) + Vec3f(B - A) * phi;
            Vec2i uvP = (uvA) + (uvB - uvA) * phi;
            int idx = P.x + P.y * WIDTH;
            if (P.x>0 && P.y>0&& idx < WIDTH * HEIGHT && zbuffer[idx] < P.z) {
                zbuffer[idx] = P.z;
                TGAColor color = model->diffuse(uvP);
                image.set(P.x, P.y, TGAColor(color.r *intensity, color.g*intensity, color.b*intensity));
            }
        }
    }

    for (int y = t1.y; y <= t2.y; y++) {
        int segment_height = t2.y - t1.y;
        if (segment_height == 0) continue;

        float alpha = (float)(y - t0.y) / total_height;
        float beta = (float)(y - t1.y) / segment_height;

        Vec3i A = Vec3f(t0) + Vec3f(t2 - t0) * alpha;
        Vec3i B = Vec3f(t1) + Vec3f(t2 - t1) * beta;

        Vec2i uvA = (uv0) + (uv2 - uv0) * alpha;
        Vec2i uvB = (uv0) + (uv2 - uv1) * beta;

        if (A.x > B.x) { std::swap(A, B); std::swap(uvA, uvB); }
        for (int x = A.x; x <= B.x; x++) {
            float phi = (B.x == A.x) ? 0.0f : (float)(x - A.x) / (B.x - A.x);
            Vec3i P = Vec3f(A) + Vec3f(B - A) * phi;
            Vec2i uvP = (uvA) + (uvB - uvA) * phi;
            int idx = P.x + P.y * WIDTH;
            if (P.x > 0 && P.y > 0 && idx < WIDTH * HEIGHT && zbuffer[idx] < P.z) {
                zbuffer[idx] = P.z;
                TGAColor color = model->diffuse(uvP);
                //image.set(P.x, P.y, TGAColor(color.r * intensity, color.g * intensity, color.b * intensity));
                image.set(P.x, P.y, color);
            }
        }
    }
}

*/