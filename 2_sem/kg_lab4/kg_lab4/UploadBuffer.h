#pragma once

#include "d3dUtil.h"

template<typename T>
class UploadBuffer
{
public:
	UploadBuffer(ID3D12Device* device, UINT elementCount, bool isConstantBuffer) : mIsConstantBuffer(isConstantBuffer) {
		mElementByteSize = sizeof(T);

		if (mIsConstantBuffer) 
			mElementByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(T));

		CD3DX12_HEAP_PROPERTIES heapPropUpload = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		CD3DX12_RESOURCE_DESC resDesc = CD3DX12_RESOURCE_DESC::Buffer(mElementByteSize * elementCount);
		ThrowIfFailed(device->CreateCommittedResource(&heapPropUpload, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&mUploadBuffer)));

		ThrowIfFailed(mUploadBuffer->Map(0, nullptr, reinterpret_cast<void**>(&mMappedData)));
		
	}
	
	UploadBuffer(const UploadBuffer& rhs) = delete;
	UploadBuffer& operator=(const UploadBuffer& rhs) = delete;
	~UploadBuffer()
	{
		if (mUploadBuffer != nullptr)
			mUploadBuffer->Unmap(0, nullptr);

		mMappedData = nullptr;
	}

	ID3D12Resource* Resource() {
		return mUploadBuffer.Get();
	}

	void CopyData(int elementIndex, const T& data) {
		memcpy(&mMappedData[elementIndex * mElementByteSize], &data, sizeof(T));
	}

private:
	ComPtr<ID3D12Resource> mUploadBuffer;
	BYTE* mMappedData = nullptr;
	bool mIsConstantBuffer = false;
	UINT mElementByteSize = 0;
};