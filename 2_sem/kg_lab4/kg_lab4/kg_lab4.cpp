#define NOMINMAX
#include "D3DApp.h"
#include <DirectXColors.h> 
#include "d3dUtil.h"
#include "MathHelper.h"
#include "UploadBuffer.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

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


private:

    void LoadTexture();
    void BuildBoxGeometry();
   // void DrawRenderItems();
    void BuildDescriptorHeaps();
    void BuildConstantBuffers();
    void BuildMaterials();
    void UpdateMaterialCBs(const GameTimer& gt);
    void AnimateMaterials(const GameTimer& gt);
    void BuildRootSignature();
    void BuildShadersAndInputLayout();
    void BuildPSO();
    void UpdatePassCB(const GameTimer& gt);
    
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

    ComPtr<ID3DBlob> mvsByteCode = nullptr;
    ComPtr<ID3DBlob> mpsByteCode = nullptr;
    ComPtr<ID3D12PipelineState> mPSO = nullptr;

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
		
    LoadTexture();

    BuildDescriptorHeaps();
	BuildConstantBuffers();
    BuildMaterials();
    BuildRootSignature();
    BuildShadersAndInputLayout();
    BuildBoxGeometry();
    BuildPSO();

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
    float x = mRadius * sinf(mPhi) * cosf(mTheta);
    float z = mRadius * sinf(mPhi) * sinf(mTheta);
    float y = mRadius * cosf(mPhi);

    mEyePos = { x, y, z };
    XMVECTOR pos = XMVectorSet(x, y, z, 1);
    XMVECTOR target = XMVectorZero();
    XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
    XMStoreFloat4x4(&mView, view);

    XMMATRIX world = XMLoadFloat4x4(&mWorld);
    XMMATRIX proj = XMLoadFloat4x4(&mProj);

    XMMATRIX worldViewProj = world * view * proj;

    ObjectConstants objConst;
    XMStoreFloat4x4(&objConst.WorldViewProj, XMMatrixTranspose(worldViewProj));
    AnimateMaterials(gt);
    mObjectCB->CopyData(0, objConst);
    UpdatePassCB(gt);
    UpdateMaterialCBs(gt);
}

