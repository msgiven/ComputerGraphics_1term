//#define NOMINMAX
#include "D3DApp.h"
#include <DirectXColors.h> 
#include "d3dUtil.h"
#include "MathHelper.h"
#include "UploadBuffer.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <stdexcept>
#include <string>
#include "ModelLoader.h"
#include "PostProcessing.h"
#include "PBR.h"
#include <cstdint>
#include <cstdio>
#include <limits>
#include <functional>
#include <unordered_set>
#include <random>
#include <assimp/postprocess.h>
#include <DirectXColors.h>
#include <SimpleMath.h>
using namespace DirectX::SimpleMath;

#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

using namespace DirectX;

struct Vertex
{
    XMFLOAT3 Pos;
    // XMFLOAT4 Color;
    XMFLOAT3 Normal;
    XMFLOAT2 TexC;
    XMFLOAT3 TangentU;
};

enum class RenderBucket
{
    Opaque = 0,
    Curtain = 1,
    Water = 2
};

struct OctreeItem
{
    RenderItem* Item = nullptr;
    RenderBucket Bucket = RenderBucket::Opaque;
    DirectX::BoundingBox Bounds = {};
};

struct OctreeNode
{
    DirectX::BoundingBox Bounds = {};
    std::array<std::unique_ptr<OctreeNode>, 8> Children = {};
    std::vector<OctreeItem> Items;
    bool IsLeaf = true;
};

class Meow : public D3DApp
{
public:
    Meow(HINSTANCE hInstance);
    ~Meow();

    virtual bool Initialize() override;
    virtual void OnResize() override;
    virtual void Update(const GameTimer& gt) override;
    virtual void Draw(const GameTimer& gt) override;

    virtual void OnMouseDown(WPARAM btnState, int x, int y) override;
    virtual void OnMouseUp(WPARAM btnState, int x, int y) override;
    virtual void OnMouseMove(WPARAM btnState, int x, int y) override;

    ComPtr<ID3D12Device> GetDevice();

private:
    std::unordered_map<std::string, std::unique_ptr<Texture>>& Textures() { return modelLoader->GetTextures(); }
    std::unordered_map<std::string, std::unique_ptr<Material>>& Materials() { return modelLoader->GetMaterials(); }
    std::vector<std::unique_ptr<RenderItem>>& AllRitems() { return modelLoader->GetAllRitems(); }

    void LoadModelAndTextures();
    void UpdateObjectCBs(const GameTimer& gt);
    void BuildDescriptorHeaps();
    void BuildConstantBuffers();
    void BuildMaterials();
    void UpdateMaterialCBs(const GameTimer& gt);
    void AnimateMaterials(const GameTimer& gt);
    void BuildRootSignature();
    void BuildLightRootSignature();
    void BuildPostProcRootSignature();
    void BuildShadersAndInputLayout();
    void BuildPSO();
    void BuildDisplayPSO();
    void BuildPostProcPSO();
    void UpdatePassCB(const GameTimer& gt);
    void BuildDeferredLights();
    void BuildShadowMap();
    void AddDirectionalDeferredLight(const XMFLOAT3& direction, const XMFLOAT3& strength);
    void AddPointDeferredLight(const XMFLOAT3& position, const XMFLOAT3& strength, float falloffStart, float falloffEnd);
    void AddSpotDeferredLight(const XMFLOAT3& position, const XMFLOAT3& direction, const XMFLOAT3& strength, float falloffStart, float falloffEnd, float spotPower);
    void InitBulletPool();
    void SpawnBullet();
    void UpdateBullets(float dt);
    void UpdateBulletLights();
    void BuildGlowPSO();
    void UpdateCulling();
    void BuildSceneOctree();
    std::unique_ptr<OctreeNode> BuildOctreeNode(const std::vector<OctreeItem>& items, const BoundingBox& bounds, int depth);
    void QueryOctreeVisible(const OctreeNode* node, const BoundingFrustum& frustum);
    void AppendSubtreeItems(const OctreeNode* node);
    BoundingBox ComputeItemWorldBounds(const RenderItem& item) const;
    static bool IntersectsFrustum(const BoundingFrustum& frustum, const BoundingBox& bounds);
    static std::array<BoundingBox, 8> SplitBounds(const BoundingBox& parentBounds);
    static BoundingBox ComputeEnclosingBounds(const std::vector<OctreeItem>& items);
    void BuildOctreeDebugVisualization();

    void BuildGrass();
    void BuildGrassRootSignature();

    ModelLoader* modelLoader;
    GBuffer* mgBuffer;
    PostProcessing* mPostProc;
    PBR* mPBR;

    XMFLOAT3 mSunThetaPhi = { 1.25f * XM_PI, XM_PIDIV4, 0.0f };

    float mYaw = 1.5f * XM_PI;
    float mPitch = 0.0f;

    float mCameraSpeed = 50.0f;

    XMFLOAT3 mForward = { 0.0f, 0.0f, 1.0f };
    XMFLOAT3 mRight = { 1.0f, 0.0f, 0.0f };
    XMFLOAT3 mUp = { 0.0f, 1.0f, 0.0f };

    std::array<const CD3DX12_STATIC_SAMPLER_DESC, 7> GetStaticSamplers();

    std::unique_ptr<MeshGeometry> mBoxGeo = nullptr;

    float mTheta = 1.5f * XM_PI;
    float mPhi = XM_PIDIV4;

    float mSunTheta = 1.25f * XM_PI;
    float mSunPhi = XM_PIDIV4;

    XMFLOAT3 mEyePos = { 0.0f, 0.0f, 0.0f };

    float mRadius = 105.0f;
    XMFLOAT4X4 mWorld = MathHelper::Identity4x4();
    XMFLOAT4X4 mView = MathHelper::Identity4x4();
    XMFLOAT4X4 mProj = MathHelper::Identity4x4();
    POINT mLastMousePos;
    const float mousSen = 0.25f;

    PassConstants mMainPassCB;
    std::unique_ptr<UploadBuffer<PassConstants>> mPassCB;

    
    struct DisplayFlagConstants {
        float isGGX = 1.0f;
        XMFLOAT3 padding = {};
    };
    DisplayFlagConstants dispFlagCB;
    std::unique_ptr<UploadBuffer<DisplayFlagConstants>> mDisplayFlagCB;

    struct CascadeGpuData
    {
        XMFLOAT4X4 LightViewProj = MathHelper::Identity4x4();
        XMFLOAT4X4 ShadowTran = MathHelper::Identity4x4();
        XMFLOAT4 Distances = XMFLOAT4(40.0f, 300.0f, 1000.0f, 0.0f);
        XMFLOAT4 Padding[7] = {};
    };
    struct ShadowConstants
    {
        CascadeGpuData Cascades[3];
        UINT gCascadeIndex;
        XMFLOAT3 padding_end;
    };
    ShadowConstants mShadowCBData;
    std::unique_ptr<UploadBuffer<ShadowConstants>> mShadowCB;
    ComPtr<ID3D12Resource> ShadowMap = nullptr;
    ComPtr<ID3D12Resource> IntegrationMap = nullptr;
    ComPtr<ID3D12Resource> IrradianceMap = nullptr;
    ComPtr<ID3D12Resource> PreFilteredEnvMap = nullptr;
    D3D12_VIEWPORT Viewport{};
    CD3DX12_RECT ScissorRect{};

    std::unique_ptr<UploadBuffer<MaterialConstants>> mMaterialCB;

    std::unique_ptr<UploadBuffer<ObjectConstants>> mObjectCB = nullptr;
    ComPtr<ID3D12DescriptorHeap> mSrvDescriptorHeap;

    std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayoutDesc;

    ComPtr<ID3D12RootSignature> mRootSignature;
    ComPtr<ID3D12RootSignature> mLightRootSig;
    ComPtr<ID3D12RootSignature> mPostProcRootSig;

    ComPtr<ID3DBlob> mvsByteCode = nullptr;
    ComPtr<ID3DBlob> mpsByteCode = nullptr;
    ComPtr<ID3D10Blob> mhsByteCode = nullptr;
    ComPtr<ID3D10Blob> mdsByteCode = nullptr;
    ComPtr<ID3D12PipelineState> mPSO = nullptr;

    ComPtr<ID3DBlob> mvsLightByteCode = nullptr;
    ComPtr<ID3DBlob> mvsShadowByteCode = nullptr;
    ComPtr<ID3DBlob> mpsLightByteCode = nullptr;
    ComPtr<ID3D12PipelineState> mLightPSO = nullptr;

    ComPtr<ID3DBlob> mvsPostProcByteCode = nullptr;
    ComPtr<ID3DBlob> mpsPostProcByteCode = nullptr;
    ComPtr<ID3D12PipelineState> mPostProcPSO = nullptr;

    ComPtr<ID3DBlob> mvsWaveByteCode = nullptr;
    ComPtr<ID3DBlob> mpsWaveByteCode = nullptr;
    ComPtr<ID3DBlob> mhsWaveByteCode = nullptr;
    ComPtr<ID3DBlob> mdsWaveByteCode = nullptr;
    ComPtr<ID3D12PipelineState> mWavePSO = nullptr;

    ComPtr<ID3DBlob> mvsGlowByteCode = nullptr;
    ComPtr<ID3DBlob> mpsGlowByteCode = nullptr;
    ComPtr<ID3D12PipelineState> mGlowPSO = nullptr;
    ComPtr<ID3D12PipelineState> mShadowPSO = nullptr;

    std::vector<RenderItem*> mOpaqueRitems;
    std::vector<RenderItem*> mVisibleOpaqueRitems;
    std::vector<std::string> mTextureOrder;
    std::unique_ptr<MeshGeometry> mModelSponza = nullptr;
    std::vector<RenderItem*> mCurtainRitems;
    std::vector<RenderItem*> mVisibleCurtainRitems;
    ComPtr<ID3DBlob> mdsWaterByteCode = nullptr;
    ComPtr<ID3DBlob> mdsCurtainByteCode = nullptr;
    ComPtr<ID3DBlob> mpsWaterByteCode = nullptr;
    ComPtr<ID3DBlob> mpsCurtainByteCode = nullptr;

    ComPtr<ID3D12PipelineState> mWaterPSO = nullptr;
    ComPtr<ID3D12PipelineState> mCurtainPSO = nullptr;
    std::unique_ptr<UploadBuffer<Light>> mLightSB;
    std::vector<Light> mDeferredLights;
    UINT mNumDirLights = 0;
    UINT mNumPointLights = 0;
    UINT mNumSpotLights = 0;

    std::vector<RenderItem*> mBulletRitems;
    std::unique_ptr<MeshGeometry> mBulletGeo = nullptr;

    std::unique_ptr<MeshGeometry> mQuadGeo = nullptr;
    std::vector<RenderItem*> mWaterRitems;
    std::vector<RenderItem*> mVisibleWaterRitems;
    std::unique_ptr<MeshGeometry> mOctreeDebugGeo = nullptr;
    std::vector<RenderItem*> mOctreeDebugRitems;
    std::vector<OctreeItem> mStaticOctreeItems;
    std::unique_ptr<OctreeNode> mSceneOctree;
    bool mShowOctreeDebug = false;
    int mOctreeDebugMaxDepth = 3;


    std::vector<std::unique_ptr<MeshGeometry>> modelsGeo;

    std::unique_ptr<MeshGeometry> b;

    ComPtr<ID3D12PipelineState> mOctreeDebugPSO = nullptr;

    struct BulletState
    {
        bool IsActive = false;
        float LifeLeft = 0.0f;
        XMFLOAT3 Position = { 0.0f, 0.0f, 0.0f };
        XMFLOAT3 Velocity = { 0.0f, 0.0f, 0.0f };
        RenderItem* RenderProxy = nullptr;
    };

