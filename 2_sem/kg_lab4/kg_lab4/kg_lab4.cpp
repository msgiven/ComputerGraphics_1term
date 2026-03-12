#define NOMINMAX
#include "D3DApp.h"
#include <DirectXColors.h> 
#include "d3dUtil.h"
#include "MathHelper.h"
#include "UploadBuffer.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>

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

    // Обработчики мыши (пустые заглушки для примера)
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
    void BuildScreenQuadRootSignature();
    void BuildShadersAndInputLayout();
    void BuildPSO();
    void BuildScreenQuadPSO();
    void UpdatePassCB(const GameTimer& gt);

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
    ComPtr<ID3D12RootSignature> mScreenQuadRootSig;

    ComPtr<ID3DBlob> mvsByteCode = nullptr;
    ComPtr<ID3DBlob> mpsByteCode = nullptr;
    ComPtr<ID3D12PipelineState> mPSO = nullptr;

    ComPtr<ID3DBlob> mvsLightByteCode = nullptr;
    ComPtr<ID3DBlob> mpsLightByteCode = nullptr;
    ComPtr<ID3D12PipelineState> mLightPSO = nullptr;

    ComPtr<ID3DBlob> mvsWaveByteCode = nullptr;
    ComPtr<ID3DBlob> mpsWaveByteCode = nullptr;
    ComPtr<ID3D12PipelineState> mWavePSO = nullptr;

    std::vector<std::unique_ptr<RenderItem>> mAllRitems;
    std::vector<RenderItem*> mOpaqueRitems;
    std::vector<std::string> mTextureOrder;
    std::unique_ptr<MeshGeometry> mModelGeo = nullptr;
    std::vector<RenderItem*> mCurtainRitems;

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
    BuildShadersAndInputLayout();
    BuildPSO();

    mgBuffer = new GBuffer(md3dDevice.Get(), mClientWidth, mClientHeight);

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

    XMStoreFloat3(&mEyePos, pos);


    XMVECTOR target = XMVectorAdd(pos, forward);
    XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
    XMStoreFloat4x4(&mView, view);

  
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


    //mCommandList->SetPipelineState(mWavePSO.Get());

    /*for (auto ri : mCurtainRitems) {
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
    }*/



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
    mCommandList->SetGraphicsRootSignature(mScreenQuadRootSig.Get());

    ID3D12DescriptorHeap* heaps[] = { mgBuffer->mSrvDescriptorHeap.Get() };
    mCommandList->SetDescriptorHeaps(_countof(heaps), heaps);
    mCommandList->SetGraphicsRootDescriptorTable(0, mgBuffer->mSrvBaseGpuHandle);


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
    texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND);

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

void Meow::BuildScreenQuadRootSignature()
{
    CD3DX12_DESCRIPTOR_RANGE texTable;
    texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3, 0);

    CD3DX12_ROOT_PARAMETER slotRootParameter[1];
    slotRootParameter[0].InitAsDescriptorTable(1, &texTable, D3D12_SHADER_VISIBILITY_PIXEL);

    auto staticSamplers = GetStaticSamplers();

    CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(1, slotRootParameter,
        (UINT)staticSamplers.size(), staticSamplers.data(),
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    ComPtr<ID3DBlob> serializedRootSig = nullptr;
    ComPtr<ID3DBlob> errorBlob = nullptr;
    D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &serializedRootSig, &errorBlob);

    md3dDevice->CreateRootSignature(0, serializedRootSig->GetBufferPointer(),
        serializedRootSig->GetBufferSize(), IID_PPV_ARGS(&mScreenQuadRootSig));
}

