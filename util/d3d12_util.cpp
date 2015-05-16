#include "d3d12_util.h"
using namespace Microsoft::WRL;

void d3d12util::SetResourceBarrier(
  ID3D12GraphicsCommandList * commandList, 
  ID3D12Resource * resource, 
  D3D12_RESOURCE_STATES before, 
  D3D12_RESOURCE_STATES after)
{
  D3D12_RESOURCE_BARRIER descBarrier;
  ZeroMemory(&descBarrier, sizeof(descBarrier));
  descBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  descBarrier.Transition.pResource = resource;
  descBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
  descBarrier.Transition.StateBefore = before;
  descBarrier.Transition.StateAfter = after;
  commandList->ResourceBarrier(1, &descBarrier);
}

D3D12_GRAPHICS_PIPELINE_STATE_DESC d3d12util::CreateGraphicsPipelineStateDesc(ID3D12RootSignature * pRootSignature, const void * pBinaryVS, int vsSize, const void * pBinaryPS, int psSize, D3D12_INPUT_ELEMENT_DESC * descInputElements, int numInputElements)
{
  D3D12_GRAPHICS_PIPELINE_STATE_DESC descState;
  ZeroMemory(&descState, sizeof(descState));
  descState.VS.pShaderBytecode = pBinaryVS;
  descState.VS.BytecodeLength = vsSize;
  descState.PS.pShaderBytecode = pBinaryPS;
  descState.PS.BytecodeLength = psSize;
  descState.SampleDesc.Count = 1;
  descState.SampleMask = UINT_MAX;
  descState.InputLayout.pInputElementDescs = descInputElements;
  descState.InputLayout.NumElements = numInputElements;
  descState.pRootSignature = pRootSignature;
  descState.NumRenderTargets = 1;
  descState.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
  descState.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  descState.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
  descState.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
  descState.RasterizerState.DepthClipEnable = TRUE;
  descState.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
  for (int i = 0;i < _countof(descState.BlendState.RenderTarget); ++i) {
    descState.BlendState.RenderTarget[i].BlendEnable = FALSE;
    descState.BlendState.RenderTarget[i].SrcBlend = D3D12_BLEND_ONE;
    descState.BlendState.RenderTarget[i].DestBlend = D3D12_BLEND_ZERO;
    descState.BlendState.RenderTarget[i].BlendOp = D3D12_BLEND_OP_ADD;
    descState.BlendState.RenderTarget[i].SrcBlendAlpha = D3D12_BLEND_ONE;
    descState.BlendState.RenderTarget[i].DestBlendAlpha = D3D12_BLEND_ZERO;
    descState.BlendState.RenderTarget[i].BlendOpAlpha = D3D12_BLEND_OP_ADD;
    descState.BlendState.RenderTarget[i].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
  }
  descState.DepthStencilState.DepthEnable = FALSE;
  return descState;
}

ComPtr<ID3D12Resource> d3d12util::CreateVertexBuffer(
  ID3D12Device* device, int bufferSize, D3D12_HEAP_TYPE type, D3D12_RESOURCE_STATES states)
{
  D3D12_HEAP_PROPERTIES heapProps;
  D3D12_RESOURCE_DESC   descResourceVB;
  ZeroMemory(&heapProps, sizeof(heapProps));
  ZeroMemory(&descResourceVB, sizeof(descResourceVB));
  heapProps.Type = type;
  heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
  heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
  heapProps.CreationNodeMask = 0;
  heapProps.VisibleNodeMask = 0;
  descResourceVB.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  descResourceVB.Width = bufferSize;
  descResourceVB.Height = 1;
  descResourceVB.DepthOrArraySize = 1;
  descResourceVB.MipLevels = 1;
  descResourceVB.Format = DXGI_FORMAT_UNKNOWN;
  descResourceVB.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
  descResourceVB.SampleDesc.Count = 1;

  ComPtr<ID3D12Resource> vertexBuffer;
  HRESULT hr;
  hr = device->CreateCommittedResource(
    &heapProps,
    D3D12_HEAP_FLAG_NONE,
    &descResourceVB,
    states,
    nullptr,
    IID_PPV_ARGS(vertexBuffer.GetAddressOf())
    );
  if (FAILED(hr)) {
    OutputDebugString(L"CreateCommittedResource() failed.\n");
  }
  return vertexBuffer;
}

