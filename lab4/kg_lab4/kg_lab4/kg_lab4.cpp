#include "D3DApp.h"
#include <DirectXColors.h> 
#include "d3dUtil.h"
#include "MathHelper.h"
#include "UploadBuffer.h"

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

struct ObjectConstants {
    XMFLOAT4X4 WorldViewProj = MathHelper::Identity4x4();
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

    void BuildBoxGeometry();
    void BuildDescriptorHeaps();
    void BuildConstantBuffers();
    void BuildRootSignature();
    void BuildShadersAndInputLayout();
    void BuildPSO();

    std::unique_ptr<MeshGeometry> mBoxGeo = nullptr;

    float mTheta = 1.5f * XM_PI;
    float mPhi = XM_PIDIV4;
    float mRadius = 105.0f;
    XMFLOAT4X4 mWorld = MathHelper::Identity4x4();
    XMFLOAT4X4 mView = MathHelper::Identity4x4();
    XMFLOAT4X4 mProj = MathHelper::Identity4x4();
    POINT mLastMousePos;
    const float mousSen = 0.25f;

    std::unique_ptr<UploadBuffer<ObjectConstants>> mObjectCB = nullptr;
    ComPtr<ID3D12DescriptorHeap> mCbvHeap;

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
		
    // Reset the command list to prep for initialization commands.
    ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));
 
    BuildDescriptorHeaps();
	BuildConstantBuffers();
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

    XMVECTOR pos = XMVectorSet(x, y, z, 1);
    XMVECTOR target = XMVectorZero();
    XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
    XMStoreFloat4x4(&mView, view);

    XMMATRIX world = XMLoadFloat4x4(&mWorld);
    XMMATRIX proj = XMLoadFloat4x4(&mProj);

    XMMATRIX worldViewProj = world * view * proj;

    ObjectConstants objConst;
    //XMStoreFloat4x4(&objConst.WorldViewProj, worldViewProj);
    XMStoreFloat4x4(&objConst.WorldViewProj, XMMatrixTranspose(worldViewProj));
    mObjectCB->CopyData(0, objConst);
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
    ID3D12DescriptorHeap* descriptorHeaps[] = { mCbvHeap.Get() };
    mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
    CD3DX12_GPU_DESCRIPTOR_HANDLE cbv(mCbvHeap->GetGPUDescriptorHandleForHeapStart());
    mCommandList->SetGraphicsRootDescriptorTable(0, cbv);

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
        mRadius = MathHelper::Clamp(mRadius, 3.0f, 115.0f);
    }

    mLastMousePos.x = x;
    mLastMousePos.y = y;
}

void Meow::OnMouseUp(WPARAM btnState, int x, int y) {
    ReleaseCapture();
}

void Meow::BuildRootSignature() {
    CD3DX12_ROOT_PARAMETER slotRootParameter[1];

    CD3DX12_DESCRIPTOR_RANGE cbvTable;
    cbvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);

    slotRootParameter[0].InitAsDescriptorTable(1, &cbvTable);

    CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(1, slotRootParameter, 0, nullptr,
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    ComPtr<ID3DBlob> serializedRootSig = nullptr;
    ComPtr<ID3DBlob> errorRootSig = nullptr;

    HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, 
        serializedRootSig.GetAddressOf(), errorRootSig.GetAddressOf());

    ThrowIfFailed(md3dDevice->CreateRootSignature(0, serializedRootSig->GetBufferPointer(),
        serializedRootSig->GetBufferSize(), IID_PPV_ARGS(&mRootSignature)));
}

void Meow::BuildDescriptorHeaps() {
    D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc;
    cbvHeapDesc.NumDescriptors = 1;
    cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    cbvHeapDesc.NodeMask = 0;

    ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(&mCbvHeap)));
}