    static constexpr UINT kMaxBullets = 1500;
    static constexpr float kBulletSpeed = 100.0f;
    static constexpr float kBulletLifeTime = 8.0f;
    static constexpr float kBulletRadius = 0.35f;
    std::array<BulletState, kMaxBullets> mBullets;
    UINT mNextBulletSlot = 0;
    bool mWasSpaceDown = false;

    std::vector<Light> mStaticLights;
    UINT mStaticDirLights = 0;
    UINT mStaticPointLights = 0;
    UINT mStaticSpotLights = 0;
    UINT mCullingFrameCounter = 0;


    struct GrassInstanceData
    {
        XMFLOAT3 Position;
        float Scale;
    };
    std::unique_ptr<UploadBuffer<GrassInstanceData>> mGrassInstanceBuffer = nullptr;
    UINT mGrassInstanceCount = 1;
    ComPtr<ID3D12RootSignature> mGrassRootSignature = nullptr;
    ComPtr<ID3D12PipelineState> mGrassPSO = nullptr;
    ComPtr<ID3DBlob> mvsGrassByteCode = nullptr;
    ComPtr<ID3DBlob> mpsGrassByteCode = nullptr;
    std::vector<GrassInstanceData> mAllGrassInstances;
    UINT mVisibleGrassCount = 0;

    ComPtr<ID3D12DescriptorHeap> mShadowDsvHeap;
    const int mapSize = 4096;

    XMMATRIX cubeOffset = XMMatrixTranslation(1.0f, 1.0f, 1.0f);
};

Meow::Meow(HINSTANCE hInstance)
    : D3DApp(hInstance)
{
}

Meow::~Meow()
{
}


bool Meow::Initialize()
{
    if (!D3DApp::Initialize())
        return false;
    //ThrowIfFailed(mDirectCmdListAlloc->Reset());
    ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

    modelLoader = new ModelLoader(md3dDevice.Get(), mCommandList.Get());

    LoadModelAndTextures();
    BuildDescriptorHeaps();
    BuildConstantBuffers();
    BuildRootSignature();
    BuildLightRootSignature();
    BuildPostProcRootSignature();
    BuildShadersAndInputLayout();
    BuildGrass();
    BuildGrassRootSignature();
    BuildPSO();
    BuildGlowPSO();

    mgBuffer = new GBuffer(md3dDevice.Get(), mClientWidth, mClientHeight);
    BuildDeferredLights();
    BuildShadowMap();
    BuildDisplayPSO();

    mPBR = new PBR(md3dDevice.Get(), mCommandList.Get(), mgBuffer);

    mPostProc = new PostProcessing(md3dDevice.Get(), mClientWidth, mClientHeight, mgBuffer);
    BuildPostProcPSO();
    ThrowIfFailed(mCommandList->Close());
    ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    FlushCommandQueue();

    return true;
}

void Meow::OnResize()
{
    D3DApp::OnResize();

    if (mClientWidth <= 0 || mClientHeight <= 0)
    {
        return;
    }
    float aspectRatio = static_cast<float>(mClientWidth) / static_cast<float>(mClientHeight);


    if (aspectRatio < 0.001f)
    {
        return;
    }
    XMMATRIX P = XMMatrixPerspectiveFovLH(0.25 * MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
    XMStoreFloat4x4(&mProj, P);

    if (mgBuffer)
    {
        mgBuffer->OnResize(md3dDevice.Get(), mClientWidth, mClientHeight);
    }
}


void Meow::BuildOctreeDebugVisualization()
{
    mOctreeDebugRitems.clear();

    if (!mSceneOctree)
    {
        return;
    }

    if (!mOctreeDebugGeo)
    {
        std::vector<Vertex> cubeVertices(8);
        const float h = 0.5f;
        const XMFLOAT3 corners[8] =
        {
            {-h, -h, -h}, { h, -h, -h}, { h,  h, -h}, {-h,  h, -h},
            {-h, -h,  h}, { h, -h,  h}, { h,  h,  h}, {-h,  h,  h}
        };

        for (size_t i = 0; i < cubeVertices.size(); ++i)
        {
            cubeVertices[i].Pos = corners[i];
            cubeVertices[i].Normal = { 0.0f, 1.0f, 0.0f };
            cubeVertices[i].TexC = { 0.5f, 0.5f };
            cubeVertices[i].TangentU = { 1.0f, 0.0f, 0.0f };
        }

        std::vector<std::uint32_t> lineIndices =
        {
            0,1, 1,2, 2,3, 3,0,
            4,5, 5,6, 6,7, 7,4,
            0,4, 1,5, 2,6, 3,7
        };

        mOctreeDebugGeo = std::make_unique<MeshGeometry>();
        mOctreeDebugGeo->Name = "OctreeDebugLines";

        const UINT vbByteSize = static_cast<UINT>(cubeVertices.size() * sizeof(Vertex));
        const UINT ibByteSize = static_cast<UINT>(lineIndices.size() * sizeof(std::uint32_t));
        mOctreeDebugGeo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(), mCommandList.Get(), cubeVertices.data(), vbByteSize, mOctreeDebugGeo->VertexBufferUploader);
        mOctreeDebugGeo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(), mCommandList.Get(), lineIndices.data(), ibByteSize, mOctreeDebugGeo->IndexBufferUploader);
        mOctreeDebugGeo->VertexByteStride = sizeof(Vertex);
        mOctreeDebugGeo->VertexBufferByteSize = vbByteSize;
        mOctreeDebugGeo->IndexFormat = DXGI_FORMAT_R32_UINT;
        mOctreeDebugGeo->IndexBufferByteSize = ibByteSize;
    }

    Material* debugMat = nullptr;
    auto matIt = Materials().find("OctreeDebug");
    if (matIt == Materials().end())
    {
        auto debugMatOwner = std::make_unique<Material>();
        debugMatOwner->name = "OctreeDebug";
        debugMatOwner->matCBIndex = static_cast<UINT>(Materials().size());
        debugMatOwner->DiffuseSrvHeapIndex = 0;
        debugMatOwner->NormalSrvHeapIndex = 0;
        debugMatOwner->HeightSrvHeapIndex = 0;
        debugMatOwner->diffuseAlbedo = { 0.2f, 3.0f, 0.45f, 1.0f };
        debugMatOwner->fresnelRO = { 0.0f, 0.0f, 0.0f };
        debugMatOwner->roughness = 0.0f;
        debugMat = debugMatOwner.get();
        Materials()[debugMatOwner->name] = std::move(debugMatOwner);
    }
    else
    {
        debugMat = matIt->second.get();
    }

    std::function<void(const OctreeNode*, int)> appendNodes = [&](const OctreeNode* node, int depth)
        {
            if (!node || depth > mOctreeDebugMaxDepth)
            {
                return;
            }

            auto debugRitem = std::make_unique<RenderItem>();
            const XMFLOAT3& c = node->Bounds.Center;
            const XMFLOAT3& e = node->Bounds.Extents;
            XMStoreFloat4x4(&debugRitem->World,
                XMMatrixScaling(e.x * 2.0f, e.y * 2.0f, e.z * 2.0f) * XMMatrixTranslation(c.x, c.y, c.z));
            debugRitem->ObjCBIndex = static_cast<UINT>(AllRitems().size());
            debugRitem->Geo = mOctreeDebugGeo.get();
            debugRitem->Mat = debugMat;
            debugRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_LINELIST;
            debugRitem->IndexCount = 24;
            debugRitem->StartIndexLocation = 0;
            debugRitem->BaseVertexLocation = 0;
            mOctreeDebugRitems.push_back(debugRitem.get());
            AllRitems().push_back(std::move(debugRitem));

            for (const auto& child : node->Children)
            {
                appendNodes(child.get(), depth + 1);
            }
        };

    appendNodes(mSceneOctree.get(), 0);
}


