#pragma once

#include <windows.h>
#include <wrl.h>
#include <dxgi1_4.h>
#include <d3d12.h>
#include <D3Dcompiler.h>
#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <DirectXColors.h>
#include <DirectXCollision.h>
#include <string>
#include <memory>
#include <algorithm>
#include <vector>
#include <array>
#include <unordered_map>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <cassert>
#include "d3dx12.h"
#include "DDSTextureLoader.h"
#include "MathHelper.h"
		 


using namespace DirectX;
using namespace DirectX::PackedVector;
using Microsoft::WRL::ComPtr;

const int MAX_LIGHT_AMOUNT = 10;

inline void ThrowIfFailed(HRESULT hr)
{
	if (FAILED(hr))
	{
		throw std::exception("DirectX call failed");
	}
}

struct Texture {
	std::string name;
	std::wstring fileName;
	XMFLOAT4X4 texTransform = MathHelper::Identity4x4();
	ComPtr<ID3D12Resource> resource = nullptr;
	ComPtr<ID3D12Resource> uploadHeap = nullptr;
};

struct Material {
	std::string name;
	int matCBIndex = -1;
	int DiffuseSrvHeapIndex = -1;
	int numFramesDirty = 1;
	XMFLOAT4 diffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
	XMFLOAT3 fresnelRO = { 0.1f,0.1f,0.1f };
	float roughness = 0.25f;
	XMFLOAT4X4 matTransform = MathHelper::Identity4x4();
};

struct MaterialConstants {
	DirectX::XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
	DirectX::XMFLOAT3 FresnelR0 = { 0.01f, 0.01f, 0.01f	};
	float Roughness = 0.25f;
	DirectX::XMFLOAT4X4 MatTransform = MathHelper::Identity4x4();
};

struct ObjectConstants {
	XMFLOAT4X4 WorldViewProj = MathHelper::Identity4x4();

};

struct Light {

	XMFLOAT3 Strength = { 0.5f, 0.5f, 0.5f };
	float FalloffStart = 1.0f;
	XMFLOAT3 Direction = { 0.0f, -1.0f, 0.0f };
	float FalloffEnd = 10.0f;
	XMFLOAT3 Position = { 0.0f, 0.0f, 0.0f };
	float SpotPower = 64.0f;
};


struct PassConstants {
	XMFLOAT4X4 View = MathHelper::Identity4x4();
	XMFLOAT4X4 InvView = MathHelper::Identity4x4();
	XMFLOAT4X4 Proj = MathHelper::Identity4x4();
	XMFLOAT4X4 InvProj = MathHelper::Identity4x4();
	XMFLOAT4X4 ViewProj = MathHelper::Identity4x4();
	XMFLOAT4X4 InvViewProj = MathHelper::Identity4x4();
	XMFLOAT3 EyePosW = { 0.0f, 0.0f, 0.0f };
	float cbPerObjectPad1 = 0.0f;
	XMFLOAT2 RenderTargetSize = { 0.0f, 0.0f };
	XMFLOAT2 InvRenderTargetSize = { 0.0f, 0.0f };
	float NearZ = 0.0f;
	float FarZ = 0.0f;
	float TotalTime = 0.0f;
	float DeltaTime = 0.0f;

	XMFLOAT4 AmbientLight = { 0.0f, 0.0f, 0.0f, 1.0f };

	Light Lights[MAX_LIGHT_AMOUNT];
};

struct GBuffer {
	ComPtr<ID3D12Resource> DiffuseTex = nullptr;
	ComPtr<ID3D12Resource> NormalTex = nullptr;
	ComPtr<ID3D12Resource> Pos = nullptr;

	// Вместо Diligent используем стандартные кучи
	ComPtr<ID3D12DescriptorHeap> mSrvDescriptorHeap = nullptr;
	ComPtr<ID3D12DescriptorHeap> mRtvDescriptorHeap = nullptr;

	// Храним базовый GPU-хендл для передачи в шейдер (SetGraphicsRootDescriptorTable)
	D3D12_GPU_DESCRIPTOR_HANDLE mSrvBaseGpuHandle;

	D3D12_CPU_DESCRIPTOR_HANDLE DiffRTV;
	D3D12_CPU_DESCRIPTOR_HANDLE NormalRTV;
	D3D12_CPU_DESCRIPTOR_HANDLE PosRTV;

