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

#include <cstdint>
#include <assimp/postprocess.h>
#include <DirectXColors.h>

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

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
    void LoadModelAndTextures();
    void UpdateObjectCBs(const GameTimer& gt);
    void BuildDescriptorHeaps();
    void BuildConstantBuffers();
    void BuildMaterials();
    void UpdateMaterialCBs(const GameTimer& gt);
    void AnimateMaterials(const GameTimer& gt);
    void BuildRootSignature();
    void BuildLightRootSignature();
    void BuildShadersAndInputLayout();
    void BuildPSO();
    void BuildDisplayPSO();
    void UpdatePassCB(const GameTimer& gt);
    void BuildDeferredLights();
    void AddDirectionalDeferredLight(const XMFLOAT3& direction, const XMFLOAT3& strength);
    void AddPointDeferredLight(const XMFLOAT3& position, const XMFLOAT3& strength, float falloffStart, float falloffEnd);
    void AddSpotDeferredLight(const XMFLOAT3& position, const XMFLOAT3& direction, const XMFLOAT3& strength, float falloffStart, float falloffEnd, float spotPower);
    void InitBulletPool();
    void SpawnBullet();
    void UpdateBullets(float dt);
    void UpdateBulletLights();
    void BuildGlowPSO();

    GBuffer* mgBuffer;

    XMFLOAT3 mSunThetaPhi = { 1.25f * XM_PI, XM_PIDIV4, 0.0f };

    float mYaw = 1.5f * XM_PI;
    float mPitch = 0.0f; 

    float mCameraSpeed = 50.0f;

    XMFLOAT3 mForward = { 0.0f, 0.0f, 1.0f };
    XMFLOAT3 mRight = { 1.0f, 0.0f, 0.0f };
    XMFLOAT3 mUp = { 0.0f, 1.0f, 0.0f };
    
    std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers();

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

    std::unordered_map<std::string, std::unique_ptr<Texture>> mTextures;

    std::unordered_map<std::string, std::unique_ptr<Material>> mMaterials;
    std::unique_ptr<UploadBuffer<MaterialConstants>> mMaterialCB;

    std::unique_ptr<UploadBuffer<ObjectConstants>> mObjectCB = nullptr;
    ComPtr<ID3D12DescriptorHeap> mSrvDescriptorHeap;

    std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayoutDesc;

    ComPtr<ID3D12RootSignature> mRootSignature;
    ComPtr<ID3D12RootSignature> mLightRootSig;

    ComPtr<ID3DBlob> mvsByteCode = nullptr;
    ComPtr<ID3DBlob> mpsByteCode = nullptr;
    ComPtr<ID3D10Blob> mhsByteCode = nullptr;
    ComPtr<ID3D10Blob> mdsByteCode = nullptr;
    ComPtr<ID3D12PipelineState> mPSO = nullptr;

    ComPtr<ID3DBlob> mvsLightByteCode = nullptr;
    ComPtr<ID3DBlob> mpsLightByteCode = nullptr;
    ComPtr<ID3D12PipelineState> mLightPSO = nullptr;

    ComPtr<ID3DBlob> mvsWaveByteCode = nullptr;
    ComPtr<ID3DBlob> mpsWaveByteCode = nullptr;
    ComPtr<ID3DBlob> mhsWaveByteCode = nullptr;
    ComPtr<ID3DBlob> mdsWaveByteCode = nullptr;
    ComPtr<ID3D12PipelineState> mWavePSO = nullptr;

    ComPtr<ID3DBlob> mvsGlowByteCode = nullptr;
    ComPtr<ID3DBlob> mpsGlowByteCode = nullptr;
    ComPtr<ID3D12PipelineState> mGlowPSO = nullptr;

    std::vector<std::unique_ptr<RenderItem>> mAllRitems;
    std::vector<RenderItem*> mOpaqueRitems;
    std::vector<std::string> mTextureOrder;
    std::unique_ptr<MeshGeometry> mModelSponza = nullptr;
    std::vector<RenderItem*> mCurtainRitems;

    std::unique_ptr<UploadBuffer<Light>> mLightSB;
    std::vector<Light> mDeferredLights;
    UINT mNumDirLights = 0;
    UINT mNumPointLights = 0;
    UINT mNumSpotLights = 0;

    std::vector<RenderItem*> mBulletRitems;
    std::unique_ptr<MeshGeometry> mBulletGeo = nullptr;

    struct BulletState
    {
        bool IsActive = false;
        float LifeLeft = 0.0f;
        XMFLOAT3 Position = { 0.0f, 0.0f, 0.0f };
        XMFLOAT3 Velocity = { 0.0f, 0.0f, 0.0f };
        RenderItem* RenderProxy = nullptr;
    };

    static constexpr UINT kMaxBullets = 1500;
    static constexpr float kBulletSpeed = 180.0f;
    static constexpr float kBulletLifeTime = 8.0f;
    static constexpr float kBulletRadius = 0.35f;
    std::array<BulletState, kMaxBullets> mBullets;
    UINT mNextBulletSlot = 0;
    bool mWasSpaceDown = false;

    std::vector<Light> mStaticLights;
    UINT mStaticDirLights = 0;
    UINT mStaticPointLights = 0;
    UINT mStaticSpotLights = 0;
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
    if(!D3DApp::Initialize())
		return false;
		
    ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

    LoadModelAndTextures();
    BuildDescriptorHeaps();
    BuildConstantBuffers();
    BuildRootSignature();
    BuildLightRootSignature();
    BuildShadersAndInputLayout();
    BuildPSO();
    BuildDisplayPSO();
    BuildGlowPSO();

    mgBuffer = new GBuffer(md3dDevice.Get(), mClientWidth, mClientHeight);
    BuildDeferredLights();
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

    const bool isSpaceDown = (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;
    if (isSpaceDown && !mWasSpaceDown)
    {
        SpawnBullet();
    }
    mWasSpaceDown = isSpaceDown;

    XMStoreFloat3(&mEyePos, pos);


    XMVECTOR target = XMVectorAdd(pos, forward);
    XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
    XMStoreFloat4x4(&mView, view);

    UpdateBullets(dt);
    UpdateBulletLights();

  
    UpdateObjectCBs(gt);
    UpdateMaterialCBs(gt);
    UpdatePassCB(gt);
}
void Meow::UpdateObjectCBs(const GameTimer& gt) {
    XMMATRIX view = XMLoadFloat4x4(&mView);
    XMMATRIX proj = XMLoadFloat4x4(&mProj);

    for (auto& e : mAllRitems) {
        XMMATRIX world = XMLoadFloat4x4(&e->World);
        XMMATRIX worldViewProj = world * view * proj;

        ObjectConstants objConst;
        XMStoreFloat4x4(&objConst.WorldViewProj, XMMatrixTranspose(worldViewProj));
        mObjectCB->CopyData(e->ObjCBIndex, objConst);
    }
}