void Meow::Update(const GameTimer& gt)
{
    float dt = gt.DeltaTime();
    if (dt > 0.1f) dt = 0.1f;


    float x = cosf(mPitch) * cosf(mYaw);
    float y = sinf(mPitch);
    float z = cosf(mPitch) * sinf(mYaw);

    XMVECTOR forward = XMVectorSet(x, y, z, 0.0f);
    XMVECTOR worldUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    XMVECTOR right = XMVector3Cross(worldUp, forward);
    if (XMVector3Less(XMVector3LengthSq(right), XMVectorReplicate(1e-6f))) {
        right = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
    }
    right = XMVector3Normalize(right);
    XMVECTOR up = XMVector3Cross(forward, right);


    XMStoreFloat3(&mForward, forward);
    XMStoreFloat3(&mRight, right);
    XMStoreFloat3(&mUp, up);


    XMVECTOR pos = XMLoadFloat3(&mEyePos);
    pos = XMVectorSetW(pos, 1.0f);

    float speed = mCameraSpeed * dt;

    if (GetAsyncKeyState('W') & 0x8000)
        pos = XMVectorMultiplyAdd(XMVectorReplicate(speed), forward, pos);
    if (GetAsyncKeyState('S') & 0x8000)
        pos = XMVectorMultiplyAdd(XMVectorReplicate(-speed), forward, pos);

    if (GetAsyncKeyState('A') & 0x8000)
        pos = XMVectorMultiplyAdd(XMVectorReplicate(-speed), right, pos);
    if (GetAsyncKeyState('D') & 0x8000)
        pos = XMVectorMultiplyAdd(XMVectorReplicate(speed), right, pos);

    const float sunSpeed = 0.02f;
    if (GetAsyncKeyState(VK_LEFT) & 0x8000) mSunTheta -= sunSpeed;
    if (GetAsyncKeyState(VK_RIGHT) & 0x8000) mSunTheta += sunSpeed;
    if (GetAsyncKeyState(VK_UP) & 0x8000) mSunPhi += sunSpeed;
    if (GetAsyncKeyState(VK_DOWN) & 0x8000) mSunPhi -= sunSpeed;

    mSunPhi = MathHelper::Clamp(mSunPhi, 0.01f, XM_PI - 0.01f);

    const bool isSpaceDown = (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;
    if (isSpaceDown && !mWasSpaceDown)
    {
        SpawnBullet();
    }
    mWasSpaceDown = isSpaceDown;

    static bool prevFlagState = false;
    bool currFlagState = (GetAsyncKeyState('C') & 0x8000) != 0;
    if (currFlagState && !prevFlagState) {
        dispFlagCB.isGGX = -dispFlagCB.isGGX;
        mDisplayFlagCB->CopyData(0, dispFlagCB);
    }
    prevFlagState = currFlagState;

    XMStoreFloat3(&mEyePos, pos);


    XMVECTOR target = XMVectorAdd(pos, forward);
    XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
    XMStoreFloat4x4(&mView, view);

    UpdateBullets(dt);
    UpdateBulletLights();


    UpdateObjectCBs(gt);
    UpdateMaterialCBs(gt);
    UpdatePassCB(gt);
    UpdateCulling();
}
void Meow::UpdateObjectCBs(const GameTimer& gt) {
    XMMATRIX view = XMLoadFloat4x4(&mView);
    XMMATRIX proj = XMLoadFloat4x4(&mProj);

    for (auto& e : AllRitems()) {
        XMMATRIX world;
       if (e->Mat->name == "CubeMat") {         
           float t = gt.TotalTime();
           XMMATRIX dynamicOffset = XMMatrixTranslation(sinf(t * XM_2PI / 10.0f) * 5.0f,
               0.0f,
               cosf(t * XM_2PI / 10.0f) * 5.0f);
           world = XMLoadFloat4x4(&e->World) * dynamicOffset;
        }
        else {
            world = XMLoadFloat4x4(&e->World);
        }
        ObjectConstants objConst;
        XMMATRIX worldViewProj = world * view * proj;

        XMStoreFloat4x4(&objConst.WorldViewProj, XMMatrixTranspose(worldViewProj));
        XMStoreFloat4x4(&objConst.World, XMMatrixTranspose(world));

        mObjectCB->CopyData(e->ObjCBIndex, objConst);
    }
}



BoundingBox Meow::ComputeItemWorldBounds(const RenderItem& item) const
{
    BoundingBox worldBounds;

    item.LocalBounds.Transform(
        worldBounds,
        XMLoadFloat4x4(&item.World)
    );

    return worldBounds;
}

bool Meow::IntersectsFrustum(const BoundingFrustum& frustum, const BoundingBox& bounds)
{
    return frustum.Contains(bounds) != DirectX::DISJOINT;
}

std::array<BoundingBox, 8> Meow::SplitBounds(const BoundingBox& parentBounds)
{
    std::array<BoundingBox, 8> result;
    const XMFLOAT3& c = parentBounds.Center;
    const XMFLOAT3& e = parentBounds.Extents;
    const XMFLOAT3 childExtents = { e.x * 0.5f, e.y * 0.5f, e.z * 0.5f };

    int childIndex = 0;
    for (int z = 0; z < 2; ++z)
    {
        for (int y = 0; y < 2; ++y)
        {
            for (int x = 0; x < 2; ++x)
            {
                const float sx = x == 0 ? -1.0f : 1.0f;
                const float sy = y == 0 ? -1.0f : 1.0f;
                const float sz = z == 0 ? -1.0f : 1.0f;

                result[childIndex].Center =
                {
                    c.x + sx * childExtents.x,
                    c.y + sy * childExtents.y,
                    c.z + sz * childExtents.z
                };
                result[childIndex].Extents = childExtents;
                ++childIndex;
            }
        }
    }

    return result;
}

BoundingBox Meow::ComputeEnclosingBounds(const std::vector<OctreeItem>& items)
{
    XMFLOAT3 minP = {
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max()
    };
    XMFLOAT3 maxP = {
        -std::numeric_limits<float>::max(),
        -std::numeric_limits<float>::max(),
        -std::numeric_limits<float>::max()
    };

    for (const OctreeItem& item : items)
    {
        const XMFLOAT3& c = item.Bounds.Center;
        const XMFLOAT3& e = item.Bounds.Extents;

        minP.x = std::min(minP.x, c.x - e.x);
        minP.y = std::min(minP.y, c.y - e.y);
        minP.z = std::min(minP.z, c.z - e.z);

        maxP.x = std::max(maxP.x, c.x + e.x);
        maxP.y = std::max(maxP.y, c.y + e.y);
        maxP.z = std::max(maxP.z, c.z + e.z);
    }

    BoundingBox root;
    root.Center = { (minP.x + maxP.x) * 0.5f, (minP.y + maxP.y) * 0.5f, (minP.z + maxP.z) * 0.5f };
    root.Extents = { (maxP.x - minP.x) * 0.5f, (maxP.y - minP.y) * 0.5f, (maxP.z - minP.z) * 0.5f };
    return root;
}

std::unique_ptr<OctreeNode> Meow::BuildOctreeNode(const std::vector<OctreeItem>& items, const BoundingBox& bounds, int depth)
{
    static constexpr int kMaxDepth = 6;
    static constexpr int kLeafItemThreshold = 32;

    auto node = std::make_unique<OctreeNode>();
    node->Bounds = bounds;

    if (items.size() <= static_cast<size_t>(kLeafItemThreshold) || depth >= kMaxDepth)
    {
        node->Items = items;
        return node;
    }

    const auto childBounds = SplitBounds(bounds);
    std::array<std::vector<OctreeItem>, 8> childBuckets;

    for (const OctreeItem& item : items)
    {
        bool pushedToChild = false;

        for (int i = 0; i < 8; ++i)
        {
            if (childBounds[i].Contains(item.Bounds) == DirectX::CONTAINS)
            {
                childBuckets[i].push_back(item);
                pushedToChild = true;
                break;
            }
        }

        if (!pushedToChild)
        {
            node->Items.push_back(item);
        }
    }

    bool hasChildren = false;
    for (int i = 0; i < 8; ++i)
    {
        if (!childBuckets[i].empty())
        {
            node->Children[i] = BuildOctreeNode(childBuckets[i], childBounds[i], depth + 1);
            hasChildren = true;
        }
    }

    node->IsLeaf = !hasChildren;
    return node;
}

void Meow::BuildSceneOctree()
{
    mStaticOctreeItems.clear();
    mStaticOctreeItems.reserve(mOpaqueRitems.size() + mCurtainRitems.size() + mWaterRitems.size());

    auto addBucket = [this](const std::vector<RenderItem*>& src, RenderBucket bucket)
        {
            for (RenderItem* item : src)
            {
                item->WorldBounds = ComputeItemWorldBounds(*item);
                mStaticOctreeItems.push_back({ item, bucket, item->WorldBounds });
            }
        };

    addBucket(mOpaqueRitems, RenderBucket::Opaque);
    addBucket(mCurtainRitems, RenderBucket::Curtain);
    addBucket(mWaterRitems, RenderBucket::Water);

    if (mStaticOctreeItems.empty())
    {
        mSceneOctree.reset();
        return;
    }

    const BoundingBox rootBounds = ComputeEnclosingBounds(mStaticOctreeItems);
    mSceneOctree = BuildOctreeNode(mStaticOctreeItems, rootBounds, 0);
}

void Meow::AppendSubtreeItems(const OctreeNode* node)
{
    if (node == nullptr)
    {
        return;
    }

    for (const OctreeItem& item : node->Items)
    {
        switch (item.Bucket)
        {
        case RenderBucket::Opaque:
            mVisibleOpaqueRitems.push_back(item.Item);
            break;
        case RenderBucket::Curtain:
            mVisibleCurtainRitems.push_back(item.Item);
            break;
        case RenderBucket::Water:
            mVisibleWaterRitems.push_back(item.Item);
            break;
        }
    }

    for (const auto& child : node->Children)
    {
        AppendSubtreeItems(child.get());
    }
}

void Meow::QueryOctreeVisible(const OctreeNode* node, const BoundingFrustum& frustum)
{
    if (node == nullptr)
    {
        return;
    }

    const auto nodeContainment = frustum.Contains(node->Bounds);
    if (nodeContainment == DirectX::DISJOINT)
    {
        return;
    }

    if (nodeContainment == DirectX::CONTAINS)
    {
        AppendSubtreeItems(node);
        return;
    }

    for (const OctreeItem& item : node->Items)
    {
        if (IntersectsFrustum(frustum, item.Bounds))
        {
            switch (item.Bucket)
            {
            case RenderBucket::Opaque:
                mVisibleOpaqueRitems.push_back(item.Item);
                break;
            case RenderBucket::Curtain:
                mVisibleCurtainRitems.push_back(item.Item);
                break;
            case RenderBucket::Water:
                mVisibleWaterRitems.push_back(item.Item);
                break;
            }
        }
    }

    for (const auto& child : node->Children)
    {
        QueryOctreeVisible(child.get(), frustum);
    }
}

void Meow::UpdateCulling()
{
    mVisibleOpaqueRitems.clear();
    mVisibleCurtainRitems.clear();
    mVisibleWaterRitems.clear();
    mVisibleOpaqueRitems.reserve(mOpaqueRitems.size());
    mVisibleCurtainRitems.reserve(mCurtainRitems.size());
    mVisibleWaterRitems.reserve(mWaterRitems.size());
    if (!mSceneOctree)
    {
        mVisibleOpaqueRitems = mOpaqueRitems;
        mVisibleCurtainRitems = mCurtainRitems;
        mVisibleWaterRitems = mWaterRitems;
        return;
    }

    BoundingFrustum viewFrustum;
    BoundingFrustum::CreateFromMatrix(viewFrustum, XMLoadFloat4x4(&mProj));

    XMMATRIX invView = XMMatrixInverse(nullptr, XMLoadFloat4x4(&mView));
    BoundingFrustum worldFrustum;
    viewFrustum.Transform(worldFrustum, invView);


    QueryOctreeVisible(mSceneOctree.get(), worldFrustum);

    mVisibleGrassCount = 0;
    for (const auto& inst : mAllGrassInstances)
    {

        DirectX::BoundingSphere sphere(inst.Position, inst.Scale);

        if (worldFrustum.Contains(sphere) != DirectX::DISJOINT)
        {
            mGrassInstanceBuffer->CopyData(mVisibleGrassCount, inst);
            mVisibleGrassCount++;
        }
    }
}



void Meow::Draw(const GameTimer& gt)
{
    auto cmdListAlloc = mDirectCmdListAlloc.Get();
    ThrowIfFailed(cmdListAlloc->Reset());
    ThrowIfFailed(mCommandList->Reset(cmdListAlloc, mPSO.Get()));

    auto shadowToDepthWrite = CD3DX12_RESOURCE_BARRIER::Transition(
        ShadowMap.Get(),
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_STATE_DEPTH_WRITE);
    mCommandList->ResourceBarrier(1, &shadowToDepthWrite);

    mCommandList->SetPipelineState(mShadowPSO.Get());
    mCommandList->SetGraphicsRootSignature(mLightRootSig.Get());
    mCommandList->SetGraphicsRootConstantBufferView(1, mPassCB->Resource()->GetGPUVirtualAddress());
    mCommandList->RSSetViewports(1, &Viewport);
    mCommandList->RSSetScissorRects(1, &ScissorRect);
    auto shadowDsv = mShadowDsvHeap->GetCPUDescriptorHandleForHeapStart();
    const UINT shadowDsvSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

    UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
    UINT matCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(MaterialConstants));
    UINT srvSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    ID3D12DescriptorHeap* descriptorHeaps[] = { mSrvDescriptorHeap.Get() };
    mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

    constexpr UINT cascadesCount = 3;
    UINT shadowCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ShadowConstants));

    for (UINT cascadeIndex = 0; cascadeIndex < cascadesCount; ++cascadeIndex)
    {
        mShadowCBData.gCascadeIndex = cascadeIndex;
        mShadowCB->CopyData(cascadeIndex, mShadowCBData);

        D3D12_GPU_VIRTUAL_ADDRESS cbAddress = mShadowCB->Resource()->GetGPUVirtualAddress();
        cbAddress += cascadeIndex * shadowCBByteSize;

        mCommandList->SetGraphicsRootConstantBufferView(2, cbAddress);

        D3D12_CPU_DESCRIPTOR_HANDLE cascadeDsv = shadowDsv;
        cascadeDsv.ptr += static_cast<SIZE_T>(cascadeIndex) * shadowDsvSize;

        mCommandList->ClearDepthStencilView(cascadeDsv, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
        mCommandList->OMSetRenderTargets(0, nullptr, false, &cascadeDsv);

        for (auto ri : mOpaqueRitems) {
            auto vbv = ri->Geo->VertexBufferView();
            auto ibv = ri->Geo->IndexBufferView();
            mCommandList->IASetVertexBuffers(0, 1, &vbv);
            mCommandList->IASetIndexBuffer(&ibv);
            mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

            CD3DX12_GPU_DESCRIPTOR_HANDLE texHandle(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
            int texIdx = (ri->Mat->DiffuseSrvHeapIndex >= 0) ? ri->Mat->DiffuseSrvHeapIndex : 0;
            texHandle.Offset(texIdx, srvSize);

            mCommandList->SetGraphicsRootDescriptorTable(0, texHandle);

            mCommandList->SetGraphicsRootConstantBufferView(1, mPassCB->Resource()->GetGPUVirtualAddress());

            mCommandList->SetGraphicsRootConstantBufferView(3,
                mObjectCB->Resource()->GetGPUVirtualAddress() + ri->ObjCBIndex * objCBByteSize);
            mCommandList->OMSetStencilRef(ri->CastsTexturedShadow ? 1 : 0);
            mCommandList->SetGraphicsRootConstantBufferView(2, cbAddress);
            mCommandList->SetGraphicsRootConstantBufferView(4, mMaterialCB->Resource()->GetGPUVirtualAddress() + ri->Mat->matCBIndex * matCBByteSize);
            mCommandList->SetGraphicsRootConstantBufferView(5, mDisplayFlagCB->Resource()->GetGPUVirtualAddress());

            mCommandList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
        }
    }



    auto shadowToPixelSrv = CD3DX12_RESOURCE_BARRIER::Transition(
        ShadowMap.Get(),
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    mCommandList->ResourceBarrier(1, &shadowToPixelSrv);

    mCommandList->SetPipelineState(mPSO.Get());

    mgBuffer->TransitToOpaqueRenderingState(mCommandList); //////////////ggggggggggg
    const float clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
    mCommandList->ClearRenderTargetView(mgBuffer->DiffRTV, clearColor, 0, nullptr);
    mCommandList->ClearRenderTargetView(mgBuffer->NormalRTV, clearColor, 0, nullptr);
    mCommandList->ClearRenderTargetView(mgBuffer->PosRTV, clearColor, 0, nullptr);

    mCommandList->RSSetViewports(1, &mScreenViewport);
    mCommandList->RSSetScissorRects(1, &mScissorRect);

    auto barrier1 = CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    mCommandList->ResourceBarrier(1, &barrier1);

    mCommandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::LightSteelBlue, 0, nullptr);
    mCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

    auto rtv = CurrentBackBufferView();
    D3D12_CPU_DESCRIPTOR_HANDLE grtvs[] = { mgBuffer->DiffRTV, mgBuffer->NormalRTV, mgBuffer->PosRTV };
    auto dsv = DepthStencilView();
    mCommandList->OMSetRenderTargets(3, grtvs, true, &dsv);

    mCommandList->SetGraphicsRootSignature(mRootSignature.Get());


    mCommandList->SetGraphicsRootConstantBufferView(2, mPassCB->Resource()->GetGPUVirtualAddress());

    for (auto ri : mVisibleOpaqueRitems) {
        auto vbv = ri->Geo->VertexBufferView();
        auto ibv = ri->Geo->IndexBufferView();
        mCommandList->IASetVertexBuffers(0, 1, &vbv);
        mCommandList->IASetIndexBuffer(&ibv);
        mCommandList->IASetPrimitiveTopology(ri->PrimitiveType);

        //mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST);
        CD3DX12_GPU_DESCRIPTOR_HANDLE texHandle(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
        int texIdx = (ri->Mat->DiffuseSrvHeapIndex >= 0) ? ri->Mat->DiffuseSrvHeapIndex : 0;
        texHandle.Offset(texIdx, srvSize);

        mCommandList->SetGraphicsRootDescriptorTable(0, texHandle);
        mCommandList->SetGraphicsRootConstantBufferView(1, mObjectCB->Resource()->GetGPUVirtualAddress() + ri->ObjCBIndex * objCBByteSize);
        mCommandList->SetGraphicsRootConstantBufferView(3, mMaterialCB->Resource()->GetGPUVirtualAddress() + ri->Mat->matCBIndex * matCBByteSize);
        mCommandList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
    }


    mCommandList->SetPipelineState(mCurtainPSO.Get());
    for (auto ri : mVisibleCurtainRitems) {
        auto vbv = ri->Geo->VertexBufferView();
        auto ibv = ri->Geo->IndexBufferView();
        mCommandList->IASetVertexBuffers(0, 1, &vbv);
        mCommandList->IASetIndexBuffer(&ibv);
        // mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST/*ri->PrimitiveType*/);

        mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST);
        CD3DX12_GPU_DESCRIPTOR_HANDLE texHandle(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
        int texIdx = (ri->Mat->DiffuseSrvHeapIndex >= 0) ? ri->Mat->DiffuseSrvHeapIndex : 0;
        texHandle.Offset(texIdx, srvSize);

        mCommandList->SetGraphicsRootDescriptorTable(0, texHandle);
        mCommandList->SetGraphicsRootConstantBufferView(1, mObjectCB->Resource()->GetGPUVirtualAddress() + ri->ObjCBIndex * objCBByteSize);
        mCommandList->SetGraphicsRootConstantBufferView(3, mMaterialCB->Resource()->GetGPUVirtualAddress() + ri->Mat->matCBIndex * matCBByteSize);

        mCommandList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
    }

    mCommandList->SetPipelineState(mWaterPSO.Get());
    for (auto ri : mVisibleWaterRitems) {
        auto vbv = ri->Geo->VertexBufferView();
        auto ibv = ri->Geo->IndexBufferView();
        mCommandList->IASetVertexBuffers(0, 1, &vbv);
        mCommandList->IASetIndexBuffer(&ibv);
        // mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST/*ri->PrimitiveType*/);

        mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST);
        CD3DX12_GPU_DESCRIPTOR_HANDLE texHandle(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
        int texIdx = (ri->Mat->DiffuseSrvHeapIndex >= 0) ? ri->Mat->DiffuseSrvHeapIndex : 0;
        texHandle.Offset(texIdx, srvSize);

        mCommandList->SetGraphicsRootDescriptorTable(0, texHandle);
        mCommandList->SetGraphicsRootConstantBufferView(1, mObjectCB->Resource()->GetGPUVirtualAddress() + ri->ObjCBIndex * objCBByteSize);
        mCommandList->SetGraphicsRootConstantBufferView(3, mMaterialCB->Resource()->GetGPUVirtualAddress() + ri->Mat->matCBIndex * matCBByteSize);

        mCommandList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
    }

    if (mVisibleGrassCount > 0)
    {
        mCommandList->SetGraphicsRootSignature(mGrassRootSignature.Get());
        mCommandList->SetGraphicsRootConstantBufferView(1, mPassCB->Resource()->GetGPUVirtualAddress());
        mCommandList->SetPipelineState(mGrassPSO.Get());
        mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        mCommandList->SetGraphicsRootShaderResourceView(0, mGrassInstanceBuffer->Resource()->GetGPUVirtualAddress());
        mCommandList->DrawInstanced(4, mVisibleGrassCount, 0, 0);
    }

    mgBuffer->TransitToLightRenderingState(mCommandList);
    mPostProc->TransitToShaderWriteState(mCommandList);

    auto postProcRtv = mPostProc->pTexRtv;
    mCommandList->OMSetRenderTargets(1, &postProcRtv, false, nullptr);
    const float clearColorPost[] = { 0.4f, 0.58f, 0.93f, 1.0f };
    mCommandList->ClearRenderTargetView(postProcRtv, clearColorPost, 0, nullptr);


    mCommandList->SetPipelineState(mLightPSO.Get());
    mCommandList->SetGraphicsRootSignature(mLightRootSig.Get());

    ID3D12DescriptorHeap* heaps[] = { mgBuffer->mSrvDescriptorHeap.Get() };
    mCommandList->SetDescriptorHeaps(_countof(heaps), heaps);
    mCommandList->SetGraphicsRootDescriptorTable(0, mgBuffer->mSrvBaseGpuHandle);
    mCommandList->SetGraphicsRootConstantBufferView(1, mPassCB->Resource()->GetGPUVirtualAddress());
    mCommandList->SetGraphicsRootConstantBufferView(2, mShadowCB->Resource()->GetGPUVirtualAddress());
    mCommandList->SetGraphicsRootConstantBufferView(3, mObjectCB->Resource()->GetGPUVirtualAddress());
    mCommandList->SetGraphicsRootConstantBufferView(4, mMaterialCB->Resource()->GetGPUVirtualAddress());
    mCommandList->SetGraphicsRootConstantBufferView(5, mDisplayFlagCB->Resource()->GetGPUVirtualAddress());
    mCommandList->IASetVertexBuffers(0, 0, nullptr);
    mCommandList->IASetIndexBuffer(nullptr);
    mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    mCommandList->DrawInstanced(3, 1, 0, 0);


    ID3D12DescriptorHeap* modelHeaps[] = { mSrvDescriptorHeap.Get() };
    mCommandList->SetDescriptorHeaps(_countof(modelHeaps), modelHeaps);
    mCommandList->SetPipelineState(mGlowPSO.Get());
    mCommandList->SetGraphicsRootSignature(mRootSignature.Get());
    mCommandList->OMSetRenderTargets(1, &postProcRtv, false, &dsv);
    mCommandList->SetGraphicsRootConstantBufferView(2, mPassCB->Resource()->GetGPUVirtualAddress());

    for (const BulletState& bullet : mBullets)
    {
        if (!bullet.IsActive || bullet.RenderProxy == nullptr)
        {
            continue;
        }

        RenderItem* ri = bullet.RenderProxy;
        auto vbv = ri->Geo->VertexBufferView();
        auto ibv = ri->Geo->IndexBufferView();
        mCommandList->IASetVertexBuffers(0, 1, &vbv);
        mCommandList->IASetIndexBuffer(&ibv);
        mCommandList->IASetPrimitiveTopology(ri->PrimitiveType);

        CD3DX12_GPU_DESCRIPTOR_HANDLE texHandle(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
        int texIdx = (ri->Mat->DiffuseSrvHeapIndex >= 0) ? ri->Mat->DiffuseSrvHeapIndex : 0;
        texHandle.Offset(texIdx, srvSize);

        mCommandList->SetGraphicsRootDescriptorTable(0, texHandle);
        mCommandList->SetGraphicsRootConstantBufferView(1, mObjectCB->Resource()->GetGPUVirtualAddress() + ri->ObjCBIndex * objCBByteSize);
        mCommandList->SetGraphicsRootConstantBufferView(3, mMaterialCB->Resource()->GetGPUVirtualAddress() + ri->Mat->matCBIndex * matCBByteSize);

        mCommandList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
    }
    if (mShowOctreeDebug && !mOctreeDebugRitems.empty())
    {
        mCommandList->SetPipelineState(mOctreeDebugPSO.Get());
        for (RenderItem* ri : mOctreeDebugRitems)
        {
            auto vbv = ri->Geo->VertexBufferView();
            auto ibv = ri->Geo->IndexBufferView();
            mCommandList->IASetVertexBuffers(0, 1, &vbv);
            mCommandList->IASetIndexBuffer(&ibv);
            mCommandList->IASetPrimitiveTopology(ri->PrimitiveType);

            CD3DX12_GPU_DESCRIPTOR_HANDLE texHandle(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
            mCommandList->SetGraphicsRootDescriptorTable(0, texHandle);
            mCommandList->SetGraphicsRootConstantBufferView(1, mObjectCB->Resource()->GetGPUVirtualAddress() + ri->ObjCBIndex * objCBByteSize);
            mCommandList->SetGraphicsRootConstantBufferView(3, mMaterialCB->Resource()->GetGPUVirtualAddress() + ri->Mat->matCBIndex * matCBByteSize);

            mCommandList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
        }
    }


    //Post process
    mPostProc->TransitToShaderReadState(mCommandList);

    auto backBufferRtv = CurrentBackBufferView();
    mCommandList->OMSetRenderTargets(1, &backBufferRtv, false, nullptr);
    mCommandList->ClearRenderTargetView(backBufferRtv, Colors::LightSteelBlue, 0, nullptr);

    mCommandList->SetPipelineState(mPostProcPSO.Get());
    mCommandList->SetGraphicsRootSignature(mPostProcRootSig.Get());

    ID3D12DescriptorHeap* ppHeaps[] = { mPostProc->pSrvDescriptorHeap.Get() };

    mCommandList->SetDescriptorHeaps(1, ppHeaps);

    mCommandList->SetGraphicsRootDescriptorTable(0, mPostProc->pSrvBaseGpuHandle);


    mCommandList->IASetVertexBuffers(0, 0, nullptr);
    mCommandList->IASetIndexBuffer(nullptr);
    mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    mCommandList->DrawInstanced(3, 1, 0, 0);

    auto barrier2 = CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    mCommandList->ResourceBarrier(1, &barrier2);

    ThrowIfFailed(mCommandList->Close());
    ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    ThrowIfFailed(mSwapChain->Present(0, 0));
    mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;
    FlushCommandQueue();
}


void Meow::AnimateMaterials(const GameTimer& gt)
{
    static float angle = 0.0f;
    angle += 0.001f;
    if (angle >= XM_2PI) angle -= XM_2PI;

    auto seaMat = Materials()["Model"].get();
    XMMATRIX S = XMMatrixScaling(12.0f, 12.0f, 1.0f);
    XMMATRIX T1 = XMMatrixTranslation(-0.5f, -0.5f, 0.0f);
    XMMATRIX R = XMMatrixRotationZ(angle);
    XMMATRIX T2 = XMMatrixTranslation(0.5f, 0.5f, 0.0f);

    XMMATRIX result = T1 * R * T2 * S;
    XMStoreFloat4x4(&seaMat->matTransform, result);
}

void Meow::OnMouseDown(WPARAM btnState, int x, int y) {
    mLastMousePos.x = x;
    mLastMousePos.y = y;

    SetCapture(mhMainWnd);
}

void Meow::OnMouseMove(WPARAM btnState, int x, int y) {
    if ((btnState & MK_LBUTTON) != 0) {
        float dx = XMConvertToRadians(mousSen * static_cast<float>(x - mLastMousePos.x));
        float dy = XMConvertToRadians(mousSen * static_cast<float>(y - mLastMousePos.y));

        mYaw += dx;
        mPitch -= dy;

        mPitch = MathHelper::Clamp(mPitch, -XM_PIDIV2 + 0.1f, XM_PIDIV2 - 0.1f);
    }

    else if ((btnState & MK_RBUTTON) != 0) {
        float dy = 0.1f * static_cast<float>(y - mLastMousePos.y);
        mCameraSpeed -= dy;
        mCameraSpeed = MathHelper::Clamp(mCameraSpeed, 5.0f, 500.0f);
    }

    mLastMousePos.x = x;
    mLastMousePos.y = y;
}


void Meow::OnMouseUp(WPARAM btnState, int x, int y) {
    ReleaseCapture();
}

void Meow::BuildRootSignature() {

    CD3DX12_ROOT_PARAMETER slotRootParametr[4] = {};

    auto sampl = GetStaticSamplers();

    CD3DX12_DESCRIPTOR_RANGE texTable;
    texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3, 0, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND);

    slotRootParametr[0].InitAsDescriptorTable(1, &texTable, D3D12_SHADER_VISIBILITY_ALL);
    slotRootParametr[1].InitAsConstantBufferView(0);
    slotRootParametr[2].InitAsConstantBufferView(1);
    slotRootParametr[3].InitAsConstantBufferView(2);

    CD3DX12_ROOT_SIGNATURE_DESC rootSigDesk(4, slotRootParametr, sampl.size(), sampl.data(), D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
    ComPtr<ID3D10Blob> serializedRootSig = nullptr;
    ComPtr<ID3D10Blob> errorRootSig = nullptr;

    HRESULT hr = D3D12SerializeRootSignature(&rootSigDesk, D3D_ROOT_SIGNATURE_VERSION_1, serializedRootSig.GetAddressOf(), errorRootSig.GetAddressOf());

    if (errorRootSig != nullptr)
    {
        ::OutputDebugStringA((char*)errorRootSig->GetBufferPointer());
    }

    ThrowIfFailed(hr);
    ThrowIfFailed(md3dDevice->CreateRootSignature(0, serializedRootSig->GetBufferPointer(), serializedRootSig->GetBufferSize(), IID_PPV_ARGS(&mRootSignature)));

}

void Meow::BuildLightRootSignature()
{
    CD3DX12_DESCRIPTOR_RANGE texTable;
    texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 10, 0);

    CD3DX12_ROOT_PARAMETER slotRootParameter[6];
    slotRootParameter[0].InitAsDescriptorTable(1, &texTable, D3D12_SHADER_VISIBILITY_PIXEL);
    slotRootParameter[1].InitAsConstantBufferView(0);
    slotRootParameter[2].InitAsConstantBufferView(1);
    slotRootParameter[3].InitAsConstantBufferView(2);
    slotRootParameter[4].InitAsConstantBufferView(3);
    slotRootParameter[5].InitAsConstantBufferView(4);

    auto staticSamplers = GetStaticSamplers();

    CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(6, slotRootParameter,
        (UINT)staticSamplers.size(), staticSamplers.data(),
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    ComPtr<ID3DBlob> serializedRootSig = nullptr;
    ComPtr<ID3DBlob> errorBlob = nullptr;

    HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &serializedRootSig, &errorBlob);
    if (errorBlob != nullptr)
    {
        ::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
    }
    ThrowIfFailed(hr);

    ThrowIfFailed(md3dDevice->CreateRootSignature(0, serializedRootSig->GetBufferPointer(),
        serializedRootSig->GetBufferSize(), IID_PPV_ARGS(&mLightRootSig)));
}


void Meow::BuildPostProcRootSignature() {
    const int paramsAmount = 1;
    CD3DX12_DESCRIPTOR_RANGE texTable;
    texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0);

    CD3DX12_ROOT_PARAMETER slotRootParameter[paramsAmount];

    slotRootParameter[0].InitAsDescriptorTable(1, &texTable, D3D12_SHADER_VISIBILITY_PIXEL);

    auto staticSamplers = GetStaticSamplers();

    CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(paramsAmount, slotRootParameter,
        (UINT)staticSamplers.size(), staticSamplers.data(),
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    ComPtr<ID3DBlob> serializedRootSig = nullptr;
    ComPtr<ID3DBlob> errorBlob = nullptr;

    HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &serializedRootSig, &errorBlob);
    if (errorBlob != nullptr)
    {
        ::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
    }
    ThrowIfFailed(hr);

    ThrowIfFailed(md3dDevice->CreateRootSignature(0, serializedRootSig->GetBufferPointer(),
        serializedRootSig->GetBufferSize(), IID_PPV_ARGS(&mPostProcRootSig)));
}

void Meow::BuildDescriptorHeaps()
{
    UINT numDescriptors = static_cast<UINT>(Materials().size()) * 3;
    if (numDescriptors == 0) numDescriptors = 1;

    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
    srvHeapDesc.NumDescriptors = numDescriptors;
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mSrvDescriptorHeap)));

    CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
    UINT srvSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    for (const auto& matEntry : Materials()) {
        const Material* mat = matEntry.second.get();
        if (mat->DiffuseSrvHeapIndex < 0 || mat->NormalSrvHeapIndex < 0 || mat->HeightSrvHeapIndex < 0) {
            continue;
        }

        auto createSrvAtIndex = [&](const std::string& texName, int descriptorIndex) {
            auto texIt = Textures().find(texName);

            CD3DX12_CPU_DESCRIPTOR_HANDLE dst(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
            dst.Offset(descriptorIndex, srvSize);

            if (texIt == Textures().end() || !texIt->second || !texIt->second->resource) {
                texIt = Textures().find("background.dds");

                if (texIt == Textures().end() || !texIt->second || !texIt->second->resource) {
                    D3D12_SHADER_RESOURCE_VIEW_DESC nullSrvDesc = {};
                    nullSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
                    nullSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                    nullSrvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                    nullSrvDesc.Texture2D.MostDetailedMip = 0;
                    nullSrvDesc.Texture2D.MipLevels = 1;
                    nullSrvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

                    md3dDevice->CreateShaderResourceView(nullptr, &nullSrvDesc, dst);
                    return;
                }
            }

            auto resource = texIt->second->resource;
            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.Format = resource->GetDesc().Format;
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Texture2D.MostDetailedMip = 0;
            srvDesc.Texture2D.MipLevels = resource->GetDesc().MipLevels;

            md3dDevice->CreateShaderResourceView(resource.Get(), &srvDesc, dst);
        };

        createSrvAtIndex(mat->DiffuseMapName, mat->DiffuseSrvHeapIndex);
        createSrvAtIndex(mat->NormalMapName, mat->NormalSrvHeapIndex);
        createSrvAtIndex(mat->HeightMapName, mat->HeightSrvHeapIndex);
    }

}

