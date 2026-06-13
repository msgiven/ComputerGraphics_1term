#include "ModelLoader.h"
#include <assimp/material.h>



std::unordered_map<std::string, std::unique_ptr<Texture>>& ModelLoader::GetTextures() {
    return textures;
}

std::unordered_map<std::string, std::unique_ptr<Material>>& ModelLoader::GetMaterials() {
    return materials;
}

std::vector<std::unique_ptr<RenderItem>>& ModelLoader::GetAllRitems() {
    return allRitems;
}

ComPtr<ID3D12Resource> ModelLoader::CreateDefaultBuffer(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList,
    const void* initData, UINT64 dataSize, ComPtr<ID3D12Resource>& uploadBuffer) {

    ComPtr<ID3D12Resource> defaultBuffer;

    CD3DX12_HEAP_PROPERTIES heapPropDef = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    CD3DX12_RESOURCE_DESC resDescSize = CD3DX12_RESOURCE_DESC::Buffer(dataSize);
    ThrowIfFailed(device->CreateCommittedResource(&heapPropDef, D3D12_HEAP_FLAG_NONE, &resDescSize,
        D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(defaultBuffer.GetAddressOf())));

    CD3DX12_HEAP_PROPERTIES heapPropUp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    ThrowIfFailed(device->CreateCommittedResource(&heapPropUp, D3D12_HEAP_FLAG_NONE, &resDescSize,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(uploadBuffer.GetAddressOf())));

    D3D12_SUBRESOURCE_DATA subResData = {};
    subResData.pData = initData;
    subResData.RowPitch = dataSize;
    subResData.SlicePitch = dataSize;

    CD3DX12_RESOURCE_BARRIER barToCopy = CD3DX12_RESOURCE_BARRIER::Transition(defaultBuffer.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
    cmdList->ResourceBarrier(1, &barToCopy);

    UpdateSubresources<1>(cmdList, defaultBuffer.Get(), uploadBuffer.Get(), 0, 0, 1, &subResData);

    CD3DX12_RESOURCE_BARRIER barToRead = CD3DX12_RESOURCE_BARRIER::Transition(defaultBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ);
    cmdList->ResourceBarrier(1, &barToRead);

    return defaultBuffer;
}


std::unique_ptr<MeshGeometry> ModelLoader::ProcessSceneMeshes(const aiScene* scene,
    const std::string& geoName, const std::string& matNamePrefix, const XMMATRIX& worldTransform) {

    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;

    auto meshGeo = std::make_unique<MeshGeometry>();
    meshGeo->Name = geoName;

    for (unsigned int m = 0; m < scene->mNumMeshes; ++m) {
        aiMesh* mesh = scene->mMeshes[m];

        SubMeshGeometry subMesh;
        subMesh.BaseVertexLocation = (UINT)vertices.size();
        subMesh.StartIndexLocation = (UINT)indices.size();
        subMesh.IndexCount = mesh->mNumFaces * 3;

        XMFLOAT3 minPos = { std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max() };
        XMFLOAT3 maxPos = { -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max() };

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
            minPos.x = std::min(minPos.x, v.Pos.x);
            minPos.y = std::min(minPos.y, v.Pos.y);
            minPos.z = std::min(minPos.z, v.Pos.z);
            maxPos.x = std::max(maxPos.x, v.Pos.x);
            maxPos.y = std::max(maxPos.y, v.Pos.y);
            maxPos.z = std::max(maxPos.z, v.Pos.z);
            vertices.push_back(v);
        }

        for (unsigned int i = 0; i < mesh->mNumFaces; ++i) {
            indices.push_back(mesh->mFaces[i].mIndices[0]);
            indices.push_back(mesh->mFaces[i].mIndices[1]);
            indices.push_back(mesh->mFaces[i].mIndices[2]);
        }


        aiMaterial* aiMat = scene->mMaterials[mesh->mMaterialIndex];
        aiString mName; aiMat->Get(AI_MATKEY_NAME, mName);
        std::string uniqueName = matNamePrefix + mName.C_Str() + std::string("_") + std::to_string(mesh->mMaterialIndex);

        auto ritem = std::make_unique<RenderItem>();
        XMStoreFloat4x4(&ritem->World, worldTransform);
        ritem->ObjCBIndex = static_cast<UINT>(allRitems.size());
        ritem->Geo = meshGeo.get();
        ritem->IndexCount = subMesh.IndexCount;
        ritem->StartIndexLocation = subMesh.StartIndexLocation;
        ritem->BaseVertexLocation = subMesh.BaseVertexLocation;
        ritem->LocalBounds.Center = {
            (minPos.x + maxPos.x) * 0.5f,
            (minPos.y + maxPos.y) * 0.5f,
            (minPos.z + maxPos.z) * 0.5f
        };
        ritem->LocalBounds.Extents = {
            (maxPos.x - minPos.x) * 0.5f,
            (maxPos.y - minPos.y) * 0.5f,
            (maxPos.z - minPos.z) * 0.5f
        };
        ritem->Mat = materials[uniqueName].get();
        if (ritem->Mat == nullptr) {
            std::string errorMsg = "Material not found for: " + uniqueName;
            MessageBoxA(nullptr, errorMsg.c_str(), "Error", MB_OK);
            __debugbreak();
        }
        allRitems.push_back(std::move(ritem));
    }

    FinalizeGeometry(meshGeo.get(), vertices, indices);
    return meshGeo;
}

