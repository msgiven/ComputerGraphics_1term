#pragma once
#include <d3dx12.h>
#include "d3dUtil.h"
#include "UploadBuffer.h"
using Microsoft::WRL::ComPtr;
class PBR
{
public:

	PBR(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, GBuffer* gbuffer) {
		CreateTextures(device, cmdList, gbuffer);
	}
	void CreateTextures(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, GBuffer* gbuffer);

private:
	ComPtr<ID3D12Resource> integrationMap = nullptr;
	ComPtr<ID3D12Resource> irradianceMap = nullptr;
	ComPtr<ID3D12Resource> preFilteredEnvMap = nullptr;

	ComPtr<ID3D12Resource> integrationMapUpload;
	ComPtr<ID3D12Resource> irradianceMapUpload;
	ComPtr<ID3D12Resource> preFilteredEnvMapUpload;
};