void Meow::BuildDescriptorHeaps()
{
    UINT numDescriptors = (UINT)mTextureOrder.size();
    if (numDescriptors == 0) numDescriptors = 1; 

    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
    srvHeapDesc.NumDescriptors = numDescriptors;
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mSrvDescriptorHeap)));

    CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
    UINT srvSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    for (auto& texName : mTextureOrder) {
        auto resource = mTextures[texName]->resource;
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = resource->GetDesc().Format;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MostDetailedMip = 0;
        srvDesc.Texture2D.MipLevels = resource->GetDesc().MipLevels;

        md3dDevice->CreateShaderResourceView(resource.Get(), &srvDesc, hDescriptor);
        hDescriptor.Offset(1, srvSize);
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

void Meow::UpdatePassCB(const GameTimer& gt)
{
    XMMATRIX view = XMLoadFloat4x4(&mView);
    XMMATRIX proj = XMLoadFloat4x4(&mProj);

    XMMATRIX viewProj = XMMatrixMultiply(view, proj);
    /*XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
    XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
    XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);*/
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

    XMVECTOR lightDir = -MathHelper::SphericalToCartesian(1.0f, mSunTheta, mSunPhi);

    XMStoreFloat3(&mMainPassCB.Lights[0].Direction, lightDir);
    mMainPassCB.Lights[0].Strength = { 0.5f, 0.5f, 0.9f };

    auto currPassCB = mPassCB.get();
    currPassCB->CopyData(0, mMainPassCB);
}


void Meow::BuildShadersAndInputLayout()
{
    std::wstring shaderPath = L"color.hlsl";
    std::wstring shaderDisplayPath = L"display.hlsl";
    //std::wstring waveShaderPath = L"wave.hlsl";

    try {
 
        mvsByteCode = d3dUtil::CompileShader(shaderPath, nullptr, "VS", "vs_5_0");
        mpsByteCode = d3dUtil::CompileShader(shaderPath, nullptr, "PS", "ps_5_0");

        mvsLightByteCode = d3dUtil::CompileShader(shaderDisplayPath, nullptr, "VS", "vs_5_0");
        mpsLightByteCode = d3dUtil::CompileShader(shaderDisplayPath, nullptr, "PS", "ps_5_0");
       
        //mvsWaveByteCode = d3dUtil::CompileShader(waveShaderPath, nullptr, "VS", "vs_5_0");
        //mpsWaveByteCode = d3dUtil::CompileShader(waveShaderPath, nullptr, "PS", "ps_5_0");
    }
    catch (std::exception& e) {
        MessageBoxA(nullptr, e.what(), "Shader Compilation Error", MB_OK);
        throw;
    }

    mInputLayoutDesc = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
    };
}
void Meow::LoadModelAndTextures()
{

    std::string baseDir = "Sponza\\";
    std::string fileName = baseDir + "sponza.obj";

    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(fileName,
        aiProcess_Triangulate |
        aiProcess_GenSmoothNormals |
        aiProcess_FlipUVs |
        aiProcess_JoinIdenticalVertices);

    if (!scene || !scene->mRootNode) {
        MessageBoxA(nullptr, "Could not load Sponza. Check paths!", "Error", MB_OK);
        return;
    }

    auto whiteTex = std::make_unique<Texture>();
    whiteTex->name = "default_white";


    mModelGeo = std::make_unique<MeshGeometry>();
    mModelGeo->Name = "SponzaGeo";

    int srvHeapIndex = 0;

    for (unsigned int i = 0; i < scene->mNumMaterials; ++i) {
        aiMaterial* aiMat = scene->mMaterials[i];
        aiString matName;
        aiMat->Get(AI_MATKEY_NAME, matName);
        std::string name = matName.C_Str();

        auto mat = std::make_unique<Material>();
        mat->name = name;
        mat->matCBIndex = i;
        mat->DiffuseSrvHeapIndex = -1; 

        if (aiMat->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
            aiString texPath;
            aiMat->GetTexture(aiTextureType_DIFFUSE, 0, &texPath);
            std::string texNameStr = texPath.C_Str();

           
            size_t lastDot = texNameStr.find_last_of(".");
            if (lastDot != std::string::npos) texNameStr = texNameStr.substr(0, lastDot) + ".dds";

            if (mTextures.find(texNameStr) == mTextures.end()) {
                auto tex = std::make_unique<Texture>();
                std::string fullPath = baseDir + texNameStr;
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
                }
            }
        }
        mMaterials[name] = std::move(mat);
    }

    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;

    for (unsigned int m = 0; m < scene->mNumMeshes; ++m) {
        aiMesh* mesh = scene->mMeshes[m];

        SubMeshGeometry subMesh;
        subMesh.BaseVertexLocation = (UINT)vertices.size();
        subMesh.StartIndexLocation = (UINT)indices.size();
        subMesh.IndexCount = mesh->mNumFaces * 3;

        for (unsigned int i = 0; i < mesh->mNumVertices; ++i) {
            Vertex v;
            v.Pos = { mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z };
            v.Normal = { mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z };
            v.TexC = mesh->mTextureCoords[0] ? XMFLOAT2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y) : XMFLOAT2(0, 0);
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
        ritem->Geo = mModelGeo.get();
        ritem->IndexCount = subMesh.IndexCount;
        ritem->StartIndexLocation = subMesh.StartIndexLocation;
        ritem->BaseVertexLocation = subMesh.BaseVertexLocation;

        aiMaterial* aiMat = scene->mMaterials[mesh->mMaterialIndex];
        aiString mName; aiMat->Get(AI_MATKEY_NAME, mName);
        ritem->Mat = mMaterials[mName.C_Str()].get();

        mAllRitems.push_back(std::move(ritem));
    }

    const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
    const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint32_t);

    mModelGeo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(), mCommandList.Get(), vertices.data(), vbByteSize, mModelGeo->VertexBufferUploader);
    mModelGeo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(), mCommandList.Get(), indices.data(), ibByteSize, mModelGeo->IndexBufferUploader);

    mModelGeo->VertexByteStride = sizeof(Vertex);
    mModelGeo->VertexBufferByteSize = vbByteSize;
    mModelGeo->IndexFormat = DXGI_FORMAT_R32_UINT;
    mModelGeo->IndexBufferByteSize = ibByteSize;

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
}