void ModelLoader::FinalizeGeometry(MeshGeometry* geo,
    const std::vector<Vertex>& vertices,
    const std::vector<std::uint32_t>& indices)
{
    const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
    const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint32_t);

    geo->VertexBufferGPU = CreateDefaultBuffer(device.Get(), commandList.Get(),
        vertices.data(), vbByteSize, geo->VertexBufferUploader);
    geo->IndexBufferGPU = CreateDefaultBuffer(device.Get(), commandList.Get(),
        indices.data(), ibByteSize, geo->IndexBufferUploader);

    geo->VertexByteStride = sizeof(Vertex);
    geo->VertexBufferByteSize = vbByteSize;
    geo->IndexFormat = DXGI_FORMAT_R32_UINT;
    geo->IndexBufferByteSize = ibByteSize;
}


std::unique_ptr<MeshGeometry> ModelLoader::LoadModel(ModelInfo& modelInfo) {

    const aiScene* model = importer.ReadFile(modelInfo.baseDir + modelInfo.fileName,
        aiProcess_Triangulate |
        aiProcess_GenSmoothNormals |
        aiProcess_FlipUVs |
        aiProcess_JoinIdenticalVertices | aiProcess_CalcTangentSpace);

    if (!model || !model->mRootNode) {
        MessageBoxA(nullptr, "Could not load Sponza. Check paths!", "Error", MB_OK);
        return nullptr;
    }

    for (unsigned int i = 0; i < model->mNumMaterials; ++i) {
        aiMaterial* aiMat = model->mMaterials[i];
        aiString matName;
        aiMat->Get(AI_MATKEY_NAME, matName);
        std::string name = modelInfo.matNamePrefix + matName.C_Str() + std::string("_") + std::to_string(i);

        auto mat = std::make_unique<Material>();
        mat->fresnelRO = { 0.04f,0.04f,0.04f };
        mat->roughness = 0.15f;
        mat->name = name;
        mat->matCBIndex = i;
        mat->DiffuseSrvHeapIndex = srvHeapIndex;
        mat->NormalSrvHeapIndex = srvHeapIndex + 1;
        mat->HeightSrvHeapIndex = srvHeapIndex + 2;
        srvHeapIndex += 3;
        std::string diffuseTexName = defaultDiffuseTexName;

        if (aiMat->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
            aiString texPath;
            aiMat->GetTexture(aiTextureType_DIFFUSE, 0, &texPath);
            diffuseTexName = texPath.C_Str();
            size_t lastDot = diffuseTexName.find_last_of(".");
            if (lastDot != std::string::npos) diffuseTexName = diffuseTexName.substr(0, lastDot) + ".dds";
        }
        if (!LoadTexture(modelInfo.baseDir, diffuseTexName)) {
            diffuseTexName = defaultDiffuseTexName;
            LoadTexture(modelInfo.baseDir, diffuseTexName);
        }

        std::string normalTexName = defaultNormalTexName;
        {
            aiString normalPath;
            const aiTextureType normalSlots[] = {
                aiTextureType_HEIGHT,
                static_cast<aiTextureType>(8),
                aiTextureType_DISPLACEMENT,
                aiTextureType_NORMALS,
                aiTextureType_UNKNOWN
            };
            for (aiTextureType slot : normalSlots) {
                if (aiMat->GetTexture(slot, 0, &normalPath) == AI_SUCCESS) {
                    normalTexName = normalPath.C_Str();
                    break;
                }
            }
            size_t nLastDot = normalTexName.find_last_of(".");
            if (nLastDot != std::string::npos) normalTexName = normalTexName.substr(0, nLastDot) + ".dds";
            if (!LoadTexture(modelInfo.baseDir, normalTexName)) {
                normalTexName = defaultNormalTexName;
                if (!LoadTexture(modelInfo.baseDir, normalTexName)) {
                    normalTexName = diffuseTexName;
                }
            }
        }

        mat->DiffuseMapName = diffuseTexName;
        mat->NormalMapName = normalTexName;
        mat->HeightMapName = defaultHeightTexName;
        mat->dispScale = 0.0f;
        materials[name] = std::move(mat);
    }

    return ProcessSceneMeshes(model, modelInfo.geoName, modelInfo.matNamePrefix, XMLoadFloat4x4(&modelInfo.world));
}



