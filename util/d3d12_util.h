#pragma once

#include <d3d12.h>
#include <wrl/client.h>

namespace d3d12util {
  using namespace Microsoft::WRL;

  // ���\�[�X��ԕύX�̂��߂̊֐�.
  void SetResourceBarrier(
    ID3D12GraphicsCommandList* commandList,
    ID3D12Resource* resource,
    D3D12_RESOURCE_STATES before,
    D3D12_RESOURCE_STATES after);

  // �W���I�� GraphicsPipelineStateDesc�𐶐�.
  D3D12_GRAPHICS_PIPELINE_STATE_DESC CreateGraphicsPipelineStateDesc(
    ID3D12RootSignature* pRootSignature,
    const void* pBinaryVS, int vsSize,
    const void* pBinaryPS, int psSize,
    D3D12_INPUT_ELEMENT_DESC* descInputElements,
    int numInputElements
    );

  ComPtr<ID3D12Resource> CreateVertexBuffer(
    ID3D12Device* device,
    int bufferSize,
    D3D12_HEAP_TYPE type,
    D3D12_RESOURCE_STATES states
    );
  ComPtr<ID3D12Resource> CreateConstantBuffer(
    ID3D12Device* device,
    int bufferSize
    );
}
