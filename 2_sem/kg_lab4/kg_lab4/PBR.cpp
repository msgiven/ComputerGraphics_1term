#include "PBR.h"

void PBR::CreateTextures(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, GBuffer* gbuffer) {

    std::wstring integrationPath = L"pbr\\IntegrationMap.dds";
    std::wstring irradiancePath = L"pbr\\IrradianceMap_BC6U.dds";
    std::wstring prefilterPath = L"pbr\\PreFilteredEnvMap_BC6U.dds";


    ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(device, cmdList,
        integrationPath.c_str(), integrationMap, integrationMapUpload));

    ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(device, cmdList,
        irradiancePath.c_str(), irradianceMap, irradianceMapUpload));

    ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(device, cmdList,
        prefilterPath.c_str(), preFilteredEnvMap, preFilteredEnvMapUpload));



    auto srvHandle = gbuffer->mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
    UINT incSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    srvHandle.ptr += 7 * incSize;

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = irradianceMap->GetDesc().Format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
    srvDesc.TextureCube.MostDetailedMip = 0;
    srvDesc.TextureCube.MipLevels = irradianceMap->GetDesc().MipLevels;
    device->CreateShaderResourceView(irradianceMap.Get(), &srvDesc, srvHandle);

    srvHandle.ptr += incSize;

    srvDesc.Format = preFilteredEnvMap->GetDesc().Format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE; 
    srvDesc.TextureCube.MipLevels = preFilteredEnvMap->GetDesc().MipLevels;
    device->CreateShaderResourceView(preFilteredEnvMap.Get(), &srvDesc, srvHandle);


    srvHandle.ptr += incSize;

    DXGI_FORMAT lutFormat = integrationMap->GetDesc().Format;
    if (lutFormat == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB) lutFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    else if (lutFormat == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB) lutFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
    srvDesc.Format = lutFormat;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D; 
    srvDesc.Texture2D.MipLevels = 1;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.PlaneSlice = 0;
    srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
    device->CreateShaderResourceView(integrationMap.Get(), &srvDesc, srvHandle);
}