std::unique_ptr<MeshGeometry> ModelLoader::LoadModelWithSpecificTextures(ModelInfo& modelInfo) {

    std::string baseDirStone = modelInfo.baseDir;
    std::string fileNameStone = baseDirStone + modelInfo.fileName;

    const aiScene* model = importer.ReadFile(fileNameStone,
        aiProcess_Triangulate |
        aiProcess_GenSmoothNormals |
        aiProcess_FlipUVs |
        aiProcess_JoinIdenticalVertices | aiProcess_CalcTangentSpace);

    if (!model || !model->mRootNode) {
        std::string error = importer.GetErrorString();
        MessageBoxA(nullptr, error.c_str(), "Assimp Error", MB_OK);
        return nullptr;
    }

    LoadTexture(baseDirStone, modelInfo.diffuseTexName);
    LoadTexture(baseDirStone, modelInfo.normalTexName);
    LoadTexture(baseDirStone, modelInfo.heightTexName);

    for (unsigned int i = 0; i < model->mNumMaterials; ++i) {
        aiMaterial* aiMat = model->mMaterials[i];
        aiString matName;
        aiMat->Get(AI_MATKEY_NAME, matName);
        std::string name = modelInfo.matNamePrefix + matName.C_Str() + std::string("_") + std::to_string(i);

        auto mat = std::make_unique<Material>();
        mat->fresnelRO = { 0.08f,0.09f,0.09f };
        mat->roughness = 0.15f;
        mat->name = name;
        mat->matCBIndex = static_cast<int>(materials.size());
        mat->DiffuseSrvHeapIndex = srvHeapIndex;
        mat->NormalSrvHeapIndex = srvHeapIndex + 1;
        mat->HeightSrvHeapIndex = srvHeapIndex + 2;
        srvHeapIndex += 3;


        mat->DiffuseMapName = modelInfo.diffuseTexName;
        mat->NormalMapName = modelInfo.normalTexName;
        mat->HeightMapName = modelInfo.heightTexName;
        mat->dispScale = 0.05f;
        materials[name] = std::move(mat);
    }

    return ProcessSceneMeshes(model, modelInfo.geoName, modelInfo.matNamePrefix, XMLoadFloat4x4(&modelInfo.world));
}

