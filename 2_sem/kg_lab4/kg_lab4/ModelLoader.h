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
#include <cstdio>
#include <stdexcept>
#include <iomanip>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include "d3dUtil.h"

using namespace DirectX;
using namespace DirectX::PackedVector;
using Microsoft::WRL::ComPtr;


const std::string DEFAULT_BASE_DIR = "Default//";
const std::string DEFAULT_DIFFUSE_TEX_NAME = "default.dds";
const std::string DEFAULT_NORMAL_TEX_NAME = "default_ddn.dds";
const std::string DEFAULT_HEIGHT_TEX_NAME = "height_ddn.dds";

struct ModelInfo {
	std::string baseDir = DEFAULT_BASE_DIR;
	std::string fileName = "";
	std::string diffuseTexName = DEFAULT_DIFFUSE_TEX_NAME;
	std::string normalTexName = DEFAULT_NORMAL_TEX_NAME;
	std::string heightTexName = DEFAULT_HEIGHT_TEX_NAME;
	std::string geoName = "";
	std::string matNamePrefix = "";
	float dispScale = 0.0f;
	XMFLOAT4X4 world = MathHelper::Identity4x4();
};

class ModelLoader
{
public:

	ModelLoader(ID3D12Device* md3dDevice, ID3D12GraphicsCommandList* mCommandList) {
		device = md3dDevice;
		commandList = mCommandList;
	}

	std::unique_ptr<MeshGeometry> LoadModel(ModelInfo& modelInfo);
	std::unique_ptr<MeshGeometry> LoadModelWithSpecificTextures(ModelInfo& modelInfo);
	std::unique_ptr<MeshGeometry> LoadCube(ModelInfo& modelInfo);
	std::unique_ptr<MeshGeometry> LoadQuad(ModelInfo& modelInfo);
	std::unique_ptr<MeshGeometry> LoadSphere(ModelInfo& modelInfo);
	std::unordered_map<std::string, std::unique_ptr<Texture>>& GetTextures();
	std::unordered_map<std::string, std::unique_ptr<Material>>& GetMaterials();
	std::vector<std::unique_ptr<RenderItem>>& GetAllRitems();

	bool LoadTexture(const std::string& baseDir, const std::string& texName) {
		if (texName.empty()) {
			return false;
		}
		if (textures.find(texName) != textures.end()) {
			return textures[texName] && textures[texName]->resource != nullptr;
		}

		auto tex = std::make_unique<Texture>();

		std::string fullPath = baseDir + texName;
		std::wstring wfullPath(fullPath.begin(), fullPath.end());

		HRESULT hr = DirectX::CreateDDSTextureFromFile12(device.Get(), commandList.Get(), wfullPath.c_str(), tex->resource, tex->uploadHeap);
		if (FAILED(hr)) {
			return false;
		}

		tex->name = texName;
		textures[texName] = std::move(tex);
		return true;
	}

private:
	struct Vertex
	{
		XMFLOAT3 Pos;
		// XMFLOAT4 Color;
		XMFLOAT3 Normal;
		XMFLOAT2 TexC;
		XMFLOAT3 TangentU;
	};

	int srvHeapIndex = 0;
	ComPtr<ID3D12Device> device;
	ComPtr<ID3D12GraphicsCommandList> commandList;
	std::unordered_map<std::string, std::unique_ptr<Texture>> textures;
	std::unordered_map<std::string, std::unique_ptr<Material>> materials;
	std::vector<std::unique_ptr<RenderItem>> allRitems;

	ComPtr<ID3D12Resource> CreateDefaultBuffer(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList,
		const void* initData, UINT64 dataSize, ComPtr<ID3D12Resource>& uploadBuffer);

	const std::string defaultDiffuseTexName = "background.dds";
	const std::string defaultNormalTexName = "background_ddn.dds";
	const std::string defaultHeightTexName = defaultDiffuseTexName;

	Assimp::Importer importer;

	std::unique_ptr<MeshGeometry> ProcessSceneMeshes(const aiScene* scene,
		const std::string& geoName,
		const std::string& matNamePrefix,
		const XMMATRIX& worldTransform);

	void FinalizeGeometry(MeshGeometry* geo,
		const std::vector<Vertex>& vertices,
		const std::vector<std::uint32_t>& indices);
};