void Meow::Draw(const GameTimer& gt)
{
    auto cmdListAlloc = mDirectCmdListAlloc.Get();
    ThrowIfFailed(cmdListAlloc->Reset());
    ThrowIfFailed(mCommandList->Reset(cmdListAlloc, mPSO.Get()));


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

    ID3D12DescriptorHeap* descriptorHeaps[] = { mSrvDescriptorHeap.Get() };
    mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

    mCommandList->SetGraphicsRootSignature(mRootSignature.Get());


    mCommandList->SetGraphicsRootConstantBufferView(2, mPassCB->Resource()->GetGPUVirtualAddress());

    UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
    UINT matCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(MaterialConstants));
    UINT srvSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);


    for (auto ri : mOpaqueRitems) {
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


    mCommandList->SetPipelineState(mWavePSO.Get());

    for (auto ri : mCurtainRitems) {
        auto vbv = ri->Geo->VertexBufferView();
        auto ibv = ri->Geo->IndexBufferView();
        mCommandList->IASetVertexBuffers(0, 1, &vbv);
        mCommandList->IASetIndexBuffer(&ibv);
        mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST/*ri->PrimitiveType*/);

        CD3DX12_GPU_DESCRIPTOR_HANDLE texHandle(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
        int texIdx = (ri->Mat->DiffuseSrvHeapIndex >= 0) ? ri->Mat->DiffuseSrvHeapIndex : 0;
        texHandle.Offset(texIdx, srvSize);

        mCommandList->SetGraphicsRootDescriptorTable(0, texHandle);
        mCommandList->SetGraphicsRootConstantBufferView(1, mObjectCB->Resource()->GetGPUVirtualAddress() + ri->ObjCBIndex * objCBByteSize);
        mCommandList->SetGraphicsRootConstantBufferView(3, mMaterialCB->Resource()->GetGPUVirtualAddress() + ri->Mat->matCBIndex * matCBByteSize);

        mCommandList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
    }



    mgBuffer->TransitToLightRenderingState(mCommandList);
    
    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        mgBuffer->Pos.Get(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    mCommandList->ResourceBarrier(1, &barrier);


    auto backBufferRtv = CurrentBackBufferView();
    mCommandList->OMSetRenderTargets(1, &backBufferRtv, false, nullptr);


    mCommandList->ClearRenderTargetView(backBufferRtv, Colors::LightSteelBlue, 0, nullptr);


    mCommandList->SetPipelineState(mLightPSO.Get());
    mCommandList->SetGraphicsRootSignature(mLightRootSig.Get());

    ID3D12DescriptorHeap* heaps[] = { mgBuffer->mSrvDescriptorHeap.Get() };
    mCommandList->SetDescriptorHeaps(_countof(heaps), heaps);
    mCommandList->SetGraphicsRootDescriptorTable(0, mgBuffer->mSrvBaseGpuHandle);
    mCommandList->SetGraphicsRootConstantBufferView(1, mPassCB->Resource()->GetGPUVirtualAddress());



    mCommandList->IASetVertexBuffers(0, 0, nullptr); 
    mCommandList->IASetIndexBuffer(nullptr);
    mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    mCommandList->DrawInstanced(3, 1, 0, 0);


    ID3D12DescriptorHeap* modelHeaps[] = { mSrvDescriptorHeap.Get() };
    mCommandList->SetDescriptorHeaps(_countof(modelHeaps), modelHeaps);
    mCommandList->SetPipelineState(mGlowPSO.Get());
    mCommandList->SetGraphicsRootSignature(mRootSignature.Get());
    mCommandList->OMSetRenderTargets(1, &backBufferRtv, false, &dsv);
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

    auto seaMat = mMaterials["Model"].get();
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
    texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND);

    slotRootParametr[0].InitAsDescriptorTable(1, &texTable, D3D12_SHADER_VISIBILITY_PIXEL);
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
    texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 4, 0);

    CD3DX12_ROOT_PARAMETER slotRootParameter[2];
    slotRootParameter[0].InitAsDescriptorTable(1, &texTable, D3D12_SHADER_VISIBILITY_PIXEL);
    slotRootParameter[1].InitAsConstantBufferView(0);

    auto staticSamplers = GetStaticSamplers();

    CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(2, slotRootParameter,
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

void Meow::BuildDescriptorHeaps()
{
    UINT numDescriptors = static_cast<UINT>(mMaterials.size()) * 2;
    if (numDescriptors == 0) numDescriptors = 1; 

    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
    srvHeapDesc.NumDescriptors = numDescriptors;
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mSrvDescriptorHeap)));

    CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
    UINT srvSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    for (const auto& matEntry : mMaterials) {
        const Material* mat = matEntry.second.get();
        if (mat->DiffuseSrvHeapIndex < 0 || mat->NormalSrvHeapIndex < 0) {
            continue;
        }

        auto createSrvAtIndex = [&](const std::string& texName, int descriptorIndex) {
            auto texIt = mTextures.find(texName);
            if (texIt == mTextures.end() || !texIt->second || !texIt->second->resource) {
                return;
            }

            auto resource = texIt->second->resource;
            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.Format = resource->GetDesc().Format;
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Texture2D.MostDetailedMip = 0;
            srvDesc.Texture2D.MipLevels = resource->GetDesc().MipLevels;

            CD3DX12_CPU_DESCRIPTOR_HANDLE dst(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
            dst.Offset(descriptorIndex, srvSize);
            md3dDevice->CreateShaderResourceView(resource.Get(), &srvDesc, dst);
            };

        createSrvAtIndex(mat->DiffuseMapName, mat->DiffuseSrvHeapIndex);
        createSrvAtIndex(mat->NormalMapName, mat->NormalSrvHeapIndex);
    }

}