std::unique_ptr<MeshGeometry> ModelLoader::LoadCube(ModelInfo& modelInfo) {

    std::string cubeMatName = "CubeMat";
    auto cubeMat = std::make_unique<Material>();
    cubeMat->fresnelRO = { 0.9f,0.9f,0.9f };
    cubeMat->name = cubeMatName;
    cubeMat->matCBIndex = static_cast<int>(materials.size());
    cubeMat->DiffuseSrvHeapIndex = srvHeapIndex;
    cubeMat->NormalSrvHeapIndex = srvHeapIndex + 1;
    cubeMat->HeightSrvHeapIndex = srvHeapIndex + 2;
    srvHeapIndex += 3;

    LoadTexture(modelInfo.baseDir, modelInfo.diffuseTexName);
    cubeMat->DiffuseMapName = modelInfo.diffuseTexName;
    cubeMat->NormalMapName = modelInfo.normalTexName;
    cubeMat->HeightMapName = modelInfo.heightTexName;
    cubeMat->dispScale = modelInfo.dispScale;

    materials[cubeMatName] = std::move(cubeMat);

    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;

    vertices.push_back({ {-0.5f, -0.5f, 0.5f}, {0,0,1}, {0,1}, {1,0,0} });
    vertices.push_back({ { 0.5f, -0.5f, 0.5f}, {0,0,1}, {1,1}, {1,0,0} });
    vertices.push_back({ { 0.5f,  0.5f, 0.5f}, {0,0,1}, {1,0}, {1,0,0} });
    vertices.push_back({ {-0.5f,  0.5f, 0.5f}, {0,0,1}, {0,0}, {1,0,0} });

    vertices.push_back({ { 0.5f, -0.5f, -0.5f}, {0,0,-1}, {0,1}, {-1,0,0} });
    vertices.push_back({ {-0.5f, -0.5f, -0.5f}, {0,0,-1}, {1,1}, {-1,0,0} });
    vertices.push_back({ {-0.5f,  0.5f, -0.5f}, {0,0,-1}, {1,0}, {-1,0,0} });
    vertices.push_back({ { 0.5f,  0.5f, -0.5f}, {0,0,-1}, {0,0}, {-1,0,0} });

    vertices.push_back({ { 0.5f, -0.5f, 0.5f}, {1,0,0}, {0,1}, {0,0,1} });
    vertices.push_back({ { 0.5f, -0.5f, -0.5f}, {1,0,0}, {1,1}, {0,0,1} });
    vertices.push_back({ { 0.5f,  0.5f, -0.5f}, {1,0,0}, {1,0}, {0,0,1} });
    vertices.push_back({ { 0.5f,  0.5f, 0.5f}, {1,0,0}, {0,0}, {0,0,1} });

    vertices.push_back({ {-0.5f, -0.5f, -0.5f}, {-1,0,0}, {0,1}, {0,0,-1} });
    vertices.push_back({ {-0.5f, -0.5f, 0.5f}, {-1,0,0}, {1,1}, {0,0,-1} });
    vertices.push_back({ {-0.5f,  0.5f, 0.5f}, {-1,0,0}, {1,0}, {0,0,-1} });
    vertices.push_back({ {-0.5f,  0.5f, -0.5f}, {-1,0,0}, {0,0}, {0,0,-1} });

    vertices.push_back({ {-0.5f, 0.5f, 0.5f}, {0,1,0}, {0,1}, {1,0,0} });
    vertices.push_back({ { 0.5f, 0.5f, 0.5f}, {0,1,0}, {1,1}, {1,0,0} });
    vertices.push_back({ { 0.5f, 0.5f, -0.5f}, {0,1,0}, {1,0}, {1,0,0} });
    vertices.push_back({ {-0.5f, 0.5f, -0.5f}, {0,1,0}, {0,0}, {1,0,0} });

    vertices.push_back({ {-0.5f, -0.5f, -0.5f}, {0,-1,0}, {0,1}, {1,0,0} });
    vertices.push_back({ { 0.5f, -0.5f, -0.5f}, {0,-1,0}, {1,1}, {1,0,0} });
    vertices.push_back({ { 0.5f, -0.5f, 0.5f}, {0,-1,0}, {1,0}, {1,0,0} });
    vertices.push_back({ {-0.5f, -0.5f, 0.5f}, {0,-1,0}, {0,0}, {1,0,0} });

    for (UINT face = 0; face < 6; ++face) {
        UINT start = face * 4;
        indices.push_back(start + 0);
        indices.push_back(start + 1);
        indices.push_back(start + 2);
        indices.push_back(start + 0);
        indices.push_back(start + 2);
        indices.push_back(start + 3);
    }


    std::unique_ptr<MeshGeometry> cubeGeo = std::make_unique<MeshGeometry>();
    cubeGeo->Name = "CubeGeo";

    auto cubeRitem = std::make_unique<RenderItem>();
    XMStoreFloat4x4(&cubeRitem->World, XMLoadFloat4x4(&modelInfo.world));
    cubeRitem->ObjCBIndex = static_cast<UINT>(allRitems.size());
    cubeRitem->Geo = cubeGeo.get();
    cubeRitem->IndexCount = 36;
    cubeRitem->StartIndexLocation = 0;
    cubeRitem->BaseVertexLocation = 0;
    cubeRitem->LocalBounds.Center = XMFLOAT3(0.0f, 0.0f, 0.0f);
    cubeRitem->LocalBounds.Extents = XMFLOAT3(0.5f, 0.5f, 0.5f);
    cubeRitem->CastsTexturedShadow = true;
    cubeRitem->Mat = materials[cubeMatName].get();
    allRitems.push_back(std::move(cubeRitem));

    FinalizeGeometry(cubeGeo.get(), vertices, indices);

    return cubeGeo;
}

