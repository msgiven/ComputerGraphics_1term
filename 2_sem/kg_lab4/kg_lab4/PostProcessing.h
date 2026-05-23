#pragma once
#include <d3dx12.h>
#include "d3dUtil.h"
#include "UploadBuffer.h"
using Microsoft::WRL::ComPtr;

class PostProcessing
{
public:
	ComPtr<ID3D12Resource> pTexture = nullptr;
	ComPtr<ID3D12DescriptorHeap> pRtvDescriptorHeap = nullptr;
	ComPtr<ID3D12DescriptorHeap> pSrvDescriptorHeap = nullptr;

	D3D12_GPU_DESCRIPTOR_HANDLE pSrvBaseGpuHandle;

	D3D12_CPU_DESCRIPTOR_HANDLE pTexRtv;



	PostProcessing(ID3D12Device* device, int width, int height, GBuffer* gbuffer) {

		this->device = device;


		D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
		rtvHeapDesc.NumDescriptors = 1;
		rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		ThrowIfFailed(device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&pRtvDescriptorHeap)));

		D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
		srvHeapDesc.NumDescriptors = 2;
		srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		ThrowIfFailed(device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&pSrvDescriptorHeap)));


		auto heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		auto resDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R16G16B16A16_FLOAT, width, height, 
			1, 0, 1, 1, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);

		D3D12_CLEAR_VALUE optCV = {};
		optCV.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		optCV.Color[0] = 0.0f; optCV.Color[1] = 0.0f; optCV.Color[2] = 0.0f; optCV.Color[3] = 1.0f;

		device->CreateCommittedResource(&heapProp, D3D12_HEAP_FLAG_NONE, &resDesc,
			D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE, &optCV, IID_PPV_ARGS(&pTexture));

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = 1;
		srvDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;

		pSrvBaseGpuHandle = pSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
		UINT srvSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(pSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
		device->CreateShaderResourceView(pTexture.Get(), &srvDesc, srvHandle);

		srvHandle.Offset(1, srvSize);

		CD3DX12_CPU_DESCRIPTOR_HANDLE gbufferSrvStart(gbuffer->mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
		gbufferSrvStart.Offset(2, srvSize);

		device->CopyDescriptorsSimple(1, srvHandle, gbufferSrvStart, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(pRtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
		UINT rtvSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		rtvDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		device->CreateRenderTargetView(pTexture.Get(), &rtvDesc, rtvHandle);
		pTexRtv = rtvHandle;
	}
	void TransitToShaderWriteState(ComPtr<ID3D12GraphicsCommandList>& cmdList) {
		D3D12_RESOURCE_BARRIER barriers[1] = {
			CD3DX12_RESOURCE_BARRIER::Transition(pTexture.Get(), D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET),
		};
		cmdList->ResourceBarrier(1, barriers);
	}

	void TransitToShaderReadState(ComPtr<ID3D12GraphicsCommandList>& cmdList) {
		D3D12_RESOURCE_BARRIER barriers[1] = {
			CD3DX12_RESOURCE_BARRIER::Transition(pTexture.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE),
		};
		cmdList->ResourceBarrier(1, barriers);
	}

private:
	ComPtr<ID3D12Device> device;

};