void Meow::BuildConstantBuffers() {
    UINT objCount = (UINT)AllRitems().size();
    UINT matCount = (UINT)Materials().size();

    if (objCount == 0) objCount = 1;
    if (matCount == 0) matCount = 1;

    mObjectCB = std::make_unique<UploadBuffer<ObjectConstants>>(md3dDevice.Get(), objCount, true);
    mMaterialCB = std::make_unique<UploadBuffer<MaterialConstants>>(md3dDevice.Get(), matCount, true);
    mPassCB = std::make_unique<UploadBuffer<PassConstants>>(md3dDevice.Get(), 1, true);
    mDisplayFlagCB = std::make_unique<UploadBuffer<DisplayFlagConstants>>(md3dDevice.Get(), 1, true);
    //mShadowCB = std::make_unique<UploadBuffer<ShadowConstants>>(md3dDevice.Get(), 1, true);
}

void Meow::BuildMaterials() {
    auto mat = std::make_unique<Material>();
    mat->name = "Model";
    mat->matCBIndex = 0;
    mat->diffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
    mat->fresnelRO = { 0.1f, 0.1f, 0.1f };
    mat->roughness = 0.125f;
    mat->DiffuseSrvHeapIndex = 0;
    mat->NormalSrvHeapIndex = 0;
    mat->HeightSrvHeapIndex = 0;
    Materials()["Model"] = std::move(mat);

}