void Meow::BuildConstantBuffers() {
    UINT objCount = (UINT)mAllRitems.size();
    UINT matCount = (UINT)mMaterials.size();

    if (objCount == 0) objCount = 1;
    if (matCount == 0) matCount = 1;

    mObjectCB = std::make_unique<UploadBuffer<ObjectConstants>>(md3dDevice.Get(), objCount, true);
    mMaterialCB = std::make_unique<UploadBuffer<MaterialConstants>>(md3dDevice.Get(), matCount, true);
    mPassCB = std::make_unique<UploadBuffer<PassConstants>>(md3dDevice.Get(), 1, true);
}

void Meow::BuildMaterials() {
    auto mat = std::make_unique<Material>();
    mat->name = "Model";
    mat->matCBIndex = 0;
    mat->diffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
    mat->fresnelRO = { 0.1f, 0.1f, 0.1f };
    mat->roughness = 0.125f;
    mat->DiffuseSrvHeapIndex = 0;
    mMaterials["Model"] = std::move(mat);

}

void Meow::UpdateMaterialCBs(const GameTimer& gt) {
    
    for (auto& e : mMaterials) {
        Material* mat = e.second.get();

        MaterialConstants matConst;
        XMMATRIX matTransform = XMLoadFloat4x4(&mat->matTransform);
        matConst.DiffuseAlbedo = mat->diffuseAlbedo;
        matConst.FresnelR0 = mat->fresnelRO;
        matConst.Roughness = mat->roughness;
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
    UINT kPointLights = 1000;
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
    AddDirectionalDeferredLight(dirNorm, XMFLOAT3(0.45f, 0.45f, 0.40f));

    for (UINT i = 0; i < kPointLights; ++i)
    {
        const float x =  (i % 60) * 16.0f;
        const float z =  (i / 60) * 12.0f;
        AddPointDeferredLight(XMFLOAT3(x, 8.0f, z), XMFLOAT3(0.01f, 0.08f, 0.06f), 20.0f, 100.0f);
    }

    XMVECTOR spotDir = XMVector3Normalize(XMVectorSet(0.7f, -1.0f, -0.7f, 0.0f));
    XMFLOAT3 spotDirNorm;
    XMStoreFloat3(&spotDirNorm, spotDir);
    AddSpotDeferredLight(XMFLOAT3(0.0f, 100.0f, 10.0f), spotDirNorm, XMFLOAT3(10.8f, 10.5f, 111.0f), 500.0f, 10000.0f,  200.0f);

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

   // XMVECTOR lightDir = -MathHelper::SphericalToCartesian(1.0f, mSunTheta, mSunPhi);

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
}


void Meow::BuildShadersAndInputLayout()
{
    std::wstring shaderPath = L"color.hlsl";
    std::wstring shaderDisplayPath = L"Display.hlsl";
    std::wstring waveShaderPath = L"wave.hlsl";
    std::wstring glowShaderPath = L"glow.hlsl";

    try {
 
        mvsByteCode = d3dUtil::CompileShader(shaderPath, nullptr, "VS", "vs_5_0");
        mpsByteCode = d3dUtil::CompileShader(shaderPath, nullptr, "PS", "ps_5_0");
        mhsByteCode = d3dUtil::CompileShader(shaderPath, nullptr, "HS", "hs_5_0");
        mdsByteCode = d3dUtil::CompileShader(shaderPath, nullptr, "DS", "ds_5_0");

        mvsLightByteCode = d3dUtil::CompileShader(shaderDisplayPath, nullptr, "VS", "vs_5_0");
        mpsLightByteCode = d3dUtil::CompileShader(shaderDisplayPath, nullptr, "PS", "ps_5_0");
       
        mvsWaveByteCode = d3dUtil::CompileShader(waveShaderPath, nullptr, "VS", "vs_5_0");
        mpsWaveByteCode = d3dUtil::CompileShader(waveShaderPath, nullptr, "PS", "ps_5_0");
        mhsWaveByteCode = d3dUtil::CompileShader(waveShaderPath, nullptr, "HS", "hs_5_0");
        mdsWaveByteCode = d3dUtil::CompileShader(waveShaderPath, nullptr, "DS", "ds_5_0");

        mvsGlowByteCode = d3dUtil::CompileShader(glowShaderPath, nullptr, "VS", "vs_5_0");
        mpsGlowByteCode = d3dUtil::CompileShader(glowShaderPath, nullptr, "PS", "ps_5_0");
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

    std::string baseDirSponza = "sponza\\";
    std::string fileNameSponza = "sponza.obj";

    std::string baseDirStone = "old_stone\\";
    std::string fileNameStone = baseDirSponza + "Sketchfab.fbx";

    Assimp::Importer importer;
    const aiScene* sponza = importer.ReadFile(fileNameSponza,
        aiProcess_Triangulate |
        aiProcess_GenSmoothNormals |
        aiProcess_FlipUVs |
        aiProcess_JoinIdenticalVertices | aiProcess_CalcTangentSpace);


    if (!sponza || !sponza->mRootNode) {
        MessageBoxA(nullptr, "Could not load Sponza. Check paths!", "Error", MB_OK);
        return;
    }

    mModelSponza = std::make_unique<MeshGeometry>();
    mModelSponza->Name = "SponzaGeo";

    int srvHeapIndex = 0;

    const std::string defaultDiffuseTexName = "background.dds";
    const std::string defaultNormalTexName = "background_ddn.dds";

    auto loadTextureIfNeeded = [&](const std::string& texName) -> bool {
        if (texName.empty()) {
            return false;
        }
        if (mTextures.find(texName) != mTextures.end()) {
            return mTextures[texName] && mTextures[texName]->resource != nullptr;
        }

        auto tex = std::make_unique<Texture>();
        std::string fullPath = baseDirSponza + texName;
        std::wstring wfullPath(fullPath.begin(), fullPath.end());
        HRESULT hr = DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(), mCommandList.Get(), wfullPath.c_str(), tex->resource, tex->uploadHeap);
        if (FAILED(hr)) {
            return false;
        }

        tex->name = texName;
        mTextures[texName] = std::move(tex);
        return true;
    };



    loadTextureIfNeeded(defaultDiffuseTexName);
    loadTextureIfNeeded(defaultNormalTexName);

    for (unsigned int i = 0; i < sponza->mNumMaterials; ++i) {
        aiMaterial* aiMat = sponza->mMaterials[i];
        aiString matName;
        aiMat->Get(AI_MATKEY_NAME, matName);
        std::string name = matName.C_Str();

        auto mat = std::make_unique<Material>();
        mat->name = name;
        mat->matCBIndex = i;
        mat->DiffuseSrvHeapIndex = srvHeapIndex;
        mat->NormalSrvHeapIndex = srvHeapIndex + 1;
        srvHeapIndex += 2;
        std::string diffuseTexName = defaultDiffuseTexName;
      
        if (aiMat->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
            aiString texPath;
            aiMat->GetTexture(aiTextureType_DIFFUSE, 0, &texPath);
            /*std::string texNameStr = texPath.C_Str();

           
            size_t lastDot = texNameStr.find_last_of(".");
            if (lastDot != std::string::npos) texNameStr = texNameStr.substr(0, lastDot) + ".dds";

            if (mTextures.find(texNameStr) == mTextures.end()) {
                auto tex = std::make_unique<Texture>();
                std::string fullPath = baseDirSponza + texNameStr;
                std::wstring wfullPath(fullPath.begin(), fullPath.end());

    
                HRESULT hr = DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(), mCommandList.Get(),
                    wfullPath.c_str(), tex->resource, tex->uploadHeap);

                if (SUCCEEDED(hr)) {
                    tex->name = texNameStr;
                    mat->DiffuseSrvHeapIndex = srvHeapIndex++;
                    mTextureOrder.push_back(texNameStr);
                    mTextures[texNameStr] = std::move(tex);
                }
            }
            else {
           
                for (int idx = 0; idx < mTextureOrder.size(); ++idx) {
                    if (mTextureOrder[idx] == texNameStr) {
                        mat->DiffuseSrvHeapIndex = idx;
                        break;
                    }
                }*/
            diffuseTexName = texPath.C_Str();
            size_t lastDot = diffuseTexName.find_last_of(".");
            if (lastDot != std::string::npos) diffuseTexName = diffuseTexName.substr(0, lastDot) + ".dds";
        }
        if (!loadTextureIfNeeded(diffuseTexName)) {
            diffuseTexName = defaultDiffuseTexName;
            loadTextureIfNeeded(diffuseTexName);
        }

        std::string normalTexName = defaultNormalTexName;
        aiString normalTexPath;
        if (aiMat->GetTexture(aiTextureType_HEIGHT, 0, &normalTexPath) == AI_SUCCESS) {
            normalTexName = normalTexPath.C_Str();
            OutputDebugStringA("Найдено в HEIGHT\n");
        }
        else if (aiMat->GetTexture(static_cast<aiTextureType>(8), 0, &normalTexPath) == AI_SUCCESS) {
            normalTexName = normalTexPath.C_Str();
            std::string msg = "Найдено в ТИПЕ 8 для: " + name + " -> " + normalTexName + "\n";
            OutputDebugStringA(msg.c_str());
        }
        else if (aiMat->GetTexture(aiTextureType_DISPLACEMENT, 0, &normalTexPath) == AI_SUCCESS) {
            normalTexName = normalTexPath.C_Str();
            OutputDebugStringA("Найдено\n");
        }
        else if (aiMat->GetTexture(aiTextureType_NORMALS, 0, &normalTexPath) == AI_SUCCESS) {
            normalTexName = normalTexPath.C_Str();
           // MessageBoxA(nullptr, "Найдено в NORMALS\n", "DEBUG", MB_OK);
            //OutputDebugStringA("Найдено в NORMALS\n");
        }
        else if (aiMat->GetTexture(aiTextureType_UNKNOWN, 0, &normalTexPath) == AI_SUCCESS) {
            normalTexName = normalTexPath.C_Str();
            OutputDebugStringA("Найдено в UNKNOWN!\n");
        }
        else {
          //  std::string msg = "pizdez на материале: " + name + "\n";
           // MessageBoxA(nullptr, msg.c_str(), "DEBUG", MB_OK);
            normalTexName = defaultNormalTexName;
        }
        size_t nLastDot = normalTexName.find_last_of(".");
        if (nLastDot != std::string::npos) normalTexName = normalTexName.substr(0, nLastDot) + ".dds";
        if (!loadTextureIfNeeded(normalTexName)) {
            normalTexName = defaultNormalTexName;
            if (!loadTextureIfNeeded(normalTexName)) {
                normalTexName = diffuseTexName;
            }
        }

        mat->DiffuseMapName = diffuseTexName;
        mat->NormalMapName = normalTexName;
        mMaterials[name] = std::move(mat);
    }

    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;

    for (unsigned int m = 0; m < sponza->mNumMeshes; ++m) {
        aiMesh* mesh = sponza->mMeshes[m];

        SubMeshGeometry subMesh;
        subMesh.BaseVertexLocation = (UINT)vertices.size();
        subMesh.StartIndexLocation = (UINT)indices.size();
        subMesh.IndexCount = mesh->mNumFaces * 3;

        for (unsigned int i = 0; i < mesh->mNumVertices; ++i) {
            Vertex v;
            v.Pos = { mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z };
            v.Normal = { mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z };
            v.TexC = mesh->mTextureCoords[0] ? XMFLOAT2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y) : XMFLOAT2(0, 0);
            if (mesh->HasTangentsAndBitangents()) {
                v.TangentU = { mesh->mTangents[i].x, mesh->mTangents[i].y, mesh->mTangents[i].z };
            }
            else {
                v.TangentU = { 1.0f, 0.0f, 0.0f };
            } 
            vertices.push_back(v);
        }

        for (unsigned int i = 0; i < mesh->mNumFaces; ++i) {
            indices.push_back(mesh->mFaces[i].mIndices[0]);
            indices.push_back(mesh->mFaces[i].mIndices[1]);
            indices.push_back(mesh->mFaces[i].mIndices[2]);
        }

        auto ritem = std::make_unique<RenderItem>();
        ritem->World = MathHelper::Identity4x4();
        XMStoreFloat4x4(&ritem->World, XMMatrixScaling(0.1f, 0.1f, 0.1f));
        ritem->ObjCBIndex = m;
        ritem->Geo = mModelSponza.get();
        ritem->IndexCount = subMesh.IndexCount;
        ritem->StartIndexLocation = subMesh.StartIndexLocation;
        ritem->BaseVertexLocation = subMesh.BaseVertexLocation;

        aiMaterial* aiMat = sponza->mMaterials[mesh->mMaterialIndex];
        aiString mName; aiMat->Get(AI_MATKEY_NAME, mName);
        ritem->Mat = mMaterials[mName.C_Str()].get();

        mAllRitems.push_back(std::move(ritem));
    }

    const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
    const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint32_t);

    mModelSponza->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(), mCommandList.Get(), vertices.data(), vbByteSize, mModelSponza->VertexBufferUploader);
    mModelSponza->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(), mCommandList.Get(), indices.data(), ibByteSize, mModelSponza->IndexBufferUploader);

    mModelSponza->VertexByteStride = sizeof(Vertex);
    mModelSponza->VertexBufferByteSize = vbByteSize;
    mModelSponza->IndexFormat = DXGI_FORMAT_R32_UINT;
    mModelSponza->IndexBufferByteSize = ibByteSize;

