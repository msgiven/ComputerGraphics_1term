#pragma once
#include "d3dUtil.h"
#include "MathHelper.h"
#include "UploadBuffer.h"


class FrameResource
{
public:
    FrameResource(ID3D12Device* device, UINT passCount, UINT objectCount);
    FrameResource(const FrameResource& rhs) = delete;
    FrameResource& operator=(const FrameResource& rhs) = delete;
private:

};