void Meow::UpdateMaterialCBs(const GameTimer& gt) {

    for (auto& e : Materials()) {
        Material* mat = e.second.get();
        MaterialConstants matConst;
        XMMATRIX matTransform = XMLoadFloat4x4(&mat->matTransform);
        matConst.DiffuseAlbedo = mat->diffuseAlbedo;
        matConst.FresnelR0 = mat->fresnelRO;
        matConst.Roughness = mat->roughness;
        matConst.DispScale = mat->dispScale;
        XMStoreFloat4x4(&matConst.MatTransform, XMMatrixTranspose(matTransform));

        mMaterialCB->CopyData(mat->matCBIndex, matConst);
    }
}

void Meow::AddDirectionalDeferredLight(const XMFLOAT3& direction, const XMFLOAT3& strength)
{
    Light light;
    light.Direction = direction;
    light.Strength = strength;
    mDeferredLights.push_back(light);
    ++mNumDirLights;
}

void Meow::AddPointDeferredLight(const XMFLOAT3& position, const XMFLOAT3& strength, float falloffStart, float falloffEnd)
{
    Light light;
    light.Position = position;
    light.Strength = strength;
    light.FalloffStart = falloffStart;
    light.FalloffEnd = falloffEnd;
    mDeferredLights.push_back(light);
    ++mNumPointLights;
}

void Meow::AddSpotDeferredLight(const XMFLOAT3& position, const XMFLOAT3& direction, const XMFLOAT3& strength, float falloffStart, float falloffEnd, float spotPower)
{
    Light light;
    light.Position = position;
    light.Direction = direction;
    light.Strength = strength;
    light.FalloffStart = falloffStart;
    light.FalloffEnd = falloffEnd;
    light.SpotPower = spotPower;
    mDeferredLights.push_back(light);
    ++mNumSpotLights;
}