void Meow::Draw(const GameTimer& gt)
{
    ThrowIfFailed(mDirectCmdListAlloc->Reset());
    ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), mPSO.Get()));

    CD3DX12_RESOURCE_BARRIER barrierToRT = CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    mCommandList->ResourceBarrier(1, &barrierToRT);

    mCommandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::Bisque, 0, nullptr);
    mCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

    D3D12_CPU_DESCRIPTOR_HANDLE currentRtv = CurrentBackBufferView();
    D3D12_CPU_DESCRIPTOR_HANDLE dsv = DepthStencilView();
    mCommandList->OMSetRenderTargets(1, &currentRtv, true, &dsv);

    mCommandList->RSSetViewports(1, &mScreenViewport);
    mCommandList->RSSetScissorRects(1, &mScissorRect);

    mCommandList->SetGraphicsRootSignature(mRootSignature.Get());
    ID3D12DescriptorHeap* descriptorHeaps[] = { mSrvDescriptorHeap.Get() };
    mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

    CD3DX12_GPU_DESCRIPTOR_HANDLE tex(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
    D3D12_GPU_VIRTUAL_ADDRESS objCBAdress = mObjectCB->Resource()->GetGPUVirtualAddress();
    D3D12_GPU_VIRTUAL_ADDRESS matCBAdress = mMaterialCB->Resource()->GetGPUVirtualAddress();
    mCommandList->SetGraphicsRootDescriptorTable(0, tex);
    mCommandList->SetGraphicsRootConstantBufferView(1, objCBAdress);
    mCommandList->SetGraphicsRootConstantBufferView(2, mPassCB->Resource()->GetGPUVirtualAddress());
    mCommandList->SetGraphicsRootConstantBufferView(3, matCBAdress);

    D3D12_VERTEX_BUFFER_VIEW vbv = mBoxGeo->VertexBufferView();
    mCommandList->IASetVertexBuffers(0, 1, &vbv);
    D3D12_INDEX_BUFFER_VIEW ibv = mBoxGeo->IndexBufferView();
    mCommandList->IASetIndexBuffer(&ibv);
    mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    mCommandList->DrawIndexedInstanced(mBoxGeo->DrawArgs["box"].IndexCount, 1, mBoxGeo->DrawArgs["box"].StartIndexLocation,
        mBoxGeo->DrawArgs["box"].BaseVertexLocation, 0);

    CD3DX12_RESOURCE_BARRIER barrierToPr = CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    mCommandList->ResourceBarrier(1, &barrierToPr);

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

        mTheta += dx;
        mPhi += dy;
        mPhi = MathHelper::Clamp(mPhi, 0.1f, MathHelper::Pi - 0.1f);

    }
    else if ((btnState & MK_RBUTTON) != 0) {
        float dx = 0.005f * static_cast<float>(x - mLastMousePos.x);
        float dy = 0.005f * static_cast<float>(y - mLastMousePos.y);

        mRadius += dx - dy;
        mRadius = MathHelper::Clamp(mRadius, 3.0f, 1159.0f);
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

void Meow::BuildDescriptorHeaps() {
    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc;
    srvHeapDesc.NumDescriptors = 1;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.NodeMask = 0;

    ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mSrvDescriptorHeap)));

    CD3DX12_CPU_DESCRIPTOR_HANDLE hDesc(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

    auto seashellTex = mTextures["Seashell"]->resource;

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = seashellTex->GetDesc().Format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MipLevels = seashellTex->GetDesc().MipLevels;
    srvDesc.Texture2D.MostDetailedMip = 0;

    md3dDevice->CreateShaderResourceView(seashellTex.Get(), &srvDesc, hDesc);
}

void Meow::BuildConstantBuffers() {
    mObjectCB = std::make_unique<UploadBuffer<ObjectConstants>>(md3dDevice.Get(), 1, true);
    mMaterialCB = std::make_unique<UploadBuffer<MaterialConstants>>(md3dDevice.Get(), 1, true);
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

    // Проверка существования файла
    std::ifstream file(shaderPath);
    if (!file.good()) {
        MessageBoxW(nullptr, L"Shader file not found!", L"Error", MB_OK);
        throw std::runtime_error("Shader file not found");
    }
    file.close();

    try {
        mvsByteCode = d3dUtil::CompileShader(shaderPath, nullptr, "VS", "vs_5_0");
        mpsByteCode = d3dUtil::CompileShader(shaderPath, nullptr, "PS", "ps_5_0");
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

void Meow::LoadTexture() {
    ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

    auto seashellTex = std::make_unique<Texture>();
    seashellTex->name = "Seashell";
    seashellTex->fileName = L"D:/C++Projects/kg_lab4/Seashell/seashellDDs.dds";
    ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(), mCommandList.Get(), 
        seashellTex->fileName.c_str(), seashellTex->resource, seashellTex->uploadHeap));
    ThrowIfFailed(mCommandList->Close());

    ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);
    FlushCommandQueue();
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    mTextures[seashellTex->name] = std::move(seashellTex);
    ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));
}