#pragma region CurtainsAndBullets

    for (auto& e : mAllRitems) {
        std::string matName = e->Mat->name;

        std::transform(matName.begin(), matName.end(), matName.begin(), ::tolower);

        if (matName.find("fabric") != std::string::npos || matName.find("curtain") != std::string::npos) {
            mCurtainRitems.push_back(e.get());
        }
        else {
            mOpaqueRitems.push_back(e.get());
        }
    }
    auto bulletMat = std::make_unique<Material>();
    bulletMat->name = "BulletGlow";
    bulletMat->matCBIndex = static_cast<UINT>(mMaterials.size());
    bulletMat->DiffuseSrvHeapIndex = srvHeapIndex;
    bulletMat->NormalSrvHeapIndex = srvHeapIndex + 1;
    bulletMat->DiffuseMapName = defaultDiffuseTexName;
    bulletMat->NormalMapName = defaultNormalTexName;
    bulletMat->diffuseAlbedo = { 2.4f, 1.7f, 0.6f, 1.0f };
    bulletMat->fresnelRO = { 0.9f, 0.8f, 0.2f };
    bulletMat->roughness = 0.02f;
    mMaterials[bulletMat->name] = std::move(bulletMat);

    std::vector<Vertex> sphereVertices;
    std::vector<std::uint32_t> sphereIndices;
    UINT kStackCount = 10;
    UINT kSliceCount = 10;
    for (UINT stack = 0; stack <= kStackCount; ++stack)
    {
        const float phi = XM_PI * static_cast<float>(stack) / static_cast<float>(kStackCount);
        for (UINT slice = 0; slice <= kSliceCount; ++slice)
        {
            const float theta = XM_2PI * static_cast<float>(slice) / static_cast<float>(kSliceCount);
            Vertex v;
            v.Pos = {
                std::sinf(phi) * std::cosf(theta),
                std::cosf(phi),
                std::sinf(phi) * std::sinf(theta)
            };
            v.Normal = v.Pos;
            v.TexC = { static_cast<float>(slice) / kSliceCount, static_cast<float>(stack) / kStackCount };
            sphereVertices.push_back(v);
        }
    }

    for (UINT stack = 0; stack < kStackCount; ++stack)
    {
        for (UINT slice = 0; slice < kSliceCount; ++slice)
        {
            UINT i0 = stack * (kSliceCount + 1) + slice;
            UINT i1 = i0 + 1;
            UINT i2 = (stack + 1) * (kSliceCount + 1) + slice;
            UINT i3 = i2 + 1;

            sphereIndices.push_back(i0);
            sphereIndices.push_back(i2);
            sphereIndices.push_back(i1);

            sphereIndices.push_back(i1);
            sphereIndices.push_back(i2);
            sphereIndices.push_back(i3);
        }
    }

    mBulletGeo = std::make_unique<MeshGeometry>();
    mBulletGeo->Name = "BulletSphere";

    const UINT sphereVBByteSize = static_cast<UINT>(sphereVertices.size() * sizeof(Vertex));
    const UINT sphereIBByteSize = static_cast<UINT>(sphereIndices.size() * sizeof(std::uint32_t));
    mBulletGeo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(), mCommandList.Get(), sphereVertices.data(), sphereVBByteSize, mBulletGeo->VertexBufferUploader);
    mBulletGeo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(), mCommandList.Get(), sphereIndices.data(), sphereIBByteSize, mBulletGeo->IndexBufferUploader);
    mBulletGeo->VertexByteStride = sizeof(Vertex);
    mBulletGeo->VertexBufferByteSize = sphereVBByteSize;
    mBulletGeo->IndexFormat = DXGI_FORMAT_R32_UINT;
    mBulletGeo->IndexBufferByteSize = sphereIBByteSize;

    InitBulletPool();