void Meow::BuildConstantBuffers() {
    mObjectCB = std::make_unique<UploadBuffer<ObjectConstants>>(md3dDevice.Get(), 1, true);

    UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
    D3D12_GPU_VIRTUAL_ADDRESS cbAdress = mObjectCB->Resource()->GetGPUVirtualAddress();
    //int boxCBufIndex = 0;
    //cbAdress += boxCBufIndex * objCBByteSize;

    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
    cbvDesc.BufferLocation = cbAdress;
    cbvDesc.SizeInBytes = objCBByteSize;

    md3dDevice->CreateConstantBufferView(&cbvDesc, mCbvHeap->GetCPUDescriptorHandleForHeapStart());
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

void Meow::BuildBoxGeometry() {

    /*std::array<Vertex, 8> vertices =
    {
        Vertex({ XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT4(Colors::White) }),
        Vertex({ XMFLOAT3(-1.0f, +1.0f, -1.0f), XMFLOAT4(Colors::Black) }),
        Vertex({ XMFLOAT3(+1.0f, +1.0f, -1.0f), XMFLOAT4(Colors::Red) }),
        Vertex({ XMFLOAT3(+1.0f, -1.0f, -1.0f), XMFLOAT4(Colors::Green) }),
        Vertex({ XMFLOAT3(-1.0f, -1.0f, +1.0f), XMFLOAT4(Colors::Blue) }),
        Vertex({ XMFLOAT3(-1.0f, +1.0f, +1.0f), XMFLOAT4(Colors::Yellow) }),
        Vertex({ XMFLOAT3(+1.0f, +1.0f, +1.0f), XMFLOAT4(Colors::Cyan) }),
        Vertex({ XMFLOAT3(+1.0f, -1.0f, +1.0f), XMFLOAT4(Colors::Magenta) })
    };

    std::array<std::uint16_t, 36> indices =
    {
        // front face
        0, 1, 2,
        0, 2, 3,

        // back face
        4, 6, 5,
        4, 7, 6,

        // left face
        4, 5, 1,
        4, 1, 0,

        // right face
        3, 2, 6,
        3, 6, 7,

        // top face
        1, 5, 6,
        1, 6, 2,

        // bottom face
        4, 0, 3,
        4, 3, 7
    };*/
    std::string fileName = "sponza.obj";
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    bool load = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, fileName.c_str());
    if (!warn.empty()) {
        OutputDebugStringA(warn.c_str());
    }
    if (!err.empty()) {
        OutputDebugStringA(err.c_str());
    }
    if (!load) {
        MessageBoxA(nullptr, "Failed to load/parse .obj", "Error", MB_OK);
        return;
    }

    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;

    for (const auto& shape : shapes) {
        for (const auto& index : shape.mesh.indices) {
            Vertex v;

            v.Pos.x = attrib.vertices[3 * index.vertex_index + 0];
            v.Pos.y = attrib.vertices[3 * index.vertex_index + 1];
            v.Pos.z = attrib.vertices[3 * index.vertex_index + 2];

            v.Pos.x *= 0.01f; 
            v.Pos.y *= 0.01f; 
            v.Pos.z *= 0.01f;

            if (index.normal_index >= 0) {
                v.Normal.x = attrib.normals[3 * index.normal_index + 0];
                v.Normal.y = attrib.normals[3 * index.normal_index + 1];
                v.Normal.z = attrib.normals[3 * index.normal_index + 2];
            }
            else {
                v.Normal = { 0.0f,1.0f,0.0f };
            }

            if (index.texcoord_index >= 0) {
                v.TexC.x = attrib.texcoords[2 * index.texcoord_index + 0];
                v.TexC.y = attrib.texcoords[2 * index.texcoord_index + 1];
            }
            else {
                v.TexC = { 0.0f, 0.0f };
            }

           // v.Color = XMFLOAT4(0.8f, 0.7f, 0.6f, 1.0f);
            vertices.push_back(v);
            indices.push_back((std::uint32_t)indices.size());
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