std::unique_ptr<MeshGeometry> ModelLoader::LoadQuad(ModelInfo& modelInfo) {
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;

    float width = 1.0f;
    float height = 1.0f;

    Vertex v0, v1, v2, v3;
    v0.Pos = { -width, 0.0f, -height };
    v1.Pos = { width, 0.0f, -height };
    v2.Pos = { -width, 0.0f,  height };
    v3.Pos = { width, 0.0f,  height };

    XMFLOAT3 normal = { 0.0f, 1.0f, 0.0f };
    v0.Normal = normal;
    v1.Normal = normal;
    v2.Normal = normal;
    v3.Normal = normal;

    v0.TexC = { 0.0f, 0.0f };
    v1.TexC = { 1.0f, 0.0f };
    v2.TexC = { 0.0f, 1.0f };
    v3.TexC = { 1.0f, 1.0f };

    v0.TangentU = { 1.0f, 0.0f, 0.0f };
    v1.TangentU = { 1.0f, 0.0f, 0.0f };
    v2.TangentU = { 1.0f, 0.0f, 0.0f };
    v3.TangentU = { 1.0f, 0.0f, 0.0f };

    vertices.push_back(v0);
    vertices.push_back(v1);
    vertices.push_back(v2);
    vertices.push_back(v3);

    indices.push_back(0);
    indices.push_back(2);
    indices.push_back(1);

    indices.push_back(1);
    indices.push_back(2);
    indices.push_back(3);


    auto quadMat = std::make_unique<Material>();
    quadMat->matCBIndex = static_cast<UINT>(materials.size());
    quadMat->DiffuseSrvHeapIndex = srvHeapIndex;
    quadMat->NormalSrvHeapIndex = srvHeapIndex + 1;
    quadMat->HeightSrvHeapIndex = srvHeapIndex + 2;
    quadMat->DiffuseMapName = defaultDiffuseTexName;
    quadMat->NormalMapName = defaultNormalTexName;
    quadMat->HeightMapName = defaultHeightTexName;
    quadMat->name = "Water";
    quadMat->diffuseAlbedo = { 0.01f, 0.1f, 0.9f, 1.0f };
    quadMat->fresnelRO = { 0.2f, 0.2f, 0.2f };
    quadMat->roughness = 0.01f;
    materials[quadMat->name] = std::move(quadMat);
    srvHeapIndex += 3;

    std::unique_ptr<MeshGeometry> quadGeo = std::make_unique<MeshGeometry>();
    quadGeo->Name = "Quad";

    auto quadRitem = std::make_unique<RenderItem>();
    quadRitem->World = MathHelper::Identity4x4();
    XMStoreFloat4x4(&quadRitem->World, XMMatrixScaling(100.1f, 1.0f, 100.1f) * XMMatrixTranslation(0.0f, 5.0f, 0.0f));
    quadRitem->ObjCBIndex = static_cast<UINT>(allRitems.size());
    quadRitem->Geo = quadGeo.get();
    quadRitem->Mat = materials["Water"].get();
    quadRitem->IndexCount = static_cast<UINT>(indices.size());
    quadRitem->StartIndexLocation = 0;
    quadRitem->BaseVertexLocation = 0;
    quadRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST;
    quadRitem->LocalBounds.Center = { 0.0f, 0.0f, 0.0f };
    quadRitem->LocalBounds.Extents = { width, 0.05f, height };
    allRitems.push_back(std::move(quadRitem));

    FinalizeGeometry(quadGeo.get(), vertices, indices);
    return quadGeo;
}