void Meow::BuildDeferredLights()
{
    UINT kPointLights = 0;
    mStaticLights.clear();
    mDeferredLights.clear();
    mNumDirLights = 0;
    mNumPointLights = 0;
    mNumSpotLights = 0;
    mStaticLights.reserve(kPointLights + 2);
    mDeferredLights.reserve(kPointLights + 2 + kMaxBullets);

    XMVECTOR lightDir = -MathHelper::SphericalToCartesian(1.0f, mSunTheta, mSunPhi);
    XMFLOAT3 dirNorm;
    XMStoreFloat3(&dirNorm, XMVector3Normalize(lightDir));
    AddDirectionalDeferredLight(XMFLOAT3(0.0f, -1.0f, 0.0f), XMFLOAT3(0.45f, 0.45f, 0.40f));

    for (UINT i = 0; i < kPointLights; ++i)
    {
        const float x = (i % 60) * 16.0f;
        const float z = (i / 60) * 12.0f;
        AddPointDeferredLight(XMFLOAT3(x, 8.0f, z), XMFLOAT3(0.01f, 0.08f, 0.06f), 20.0f, 100.0f);
    }

    XMVECTOR spotDir = XMVector3Normalize(XMVectorSet(0.7f, -1.0f, -0.7f, 0.0f));
    XMFLOAT3 spotDirNorm;
    XMStoreFloat3(&spotDirNorm, spotDir);
    //AddSpotDeferredLight(XMFLOAT3(0.0f, 100.0f, 10.0f), spotDirNorm, XMFLOAT3(10.8f, 10.5f, 111.0f), 500.0f, 10000.0f, 200.0f);

    mStaticLights = mDeferredLights;
    mStaticDirLights = mNumDirLights;
    mStaticPointLights = mNumPointLights;
    mStaticSpotLights = mNumSpotLights;

    mLightSB = std::make_unique<UploadBuffer<Light>>(md3dDevice.Get(), static_cast<UINT>(mStaticLights.size()) + kMaxBullets, false);
    for (UINT i = 0; i < mStaticLights.size(); ++i)
    {
        mLightSB->CopyData(i, mStaticLights[i]);
    }

    mgBuffer->UpdateLightSrv(md3dDevice.Get(), mLightSB->Resource(), static_cast<UINT>(mStaticLights.size()) + kMaxBullets, sizeof(Light));
}

void Meow::BuildShadowMap()
{
    const int cascadesCount = 3;
    auto device = md3dDevice.Get();

    auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    auto resDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32G8X24_TYPELESS, mapSize, mapSize, cascadesCount, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

    D3D12_CLEAR_VALUE optCV;
    optCV.Format = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
    optCV.DepthStencil.Depth = 1.0f;
    optCV.DepthStencil.Stencil = 0;

    ThrowIfFailed(device->CreateCommittedResource(
        &heapProperties, D3D12_HEAP_FLAG_NONE,
        &resDesc, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        &optCV, IID_PPV_ARGS(&ShadowMap)));

    auto shadowSrvHandle = mgBuffer->mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
    shadowSrvHandle.ptr += 4 * device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
    srvDesc.Texture2DArray.MostDetailedMip = 0;
    srvDesc.Texture2DArray.MipLevels = 1;
    srvDesc.Texture2DArray.ResourceMinLODClamp = 0.0f;
    srvDesc.Texture2DArray.FirstArraySlice = 0;
    srvDesc.Texture2DArray.ArraySize = cascadesCount;
    srvDesc.Texture2DArray.PlaneSlice = 0;

    device->CreateShaderResourceView(ShadowMap.Get(), &srvDesc, shadowSrvHandle);

    auto shadowStencilSrvHandle = mgBuffer->mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
    shadowStencilSrvHandle.ptr += 5 * device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    D3D12_SHADER_RESOURCE_VIEW_DESC shadowStencilSrvDesc = {};
    shadowStencilSrvDesc.Format = DXGI_FORMAT_X32_TYPELESS_G8X24_UINT;
    shadowStencilSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    shadowStencilSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
    shadowStencilSrvDesc.Texture2DArray.MostDetailedMip = 0;
    shadowStencilSrvDesc.Texture2DArray.MipLevels = 1;
    shadowStencilSrvDesc.Texture2DArray.ResourceMinLODClamp = 0.0f;
    shadowStencilSrvDesc.Texture2DArray.FirstArraySlice = 0;
    shadowStencilSrvDesc.Texture2DArray.ArraySize = cascadesCount;
    shadowStencilSrvDesc.Texture2DArray.PlaneSlice = 1;
    device->CreateShaderResourceView(ShadowMap.Get(), &shadowStencilSrvDesc, shadowStencilSrvHandle);

    auto customShadowSrvHandle = mgBuffer->mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
    customShadowSrvHandle.ptr += 6 * device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    D3D12_SHADER_RESOURCE_VIEW_DESC customShadowSrvDesc = {};
    customShadowSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    customShadowSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    customShadowSrvDesc.Texture2D.MostDetailedMip = 0;
    customShadowSrvDesc.Texture2D.MipLevels = 1;
    customShadowSrvDesc.Texture2D.PlaneSlice = 0;
    customShadowSrvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
    auto customShadowIt = Textures().find("Max_logo_2025.dds");
    if (customShadowIt != Textures().end())
    {
        auto resource = customShadowIt->second->resource;
        customShadowSrvDesc.Format = resource->GetDesc().Format;;
        device->CreateShaderResourceView(resource.Get(), &customShadowSrvDesc, customShadowSrvHandle);
    }
    else
    {
        device->CreateShaderResourceView(nullptr, &customShadowSrvDesc, customShadowSrvHandle);
        OutputDebugStringA("WARNING: background.dds not found for shadow map!\n");
    }

    Viewport = D3D12_VIEWPORT{ 0, 0, static_cast<float>(mapSize), static_cast<float>(mapSize), 0.0f, 1.0f };
    ScissorRect = CD3DX12_RECT(0, 0, LONG_MAX, LONG_MAX);

    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
    dsvHeapDesc.NumDescriptors = cascadesCount;
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    ThrowIfFailed(device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&mShadowDsvHeap)));


    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
    dsvDesc.Format = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
    dsvDesc.Texture2DArray.ArraySize = 1;
    dsvDesc.Texture2DArray.MipSlice = 0;

    auto shadowDsvHandle = mShadowDsvHeap->GetCPUDescriptorHandleForHeapStart();
    const UINT dsvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    for (UINT cascadeIndex = 0; cascadeIndex < cascadesCount; ++cascadeIndex)
    {
        dsvDesc.Texture2DArray.FirstArraySlice = cascadeIndex;
        device->CreateDepthStencilView(ShadowMap.Get(), &dsvDesc, shadowDsvHandle);
        shadowDsvHandle.ptr += dsvDescriptorSize;
    }

    mShadowCB = std::make_unique<UploadBuffer<ShadowConstants>>(device, 3, true);
}


std::vector<Vector4> GetFrustumCornersWorldSpace(const Matrix& view, const Matrix& proj) {
    const auto viewProj = view * proj;
    const auto inv = viewProj.Invert();

    std::vector<Vector4> frustumCorners;
    frustumCorners.reserve(8);
    for (unsigned int x = 0; x < 2; ++x) {
        for (unsigned int y = 0; y < 2; ++y) {
            for (unsigned int z = 0; z < 2; ++z) {
                const Vector4 pt =
                    Vector4::Transform(Vector4(
                        2.0f * x - 1.0f,
                        2.0f * y - 1.0f,
                        z,
                        1.0f), inv);
                frustumCorners.push_back(pt / pt.w);
            }
        }
    }
    return frustumCorners;
}

void Meow::UpdatePassCB(const GameTimer& gt)
{
    XMMATRIX view = XMLoadFloat4x4(&mView);
    XMMATRIX proj = XMLoadFloat4x4(&mProj);

    XMMATRIX viewProj = XMMatrixMultiply(view, proj);
    XMMATRIX invView = XMMatrixInverse(nullptr, view);
    XMMATRIX invProj = XMMatrixInverse(nullptr, proj);
    XMMATRIX invViewProj = XMMatrixInverse(nullptr, viewProj);


    XMStoreFloat4x4(&mMainPassCB.View, XMMatrixTranspose(view));
    XMStoreFloat4x4(&mMainPassCB.InvView, XMMatrixTranspose(invView));
    XMStoreFloat4x4(&mMainPassCB.Proj, XMMatrixTranspose(proj));
    XMStoreFloat4x4(&mMainPassCB.InvProj, XMMatrixTranspose(invProj));
    XMStoreFloat4x4(&mMainPassCB.ViewProj, XMMatrixTranspose(viewProj));
    XMStoreFloat4x4(&mMainPassCB.InvViewProj, XMMatrixTranspose(invViewProj));
    mMainPassCB.EyePosW = mEyePos;
    mMainPassCB.RenderTargetSize = XMFLOAT2((float)mClientWidth, (float)mClientHeight);
    mMainPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / mClientWidth, 1.0f / mClientHeight);
    mMainPassCB.NearZ = 1.0f;
    mMainPassCB.FarZ = 1000.0f;
    mMainPassCB.TotalTime = gt.TotalTime();
    mMainPassCB.DeltaTime = gt.DeltaTime();
    mMainPassCB.AmbientLight = { 0.25f, 0.25f, 0.35f, 1.0f };


    mMainPassCB.NumDirLights = mNumDirLights;
    mMainPassCB.NumPointLights = mNumPointLights;
    mMainPassCB.NumSpotLights = mNumSpotLights;
    mMainPassCB.NumLightsTotal = static_cast<UINT>(mDeferredLights.size());

    XMVECTOR lightDir = -MathHelper::SphericalToCartesian(1.0f, mSunTheta, mSunPhi);
    XMStoreFloat3(&mDeferredLights[0].Direction, XMVector3Normalize(lightDir));
    if (mLightSB)
    {
        mLightSB->CopyData(0, mDeferredLights[0]);
    }

    auto currPassCB = mPassCB.get();
    currPassCB->CopyData(0, mMainPassCB);

    Matrix viewSm(view);
    Matrix projSm(proj);
    auto corners = GetFrustumCornersWorldSpace(viewSm, projSm);

    Vector3 center = Vector3::Zero;
    float cascadeDistances[4] = { 1.0f, 300.0f, 1000.0f, 10000.0f };
    Vector3 inLightDirection = Vector3(mDeferredLights[0].Direction.x, mDeferredLights[0].Direction.y, mDeferredLights[0].Direction.z);

    for (int i = 0; i < 3; ++i)
    {

        Matrix projSm = XMMatrixPerspectiveFovLH(0.25f * MathHelper::Pi, AspectRatio(), cascadeDistances[i], cascadeDistances[i + 1]);
        Matrix viewSm(view);
        auto corners = GetFrustumCornersWorldSpace(viewSm, projSm);

        Vector3 center = Vector3::Zero;
        for (const auto& v : corners) {
            center += Vector3(v.x, v.y, v.z);
        }
        center /= (float)corners.size();

        const auto lightView = Matrix::CreateLookAt(center, center + inLightDirection, Vector3::Up);

        float minX = std::numeric_limits<float>::max();
        float maxX = std::numeric_limits<float>::lowest();
        float minY = std::numeric_limits<float>::max();
        float maxY = std::numeric_limits<float>::lowest();
        float minZ = std::numeric_limits<float>::max();
        float maxZ = std::numeric_limits<float>::lowest();
        for (const auto& v : corners) {
            const auto trf = Vector4::Transform(v, lightView);

            minX = std::min(minX, trf.x);
            maxX = std::max(maxX, trf.x);
            minY = std::min(minY, trf.y);
            maxY = std::max(maxY, trf.y);
            minZ = std::min(minZ, trf.z);
            maxZ = std::max(maxZ, trf.z);
        }

        constexpr float zMult = 10.0f;
        minZ = (minZ < 0) ? minZ * zMult : minZ / zMult;
        maxZ = (maxZ < 0) ? maxZ / zMult : maxZ * zMult;

        auto lightProjection = Matrix::CreateOrthographicOffCenter(minX, maxX, minY, maxY, minZ, maxZ);
        Matrix lightViewProj = lightView * lightProjection;
        Matrix T(0.5f, 0.0f, 0.0f, 0.0f,
            0.0f, -0.5f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.5f, 0.5f, 0.0f, 1.0f);
        Matrix shadowTran = lightViewProj * T;

        XMStoreFloat4x4(&mShadowCBData.Cascades[i].LightViewProj, XMMatrixTranspose(XMLoadFloat4x4(reinterpret_cast<XMFLOAT4X4*>(&lightViewProj))));
        XMStoreFloat4x4(&mShadowCBData.Cascades[i].ShadowTran, XMMatrixTranspose(XMLoadFloat4x4(reinterpret_cast<XMFLOAT4X4*>(&shadowTran))));
    }
    mShadowCBData.Cascades[0].Distances = XMFLOAT4(cascadeDistances[1], cascadeDistances[2], cascadeDistances[3], 0.0f);
    mShadowCB->CopyData(0, mShadowCBData);
}