void Meow::BuildBoxGeometry() {

    std::string fileName = "D:\\C++Projects\\kg_lab4\\Seashell\\seashell.obj";

    Assimp::Importer importer;
    // Флаги: триангуляция, генерация нормалей (если их нет), переворот UV по Y (нужно для DirectX)
    const aiScene* scene = importer.ReadFile(fileName,
        aiProcess_Triangulate |
        aiProcess_GenSmoothNormals |
        aiProcess_FlipUVs |
        aiProcess_JoinIdenticalVertices);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        std::string err = importer.GetErrorString();
        OutputDebugStringA(err.c_str());
        MessageBoxA(nullptr, "Failed to load/parse .obj with Assimp", "Error", MB_OK);
        return;
    }

    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;

    for (unsigned int m = 0; m < scene->mNumMeshes; ++m) {
        aiMesh* mesh = scene->mMeshes[m];

        uint32_t vertexOffset = (uint32_t)vertices.size();


        for (unsigned int i = 0; i < mesh->mNumVertices; ++i) {
            Vertex v;

            v.Pos.x = mesh->mVertices[i].x;
            v.Pos.y = mesh->mVertices[i].y;
            v.Pos.z = mesh->mVertices[i].z;


            if (mesh->HasNormals()) {
                v.Normal.x = mesh->mNormals[i].x;
                v.Normal.y = mesh->mNormals[i].y;
                v.Normal.z = mesh->mNormals[i].z;
            }
            else {
                v.Normal = { 0.0f, 1.0f, 0.0f };
            }

            if (mesh->mTextureCoords[0]) {
                v.TexC.x = mesh->mTextureCoords[0][i].x;
                v.TexC.y = mesh->mTextureCoords[0][i].y;
            }
            else {
                v.TexC = { 0.0f, 0.0f };
            }

            vertices.push_back(v);
        }


        for (unsigned int i = 0; i < mesh->mNumFaces; ++i) {
            aiFace face = mesh->mFaces[i];
            for (unsigned int j = 0; j < face.mNumIndices; ++j) {
      
                indices.push_back(vertexOffset + face.mIndices[j]);
            }
        }
    }

    const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
    const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint32_t);

    mBoxGeo = std::make_unique<MeshGeometry>();
    mBoxGeo->Name = "mBoxGeo";
    //ThrowIfFailed(D3DCreateBlob(vbByteSize, &mBoxGeo->VertexBufferCPU));
    //CopyMemory(mBoxGeo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);
    //ThrowIfFailed(D3DCreateBlob(ibByteSize, &mBoxGeo->IndexBufferCPU));
    //CopyMemory(mBoxGeo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

    mBoxGeo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(), mCommandList.Get(), vertices.data(), vbByteSize, mBoxGeo->VertexBufferUploader);
    mBoxGeo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(), mCommandList.Get(), indices.data(), ibByteSize, mBoxGeo->IndexBufferUploader);

    mBoxGeo->VertexByteStride = sizeof(Vertex);
    mBoxGeo->VertexBufferByteSize = vbByteSize;
    mBoxGeo->IndexFormat = DXGI_FORMAT_R32_UINT;//DXGI_FORMAT_R16_UINT;
    mBoxGeo->IndexBufferByteSize = ibByteSize;

    SubMeshGeometry subMesh;
    subMesh.IndexCount = (UINT)indices.size();
    subMesh.StartIndexLocation = 0;
    subMesh.BaseVertexLocation = 0;

    mBoxGeo->DrawArgs["box"] = subMesh;
}

void Meow::BuildPSO() {
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = mRootSignature.Get();
    psoDesc.VS = { reinterpret_cast<BYTE*>(mvsByteCode->GetBufferPointer()), mvsByteCode->GetBufferSize() };
    psoDesc.PS = { reinterpret_cast<BYTE*>(mpsByteCode->GetBufferPointer()), mpsByteCode->GetBufferSize() };
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    CD3DX12_RASTERIZER_DESC rsDesc(D3D12_DEFAULT);
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.InputLayout = { mInputLayoutDesc.data(), (UINT)mInputLayoutDesc.size()};
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = mBackBufferFormat;
    psoDesc.DSVFormat = mDepthStencilFormat;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
    psoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;

    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSO)));
}

std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> Meow::GetStaticSamplers()
{
    // Applications usually only need a handful of samplers.  So just define them all up front
    // and keep them available as part of the root signature.  

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