void Meow::BuildPSO() {
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = mRootSignature.Get();
    psoDesc.VS = { reinterpret_cast<BYTE*>(mvsByteCode->GetBufferPointer()), mvsByteCode->GetBufferSize() };
    psoDesc.PS = { reinterpret_cast<BYTE*>(mpsByteCode->GetBufferPointer()), mpsByteCode->GetBufferSize() };
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE; 
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.InputLayout = { mInputLayoutDesc.data(), (UINT)mInputLayoutDesc.size() };
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 3;
    psoDesc.RTVFormats[0] = mBackBufferFormat;     
    psoDesc.RTVFormats[1] = DXGI_FORMAT_R32G32B32A32_FLOAT;  
    psoDesc.RTVFormats[2] = DXGI_FORMAT_R32G32B32A32_FLOAT;
    psoDesc.DSVFormat = mDepthStencilFormat;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
    psoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;

    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSO)));

    //psoDesc.VS = { reinterpret_cast<BYTE*>(mvsWaveByteCode->GetBufferPointer()), mvsWaveByteCode->GetBufferSize() };
    //psoDesc.PS = { reinterpret_cast<BYTE*>(mpsWaveByteCode->GetBufferPointer()), mpsWaveByteCode->GetBufferSize() };
    //psoDesc.VS = { reinterpret_cast<BYTE*>(mvsWaveByteCode->GetBufferPointer())};
    //psoDesc.PS = { reinterpret_cast<BYTE*>(mpsWaveByteCode->GetBufferPointer())};

    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mWavePSO)));
}

void Meow::BuildScreenQuadPSO()
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;
    ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));


    psoDesc.VS = { reinterpret_cast<BYTE*>(mvsLightByteCode->GetBufferPointer()), mvsLightByteCode->GetBufferSize() };
    psoDesc.PS = { reinterpret_cast<BYTE*>(mpsLightByteCode->GetBufferPointer()), mpsLightByteCode->GetBufferSize() };

    psoDesc.InputLayout = { nullptr, 0 };
    psoDesc.pRootSignature = mScreenQuadRootSig.Get();

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