void Meow::BuildShadersAndInputLayout()
{
    std::wstring shaderPath = L"color.hlsl";
    std::wstring shaderDisplayPath = L"Display.hlsl";
    std::wstring shaderPostProcPath = L"PostProcess.hlsl";
    std::wstring waveShaderPath = L"wave.hlsl";
    std::wstring glowShaderPath = L"glow.hlsl";

    try {

        mvsByteCode = d3dUtil::CompileShader(shaderPath, nullptr, "VS", "vs_5_0");
        mpsByteCode = d3dUtil::CompileShader(shaderPath, nullptr, "PS", "ps_5_0");
        mhsByteCode = d3dUtil::CompileShader(shaderPath, nullptr, "HS", "hs_5_0");
        mdsByteCode = d3dUtil::CompileShader(shaderPath, nullptr, "DS", "ds_5_0");

        mvsLightByteCode = d3dUtil::CompileShader(shaderDisplayPath, nullptr, "VS", "vs_5_0");
        mvsShadowByteCode = d3dUtil::CompileShader(shaderDisplayPath, nullptr, "VS_Shadow", "vs_5_0");
        mpsLightByteCode = d3dUtil::CompileShader(shaderDisplayPath, nullptr, "PS", "ps_5_0");

        mvsPostProcByteCode = d3dUtil::CompileShader(shaderPostProcPath, nullptr, "VS", "vs_5_0");
        mpsPostProcByteCode = d3dUtil::CompileShader(shaderPostProcPath, nullptr, "PS", "ps_5_0");

        mvsWaveByteCode = d3dUtil::CompileShader(waveShaderPath, nullptr, "VS", "vs_5_0");
        mhsWaveByteCode = d3dUtil::CompileShader(waveShaderPath, nullptr, "HS", "hs_5_0");

        mdsWaterByteCode = d3dUtil::CompileShader(waveShaderPath, nullptr, "DS_Water", "ds_5_0");
        mdsCurtainByteCode = d3dUtil::CompileShader(waveShaderPath, nullptr, "DS_Curtain", "ds_5_0");
        mpsWaterByteCode = d3dUtil::CompileShader(waveShaderPath, nullptr, "PS_Water", "ps_5_0");
        mpsCurtainByteCode = d3dUtil::CompileShader(waveShaderPath, nullptr, "PS_Curtain", "ps_5_0");

        mvsGlowByteCode = d3dUtil::CompileShader(glowShaderPath, nullptr, "VS", "vs_5_0");
        mpsGlowByteCode = d3dUtil::CompileShader(glowShaderPath, nullptr, "PS", "ps_5_0");

        mvsGrassByteCode = d3dUtil::CompileShader(L"grass.hlsl", nullptr, "VS", "vs_5_0");
        mpsGrassByteCode = d3dUtil::CompileShader(L"grass.hlsl", nullptr, "PS", "ps_5_0");
    }

    catch (const std::exception& e) {
        throw std::runtime_error(std::string("Shader compilation failed: ") + e.what());
    }

    mInputLayoutDesc = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
    };
}


void Meow::LoadModelAndTextures()
{
    ModelInfo sponzaInfo;
    sponzaInfo.baseDir = "Sponza\\";
    sponzaInfo.fileName = "sponza.obj";
    sponzaInfo.geoName = "SponzaGeo";
    XMMATRIX sponzaScale = XMMatrixScaling(0.1f, 0.1f, 0.1f);
    XMMATRIX sponzaOffset = XMMatrixTranslation(1.0f, 1.0f, 1.0f);
    XMStoreFloat4x4(&sponzaInfo.world, sponzaScale * sponzaOffset);
    modelsGeo.push_back(modelLoader->LoadModel(sponzaInfo));


    ModelInfo oldStoneInfo;
    oldStoneInfo.baseDir = "black-swan\\";
    oldStoneInfo.fileName = "black swan.fbx";
    oldStoneInfo.geoName = "OldStoneGeo";
    oldStoneInfo.matNamePrefix = "old_stone_";
    oldStoneInfo.diffuseTexName = "textures\\bswan_col.dds";
    oldStoneInfo.normalTexName = "textures\\bswan_nor.dds";
    oldStoneInfo.heightTexName = "textures\\bswan_displacement.dds";
    XMMATRIX oldStoneScale = XMMatrixScaling(10.5f, 10.5f, 10.5f);
    XMMATRIX oldStoneRot = XMMatrixRotationRollPitchYaw(
        XMConvertToRadians(-90.0f),
        XMConvertToRadians(90.0f),
        XMConvertToRadians(0.0f));
    XMMATRIX oldStoneOffset = XMMatrixTranslation(200.0f, 115.0f, -100.0f);
    XMStoreFloat4x4(&oldStoneInfo.world, oldStoneScale * oldStoneRot * oldStoneOffset);
    modelsGeo.push_back(modelLoader->LoadModelWithSpecificTextures(oldStoneInfo));

    ModelInfo cubeInfo;
    cubeInfo.name = "cube";
    cubeInfo.baseDir = "sponza\\";
    cubeInfo.diffuseTexName = "Max_logo_2025.dds";
    cubeInfo.normalTexName = "background_ddn.dds";
    cubeInfo.heightTexName = "background.dds";
    cubeInfo.dispScale = 0.0f;
    XMMATRIX cubeScale = XMMatrixScaling(10.0f, 10.0f, 10.0f);
    XMMATRIX cubeOffset = XMMatrixTranslation(20.0f, 35.0f, -10.0f);
    XMStoreFloat4x4(&cubeInfo.world, cubeScale * cubeOffset);
    modelsGeo.push_back(modelLoader->LoadCube(cubeInfo));

    ModelInfo quadInfo;
    modelsGeo.push_back(modelLoader->LoadQuad(quadInfo));


    


    ModelInfo sphereInfo;
    cubeInfo.dispScale = 0.0f;
    XMMATRIX sphereScale = XMMatrixScaling(10.0f, 10.0f, 10.0f);
    XMMATRIX sphereOffset = XMMatrixTranslation(20.0f, 55.0f, -10.0f);
    XMStoreFloat4x4(&sphereInfo.world, sphereScale * sphereOffset);
    modelsGeo.push_back(modelLoader->LoadSphere(sphereInfo));

#pragma region CurtainsAndBullets

    for (auto& e : AllRitems()) {
        std::string matName = e->Mat->name;
        std::transform(matName.begin(), matName.end(), matName.begin(), ::tolower);

        if (matName.find("water") != std::string::npos) {
            mWaterRitems.push_back(e.get());
        }
        else if (matName.find("fabric") != std::string::npos || matName.find("curtain") != std::string::npos) {
            mCurtainRitems.push_back(e.get());
        }
        else {
            mOpaqueRitems.push_back(e.get());
        }
    }

    BuildSceneOctree();
    BuildOctreeDebugVisualization();

    InitBulletPool();

#pragma endregion
}


void Meow::BuildGrass()
{
    mGrassInstanceBuffer = std::make_unique<UploadBuffer<GrassInstanceData>>(md3dDevice.Get(), mGrassInstanceCount, false);
    mAllGrassInstances.resize(mGrassInstanceCount);

    std::mt19937 randEngine(1337);
    std::uniform_real_distribution<float> xzDist(-500.0f, 500.0f);
    std::uniform_real_distribution<float> scaleDist(0.5f, 1.5f);

    for (UINT i = 0; i < mGrassInstanceCount; ++i)
    {
        mAllGrassInstances[i].Position = XMFLOAT3(xzDist(randEngine), 0.0f, xzDist(randEngine));
        mAllGrassInstances[i].Scale = scaleDist(randEngine);
    }
}


void Meow::BuildGrassRootSignature()
{
    CD3DX12_ROOT_PARAMETER slotRootParameter[2];
    slotRootParameter[0].InitAsShaderResourceView(0);

    slotRootParameter[1].InitAsConstantBufferView(1);

    CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(2, slotRootParameter, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    ComPtr<ID3DBlob> serializedRootSig = nullptr;
    ComPtr<ID3DBlob> errorBlob = nullptr;
    HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &serializedRootSig, &errorBlob);
    if (errorBlob != nullptr)
        ::OutputDebugStringA((char*)errorBlob->GetBufferPointer());

    ThrowIfFailed(hr);
    ThrowIfFailed(md3dDevice->CreateRootSignature(0, serializedRootSig->GetBufferPointer(), serializedRootSig->GetBufferSize(), IID_PPV_ARGS(&mGrassRootSignature)));
}

void Meow::InitBulletPool()
{
    const auto submeshIndexCount = static_cast<UINT>(10 * 10 * 6);
    const UINT objStartIndex = static_cast<UINT>(AllRitems().size());
    for (UINT i = 0; i < kMaxBullets; ++i)
    {
        auto bulletRI = std::make_unique<RenderItem>();
        bulletRI->ObjCBIndex = static_cast<UINT>(AllRitems().size());
        bulletRI->Geo = mBulletGeo.get();
        bulletRI->Mat = Materials()["BulletGlow"].get();
        bulletRI->IndexCount = submeshIndexCount;
        bulletRI->StartIndexLocation = 0;
        bulletRI->BaseVertexLocation = 0;
        bulletRI->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        bulletRI->LocalBounds.Center = { 0.0f, 0.0f, 0.0f };
        bulletRI->LocalBounds.Extents = { kBulletRadius, kBulletRadius, kBulletRadius };
        XMStoreFloat4x4(&bulletRI->World, XMMatrixTranslation(0.0f, -10000.0f, 0.0f));

        mBullets[i].RenderProxy = bulletRI.get();
        mBulletRitems.push_back(bulletRI.get());
        AllRitems().push_back(std::move(bulletRI));
    }
}

void Meow::SpawnBullet()
{
    BulletState& bullet = mBullets[mNextBulletSlot];
    bullet.IsActive = true;
    bullet.LifeLeft = kBulletLifeTime;
    bullet.RenderProxy->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    XMVECTOR eye = XMLoadFloat3(&mEyePos);
    XMVECTOR dir = XMVector3Normalize(XMLoadFloat3(&mForward));
    XMVECTOR spawnPos = XMVectorMultiplyAdd(XMVectorReplicate(2.5f), dir, eye);
    XMVECTOR velocity = XMVectorScale(dir, kBulletSpeed);

    XMStoreFloat3(&bullet.Position, spawnPos);
    XMStoreFloat3(&bullet.Velocity, velocity);

    ++mNextBulletSlot;
    if (mNextBulletSlot >= kMaxBullets)
    {
        mNextBulletSlot = 0;
    }
}

void Meow::UpdateBullets(float dt)
{
    for (BulletState& bullet : mBullets)
    {
        if (!bullet.IsActive)
        {
            continue;
        }

        bullet.LifeLeft -= dt;
        if (bullet.LifeLeft <= 0.0f)
        {
            bullet.IsActive = false;
            XMStoreFloat4x4(&bullet.RenderProxy->World, XMMatrixTranslation(0.0f, -10000.0f, 0.0f));
            continue;
        }

        XMVECTOR pos = XMLoadFloat3(&bullet.Position);
        XMVECTOR vel = XMLoadFloat3(&bullet.Velocity);
        pos = XMVectorMultiplyAdd(XMVectorReplicate(dt), vel, pos);
        XMStoreFloat3(&bullet.Position, pos);

        XMMATRIX world = XMMatrixScaling(kBulletRadius, kBulletRadius, kBulletRadius) *
            XMMatrixTranslation(bullet.Position.x, bullet.Position.y, bullet.Position.z);
        XMStoreFloat4x4(&bullet.RenderProxy->World, world);
    }
}