#pragma endregion
}

void Meow::InitBulletPool()
{
    const auto submeshIndexCount = static_cast<UINT>(10 * 10 * 6);
    const UINT objStartIndex = static_cast<UINT>(mAllRitems.size());
    for (UINT i = 0; i < kMaxBullets; ++i)
    {
        auto bulletRI = std::make_unique<RenderItem>();
        bulletRI->ObjCBIndex = objStartIndex + i;
        bulletRI->Geo = mBulletGeo.get();
        bulletRI->Mat = mMaterials["BulletGlow"].get();
        bulletRI->IndexCount = submeshIndexCount;
        bulletRI->StartIndexLocation = 0;
        bulletRI->BaseVertexLocation = 0;
        //ebulletRI->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        XMStoreFloat4x4(&bulletRI->World, XMMatrixTranslation(0.0f, -10000.0f, 0.0f));

        mBullets[i].RenderProxy = bulletRI.get();
        mBulletRitems.push_back(bulletRI.get());
        mAllRitems.push_back(std::move(bulletRI));
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
            bulletLight.FalloffEnd = 2.5f; 
            bulletLight.Direction = XMFLOAT3(1.0f, -1.0f, 1.0f);
            bulletLight.SpotPower = 10.0f;

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

    psoDesc.VS = { reinterpret_cast<BYTE*>(mvsWaveByteCode->GetBufferPointer()), mvsWaveByteCode->GetBufferSize() };
    psoDesc.PS = { reinterpret_cast<BYTE*>(mpsWaveByteCode->GetBufferPointer()), mpsWaveByteCode->GetBufferSize() };
    psoDesc.HS = { reinterpret_cast<BYTE*>(mhsWaveByteCode->GetBufferPointer()), mhsWaveByteCode->GetBufferSize() };
    psoDesc.DS = { reinterpret_cast<BYTE*>(mdsWaveByteCode->GetBufferPointer()), mdsWaveByteCode->GetBufferSize() };

    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mWavePSO)));
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
    psoDesc.RTVFormats[0] = mBackBufferFormat;
    psoDesc.SampleDesc.Count = 1;

    md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mLightPSO));
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
    psoDesc.RTVFormats[0] = mBackBufferFormat;
    psoDesc.DSVFormat = mDepthStencilFormat;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
    psoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;

    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mGlowPSO)));
}
std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> Meow::GetStaticSamplers()
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

    return {
        pointWrap, pointClamp,
        linearWrap, linearClamp,
        anisotropicWrap, anisotropicClamp };
}

ComPtr<ID3D12Device> Meow::GetDevice() {
    return md3dDevice;
}


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance,
    PSTR cmdLine, int showCmd)
{
    // Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
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