ComPtr<ID3D12Resource> d3d12util::CreateIndexBuffer(ID3D12Device * device, int bufferSize, D3D12_HEAP_TYPE type, D3D12_RESOURCE_STATES states)
{
  D3D12_HEAP_PROPERTIES heapProps;
  D3D12_RESOURCE_DESC   descResourceIB;
  ZeroMemory(&heapProps, sizeof(heapProps));
  ZeroMemory(&descResourceIB, sizeof(descResourceIB));
  heapProps.Type = type;
  heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
  heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
  heapProps.CreationNodeMask = 0;
  heapProps.VisibleNodeMask = 0;
  descResourceIB.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  descResourceIB.Width = bufferSize;
  descResourceIB.Height = 1;
  descResourceIB.DepthOrArraySize = 1;
  descResourceIB.MipLevels = 1;
  descResourceIB.Format = DXGI_FORMAT_UNKNOWN;
  descResourceIB.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
  descResourceIB.SampleDesc.Count = 1;

  ComPtr<ID3D12Resource> indexBuffer;
  HRESULT hr;
  hr = device->CreateCommittedResource(
    &heapProps,
    D3D12_HEAP_FLAG_NONE,
    &descResourceIB,
    states,
    nullptr,
    IID_PPV_ARGS(indexBuffer.GetAddressOf())
    );
  if (FAILED(hr)) {
    OutputDebugString(L"CreateCommittedResource() failed.\n");
  }
  return indexBuffer;
}

ComPtr<ID3D12Resource> d3d12util::CreateConstantBuffer(ID3D12Device * device, int bufferSize)
{
  ComPtr<ID3D12Resource> cbBuffer;
  D3D12_HEAP_PROPERTIES heapProps;
  D3D12_RESOURCE_DESC   descResourceVB;
  ZeroMemory(&heapProps, sizeof(heapProps));
  ZeroMemory(&descResourceVB, sizeof(descResourceVB));
  heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
  heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
  heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
  heapProps.CreationNodeMask = 0;
  heapProps.VisibleNodeMask = 0;
  descResourceVB.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  descResourceVB.Width = bufferSize;
  descResourceVB.Height = 1;
  descResourceVB.DepthOrArraySize = 1;
  descResourceVB.MipLevels = 1;
  descResourceVB.Format = DXGI_FORMAT_UNKNOWN;
  descResourceVB.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
  descResourceVB.SampleDesc.Count = 1;
  
  HRESULT hr;
  hr = device->CreateCommittedResource(
    &heapProps,
    D3D12_HEAP_FLAG_NONE,
    &descResourceVB,
    D3D12_RESOURCE_STATE_GENERIC_READ,
    nullptr,
    IID_PPV_ARGS(cbBuffer.GetAddressOf())
    );
  if (FAILED(hr)) {
    OutputDebugString(L"CreateCommittedResource() failed.\n");
  }
  return cbBuffer;
}

ComPtr<ID3D12Resource> d3d12util::CreateDepthBuffer(ID3D12Device * device, int width, int height)
{
  ComPtr<ID3D12Resource> depthBuffer;
  D3D12_RESOURCE_DESC descDepth;
  ZeroMemory(&descDepth, sizeof(descDepth));
  D3D12_HEAP_PROPERTIES heapProps;
  ZeroMemory(&heapProps, sizeof(heapProps));
  heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
  heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
  heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
  heapProps.CreationNodeMask = 0;
  heapProps.VisibleNodeMask = 0;
  descDepth.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
  descDepth.Width = width;
  descDepth.Height = height;
  descDepth.DepthOrArraySize = 1;
  descDepth.MipLevels = 0;
  descDepth.Format = DXGI_FORMAT_R32_TYPELESS;
  descDepth.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
  descDepth.SampleDesc.Count = 1;
  descDepth.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

  D3D12_CLEAR_VALUE dsvClearValue;
  dsvClearValue.Format = DXGI_FORMAT_D32_FLOAT;
  dsvClearValue.DepthStencil.Depth = 1.0f;
  dsvClearValue.DepthStencil.Stencil = 0;
  HRESULT hr = device->CreateCommittedResource(
    &heapProps,
    D3D12_HEAP_FLAG_NONE,
    &descDepth,
    D3D12_RESOURCE_STATE_DEPTH_WRITE,
    &dsvClearValue,
    IID_PPV_ARGS(depthBuffer.ReleaseAndGetAddressOf())
    );
  return depthBuffer;
}