void Meow::UpdateBulletLights()
{
    mDeferredLights.clear();

    for (UINT i = 0; i < mStaticDirLights; ++i)
    {
        mDeferredLights.push_back(mStaticLights[i]);
    }

    UINT pointStart = mStaticDirLights;
    for (UINT i = 0; i < mStaticPointLights; ++i)
    {
        mDeferredLights.push_back(mStaticLights[pointStart + i]);
    }

    UINT bulletLightCount = 0;
    for (const BulletState& bullet : mBullets)
    {
        if (bullet.IsActive)
        {
            Light bulletLight;
            bulletLight.Position = bullet.Position;
            bulletLight.Strength = XMFLOAT3(100.0f, 100.5f, 100.0f);
            bulletLight.FalloffStart = 0.9f;
            bulletLight.FalloffEnd = 20.5f;
            bulletLight.Direction = XMFLOAT3(1.0f, -1.0f, 1.0f);
            bulletLight.SpotPower = 100.0f;

            mDeferredLights.push_back(bulletLight);
            bulletLightCount++;
        }
    }

    UINT spotStart = mStaticDirLights + mStaticPointLights;
    for (UINT i = 0; i < mStaticSpotLights; ++i)
    {
        mDeferredLights.push_back(mStaticLights[spotStart + i]);
    }

    mNumDirLights = mStaticDirLights;
    mNumPointLights = mStaticPointLights + bulletLightCount;
    mNumSpotLights = mStaticSpotLights;

    if (mLightSB)
    {
        for (UINT i = 0; i < mDeferredLights.size(); ++i)
        {
            mLightSB->CopyData(i, mDeferredLights[i]);
        }
    }
}

void Meow::BuildPSO() {
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = mRootSignature.Get();
    psoDesc.VS = { reinterpret_cast<BYTE*>(mvsByteCode->GetBufferPointer()), mvsByteCode->GetBufferSize() };
    psoDesc.PS = { reinterpret_cast<BYTE*>(mpsByteCode->GetBufferPointer()), mpsByteCode->GetBufferSize() };
    psoDesc.HS = { reinterpret_cast<BYTE*>(mhsByteCode->GetBufferPointer()), mhsByteCode->GetBufferSize() };
    psoDesc.DS = { reinterpret_cast<BYTE*>(mdsByteCode->GetBufferPointer()), mdsByteCode->GetBufferSize() };
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.InputLayout = { mInputLayoutDesc.data(), (UINT)mInputLayoutDesc.size() };
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;//D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 3;
    psoDesc.RTVFormats[0] = mBackBufferFormat;
    psoDesc.RTVFormats[1] = DXGI_FORMAT_R32G32B32A32_FLOAT;
    psoDesc.RTVFormats[2] = DXGI_FORMAT_R32_FLOAT;
    psoDesc.DSVFormat = mDepthStencilFormat;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
    psoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;

    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSO)));

    D3D12_GRAPHICS_PIPELINE_STATE_DESC shadowPsoDesc = psoDesc;
    shadowPsoDesc.pRootSignature = mLightRootSig.Get();
    shadowPsoDesc.VS = { reinterpret_cast<BYTE*>(mvsShadowByteCode->GetBufferPointer()), mvsShadowByteCode->GetBufferSize() };
    shadowPsoDesc.HS = { nullptr, 0 };
    shadowPsoDesc.DS = { nullptr, 0 };
    shadowPsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    shadowPsoDesc.PS = { nullptr, 0 };
    shadowPsoDesc.NumRenderTargets = 0;
    shadowPsoDesc.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;
    shadowPsoDesc.RTVFormats[1] = DXGI_FORMAT_UNKNOWN;
    shadowPsoDesc.RTVFormats[2] = DXGI_FORMAT_UNKNOWN;
    shadowPsoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
    
    shadowPsoDesc.DepthStencilState.StencilEnable = true;
    shadowPsoDesc.DepthStencilState.StencilReadMask = 0xFF;
    shadowPsoDesc.DepthStencilState.StencilWriteMask = 0xFF;
    shadowPsoDesc.DepthStencilState.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL;
    shadowPsoDesc.DepthStencilState.FrontFace.StencilPassOp = D3D12_STENCIL_OP_REPLACE;
    shadowPsoDesc.DepthStencilState.FrontFace.StencilFailOp = D3D12_STENCIL_OP_REPLACE;
    shadowPsoDesc.DepthStencilState.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
    shadowPsoDesc.DepthStencilState.BackFace = shadowPsoDesc.DepthStencilState.FrontFace;
    shadowPsoDesc.RasterizerState.DepthBias = 500;
    shadowPsoDesc.RasterizerState.SlopeScaledDepthBias = 1.0f;
    shadowPsoDesc.RasterizerState.DepthBiasClamp = 0.0f;
    shadowPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&shadowPsoDesc, IID_PPV_ARGS(&mShadowPSO)));

    D3D12_GRAPHICS_PIPELINE_STATE_DESC grassPsoDesc = psoDesc;
    grassPsoDesc.pRootSignature = mRootSignature.Get();
    grassPsoDesc.pRootSignature = mGrassRootSignature.Get();
    grassPsoDesc.VS = { reinterpret_cast<BYTE*>(mvsGrassByteCode->GetBufferPointer()), mvsGrassByteCode->GetBufferSize() };
    grassPsoDesc.PS = { reinterpret_cast<BYTE*>(mpsGrassByteCode->GetBufferPointer()), mpsGrassByteCode->GetBufferSize() };
    grassPsoDesc.HS = { nullptr, 0 };
    grassPsoDesc.DS = { nullptr, 0 };
    grassPsoDesc.InputLayout = { nullptr, 0 };
    grassPsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&grassPsoDesc, IID_PPV_ARGS(&mGrassPSO)));

    psoDesc.VS = { reinterpret_cast<BYTE*>(mvsWaveByteCode->GetBufferPointer()), mvsWaveByteCode->GetBufferSize() };
    psoDesc.HS = { reinterpret_cast<BYTE*>(mhsWaveByteCode->GetBufferPointer()), mhsWaveByteCode->GetBufferSize() };
    psoDesc.DS = { reinterpret_cast<BYTE*>(mdsCurtainByteCode->GetBufferPointer()), mdsCurtainByteCode->GetBufferSize() };
    psoDesc.PS = { reinterpret_cast<BYTE*>(mpsCurtainByteCode->GetBufferPointer()), mpsCurtainByteCode->GetBufferSize() };
    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mCurtainPSO)));

    psoDesc.DS = { reinterpret_cast<BYTE*>(mdsWaterByteCode->GetBufferPointer()), mdsWaterByteCode->GetBufferSize() };
    psoDesc.PS = { reinterpret_cast<BYTE*>(mpsWaterByteCode->GetBufferPointer()), mpsWaterByteCode->GetBufferSize() };

    D3D12_RENDER_TARGET_BLEND_DESC transparencyBlendDesc;
    transparencyBlendDesc.BlendEnable = true;
    transparencyBlendDesc.LogicOpEnable = false;
    transparencyBlendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
    transparencyBlendDesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    transparencyBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
    transparencyBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
    transparencyBlendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;
    transparencyBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    transparencyBlendDesc.LogicOp = D3D12_LOGIC_OP_NOOP;
    transparencyBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    psoDesc.BlendState.RenderTarget[0] = transparencyBlendDesc;
    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mWaterPSO)));

}

void Meow::BuildDisplayPSO()
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;
    ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));


    psoDesc.VS = { reinterpret_cast<BYTE*>(mvsLightByteCode->GetBufferPointer()), mvsLightByteCode->GetBufferSize() };
    psoDesc.PS = { reinterpret_cast<BYTE*>(mpsLightByteCode->GetBufferPointer()), mpsLightByteCode->GetBufferSize() };

    psoDesc.InputLayout = { nullptr, 0 };
    psoDesc.pRootSignature = mLightRootSig.Get();

    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);


    psoDesc.DepthStencilState.DepthEnable = FALSE;
    psoDesc.DepthStencilState.StencilEnable = FALSE;

    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;


    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;
    psoDesc.SampleDesc.Count = 1;

    md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mLightPSO));
}

void Meow::BuildPostProcPSO()
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;
    ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));


    psoDesc.VS = { reinterpret_cast<BYTE*>(mvsPostProcByteCode->GetBufferPointer()), mvsPostProcByteCode->GetBufferSize() };
    psoDesc.PS = { reinterpret_cast<BYTE*>(mpsPostProcByteCode->GetBufferPointer()), mpsPostProcByteCode->GetBufferSize() };

    psoDesc.InputLayout = { nullptr, 0 };
    psoDesc.pRootSignature = mPostProcRootSig.Get();

    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);


    psoDesc.DepthStencilState.DepthEnable = FALSE;
    psoDesc.DepthStencilState.StencilEnable = FALSE;

    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;


    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = mBackBufferFormat;
    psoDesc.SampleDesc.Count = 1;

    md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPostProcPSO));
}



void Meow::BuildGlowPSO()
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = mRootSignature.Get();
    psoDesc.VS = { reinterpret_cast<BYTE*>(mvsGlowByteCode->GetBufferPointer()), mvsGlowByteCode->GetBufferSize() };
    psoDesc.PS = { reinterpret_cast<BYTE*>(mpsGlowByteCode->GetBufferPointer()), mpsGlowByteCode->GetBufferSize() };
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.BlendState.RenderTarget[0].BlendEnable = TRUE;
    psoDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
    psoDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
    psoDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
    psoDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
    psoDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
    psoDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
    psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState.DepthEnable = TRUE;
    psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;

    psoDesc.InputLayout = { mInputLayoutDesc.data(), (UINT)mInputLayoutDesc.size() };
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
    psoDesc.DSVFormat = mDepthStencilFormat;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
    psoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;

    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mGlowPSO)));

    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    psoDesc.DepthStencilState.DepthEnable = TRUE;
    psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mOctreeDebugPSO)));
}



std::array<const CD3DX12_STATIC_SAMPLER_DESC, 7> Meow::GetStaticSamplers()
{


    const CD3DX12_STATIC_SAMPLER_DESC pointWrap(
        0, // shaderRegister
        D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

    const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
        1, // shaderRegister
        D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

    const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
        2, // shaderRegister
        D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

    const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
        3, // shaderRegister
        D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

    const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
        4, // shaderRegister
        D3D12_FILTER_ANISOTROPIC, // filter
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressW
        0.0f,                             // mipLODBias
        8);                               // maxAnisotropy

    const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(
        5, // shaderRegister
        D3D12_FILTER_ANISOTROPIC, // filter
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressW
        0.0f,                              // mipLODBias
        8);                                // maxAnisotropy
    const CD3DX12_STATIC_SAMPLER_DESC shadowBorder(
        6, // shaderRegister (s1)
        D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT, // filter
        D3D12_TEXTURE_ADDRESS_MODE_BORDER,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_BORDER,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_BORDER,  // addressW
        0.0f,                               // mipLODBias
        16,                                 // maxAnisotropy
        D3D12_COMPARISON_FUNC_LESS_EQUAL,   // comparisonFunc 
        D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE);

    return {
        pointWrap, pointClamp,
        linearWrap, linearClamp,
        anisotropicWrap, anisotropicClamp, shadowBorder };
}

ComPtr<ID3D12Device> Meow::GetDevice() {
    return md3dDevice;
}


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance,
    PSTR cmdLine, int showCmd)
{
#if defined(DEBUG) || defined(_DEBUG)
    ComPtr<ID3D12Debug> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
    {
        debugController->EnableDebugLayer();
    }
#endif

    try
    {
        Meow theApp(hInstance);
        if (!theApp.Initialize())
            return 0;

        return theApp.Run();
    }
    catch (std::exception& e)
    {
        MessageBoxA(nullptr, e.what(), "DirectX Application Failed", MB_OK);
        return 0;
    }

}