	GBuffer(ID3D12Device* device, int width, int height) {
		D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
		rtvHeapDesc.NumDescriptors = 3;
		rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		ThrowIfFailed(device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&mRtvDescriptorHeap)));

		// 2. Создание SRV кучи (замена Diligent)
		D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
		srvHeapDesc.NumDescriptors = 3;
		srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE; // Важно для использования в шейдерах
		ThrowIfFailed(device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mSrvDescriptorHeap)));

		mSrvBaseGpuHandle = mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart();

		UINT srvSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

		// 3. Создание ресурсов (текстур)
		auto heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		auto resDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, width, height, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);

		D3D12_CLEAR_VALUE optCV = {};
		optCV.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		optCV.Color[0] = 0.0f; optCV.Color[1] = 0.0f; optCV.Color[2] = 0.0f; optCV.Color[3] = 1.0f;

		device->CreateCommittedResource(&heapProp, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE, &optCV, IID_PPV_ARGS(&DiffuseTex));

		resDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		optCV.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		device->CreateCommittedResource(&heapProp, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE, &optCV, IID_PPV_ARGS(&NormalTex));

		resDesc.Format = DXGI_FORMAT_R32_FLOAT;
		optCV.Format = DXGI_FORMAT_R32_FLOAT;
		device->CreateCommittedResource(&heapProp, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE, &optCV, IID_PPV_ARGS(&Pos));

		// 4. Создание SRV (Shader Resource Views)
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = 1;

		srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		device->CreateShaderResourceView(DiffuseTex.Get(), &srvDesc, srvHandle);

		srvDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		srvHandle.Offset(1, srvSize);
		device->CreateShaderResourceView(NormalTex.Get(), &srvDesc, srvHandle);

		srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
		srvHandle.Offset(1, srvSize);
		device->CreateShaderResourceView(Pos.Get(), &srvDesc, srvHandle);

		// 5. Создание RTV (Render Target Views)
		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(mRtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
		UINT rtvSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

		rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		device->CreateRenderTargetView(DiffuseTex.Get(), &rtvDesc, rtvHandle);
		DiffRTV = rtvHandle;

		rtvDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		rtvHandle.Offset(1, rtvSize);
		device->CreateRenderTargetView(NormalTex.Get(), &rtvDesc, rtvHandle);
		NormalRTV = rtvHandle;

		rtvDesc.Format = DXGI_FORMAT_R32_FLOAT;
		rtvHandle.Offset(1, rtvSize);
		device->CreateRenderTargetView(Pos.Get(), &rtvDesc, rtvHandle);
		PosRTV = rtvHandle;
	}

	void TransitToOpaqueRenderingState(ComPtr<ID3D12GraphicsCommandList>& cmdList) {
		D3D12_RESOURCE_BARRIER barriers[3] = {
			CD3DX12_RESOURCE_BARRIER::Transition(DiffuseTex.Get(), D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET),
			CD3DX12_RESOURCE_BARRIER::Transition(NormalTex.Get(), D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET),
			CD3DX12_RESOURCE_BARRIER::Transition(Pos.Get(), D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET)
		};
		cmdList->ResourceBarrier(3, barriers);
	}

	void TransitToLightRenderingState(ComPtr<ID3D12GraphicsCommandList>& cmdList) {
		D3D12_RESOURCE_BARRIER barriers[3] = {
			CD3DX12_RESOURCE_BARRIER::Transition(DiffuseTex.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE),
			CD3DX12_RESOURCE_BARRIER::Transition(NormalTex.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE),
			CD3DX12_RESOURCE_BARRIER::Transition(Pos.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE)
		};
		cmdList->ResourceBarrier(3, barriers);
	}
	void OnResize(int width, int height) {

	}
};


class d3dUtil
{
public:
	static ComPtr<ID3DBlob> CompileShader(
		const std::wstring& filename,
		const D3D_SHADER_MACRO* defines,
		const std::string& entrypoint,
		const std::string& target);

	static ComPtr<ID3D12Resource> CreateDefaultBuffer(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList,
		const void* initData, UINT64 dataSize, ComPtr<ID3D12Resource>& uploadBuffer);

	static ComPtr<ID3DBlob> LoadBinary(const std::wstring& filename);

	static UINT CalcConstantBufferByteSize(UINT byteSize)
	{
		// Constant buffers must be a multiple of the minimum hardware
		// allocation size (usually 256 bytes).  So round up to nearest
		// multiple of 256.  We do this by adding 255 and then masking off
		// the lower 2 bytes which store all bits < 256.
		// Example: Suppose byteSize = 300.
		// (300 + 255) & ~255
		// 555 & ~255
		// 0x022B & ~0x00ff
		// 0x022B & 0xff00
		// 0x0200
		// 512
		return (byteSize + 255) & ~255;
	}
};

struct SubMeshGeometry {
	UINT IndexCount = 0;
	UINT StartIndexLocation = 0;
	UINT BaseVertexLocation = 0;
};

struct MeshGeometry {

	std::string Name;

	ComPtr<ID3DBlob> VertexBufferCPU = nullptr;
	ComPtr<ID3DBlob> IndexBufferCPU = nullptr;

	ComPtr<ID3D12Resource> VertexBufferGPU = nullptr;
	ComPtr<ID3D12Resource> IndexBufferGPU = nullptr;

	ComPtr<ID3D12Resource> VertexBufferUploader = nullptr;
	ComPtr<ID3D12Resource> IndexBufferUploader = nullptr;

	UINT VertexByteStride = 0;
	UINT VertexBufferByteSize = 0;
	DXGI_FORMAT IndexFormat = DXGI_FORMAT_R16_UINT;
	UINT IndexBufferByteSize = 0;

	std::unordered_map<std::string, SubMeshGeometry> DrawArgs;

	D3D12_VERTEX_BUFFER_VIEW VertexBufferView()const {
		D3D12_VERTEX_BUFFER_VIEW vbv = {};
		vbv.BufferLocation = VertexBufferGPU->GetGPUVirtualAddress();
		vbv.StrideInBytes = VertexByteStride;
		vbv.SizeInBytes = VertexBufferByteSize;
		return vbv;
	}

	D3D12_INDEX_BUFFER_VIEW IndexBufferView()const {
		D3D12_INDEX_BUFFER_VIEW ibv = {};
		ibv.BufferLocation = IndexBufferGPU->GetGPUVirtualAddress();
		ibv.Format = IndexFormat;
		ibv.SizeInBytes = IndexBufferByteSize;
		return ibv;
	}

	void ReleaseUploaders() {
		VertexBufferUploader = nullptr;
		IndexBufferUploader = nullptr;
	}
};

struct RenderItem {
	RenderItem() = default;

	XMFLOAT4X4 World = MathHelper::Identity4x4();
	XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();

	UINT ObjCBIndex = -1;

	Material* Mat = nullptr;
	MeshGeometry* Geo = nullptr;

	D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	UINT IndexCount = 0;
	UINT StartIndexLocation = 0;
	int BaseVertexLocation = 0;
};