std::unique_ptr<MeshGeometry> ModelLoader::LoadSphere(ModelInfo& modelInfo) {
    auto bulletMat = std::make_unique<Material>();
    bulletMat->name = "BulletGlow";
    bulletMat->matCBIndex = static_cast<UINT>(materials.size());
    bulletMat->DiffuseSrvHeapIndex = srvHeapIndex;
    bulletMat->NormalSrvHeapIndex = srvHeapIndex + 1;
    bulletMat->HeightSrvHeapIndex = srvHeapIndex + 2;
    bulletMat->DiffuseMapName = defaultDiffuseTexName;
    bulletMat->NormalMapName = defaultNormalTexName;
    bulletMat->HeightMapName = defaultHeightTexName;
    bulletMat->diffuseAlbedo = { 2.4f, 1.7f, 0.6f, 1.0f };
    bulletMat->fresnelRO = { 0.9f, 0.8f, 0.2f };
    bulletMat->roughness = 0.02f;
    materials[bulletMat->name] = std::move(bulletMat);
    srvHeapIndex += 3;

    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
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
            vertices.push_back(v);
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

            indices.push_back(i0);
            indices.push_back(i2);
            indices.push_back(i1);

            indices.push_back(i1);
            indices.push_back(i2);
            indices.push_back(i3);
        }
    }

    std::unique_ptr<MeshGeometry> bulletGeo = std::make_unique<MeshGeometry>();
    bulletGeo->Name = "BulletSphere";

    const auto submeshIndexCount = static_cast<UINT>(10 * 10 * 6);
    const UINT objStartIndex = static_cast<UINT>(allRitems.size());

    auto bulletRI = std::make_unique<RenderItem>();
    bulletRI->ObjCBIndex = static_cast<UINT>(allRitems.size());
    bulletRI->Geo = bulletGeo.get();
    bulletRI->Mat = materials["BulletGlow"].get();
    bulletRI->IndexCount = submeshIndexCount;
    bulletRI->StartIndexLocation = 0;
    bulletRI->BaseVertexLocation = 0;
    bulletRI->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    bulletRI->LocalBounds.Center = { 0.0f, 0.0f, 0.0f };
    bulletRI->LocalBounds.Extents = { 1.0f, 1.0f, 1.0f };
    XMStoreFloat4x4(&bulletRI->World, XMMatrixTranslation(0.0f, -10000.0f, 0.0f));

    allRitems.push_back(std::move(bulletRI));


    FinalizeGeometry(bulletGeo.get(), vertices, indices);
    return bulletGeo;
}
