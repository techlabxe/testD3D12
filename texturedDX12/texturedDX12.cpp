#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <wrl/client.h>
#include <DirectXMath.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include "../util/d3d12_util.h"

#define WINDOW_WIDTH  640
#define WINDOW_HEIGHT 480

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

using namespace DirectX;
using namespace Microsoft::WRL;

LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);
// DirectX12 Function
BOOL InitializeDX12(HWND);
BOOL ResourceSetupDX12();
void RenderDX12();
void TerminateDX12();
void WaitForCommandQueue(ID3D12CommandQueue* pCommandQueue);

struct MyVertex {
  XMFLOAT3 Pos;
  XMFLOAT3 Color;
  XMFLOAT2 Tex;
};
MyVertex cubeVertices[] = {
  { XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT2(0.0f,1.0f) },
  { XMFLOAT3(-1.0f, -1.0f,  1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f), XMFLOAT2(0.0f,1.0f) },
  { XMFLOAT3(-1.0f,  1.0f, -1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT2(0.0f,0.0f) },
  { XMFLOAT3(-1.0f,  1.0f,  1.0f), XMFLOAT3(0.0f, 1.0f, 1.0f), XMFLOAT2(0.0f,0.0f) },
  { XMFLOAT3(1.0f, -1.0f, -1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f), XMFLOAT2(1.0f,1.0f) },
  { XMFLOAT3(1.0f, -1.0f,  1.0f), XMFLOAT3(1.0f, 0.0f, 1.0f), XMFLOAT2(1.0f,1.0f) },
  { XMFLOAT3(1.0f,  1.0f, -1.0f), XMFLOAT3(1.0f, 1.0f, 0.0f), XMFLOAT2(1.0f,0.0f) },
  { XMFLOAT3(1.0f,  1.0f,  1.0f), XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT2(1.0f,0.0f) },
};
uint16_t cubeIndices[] = {
  0,2,1, // -x
  1,2,3,

  4,5,6, // +x
  5,7,6,

  0,1,5, // -y
  0,5,4,

  2,6,7, // +y
  2,7,3,

  0,4,6, // -z
  0,6,2,

  1,3,7, // +z
  1,7,5,
};

int APIENTRY WinMain(_In_ HINSTANCE hInstance,
  _In_opt_ HINSTANCE hPrevInstance,
  _In_ LPSTR    lpCmdLine,
  _In_ int       nCmdShow)
{
  UNREFERENCED_PARAMETER(hPrevInstance);
  UNREFERENCED_PARAMETER(lpCmdLine);

  MSG msg;
  HWND hwnd;
  WNDCLASSEX wc;
  ZeroMemory(&wc, sizeof(wc));
  ZeroMemory(&msg, sizeof(msg));

  wc.cbSize = sizeof(wc);
  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.lpfnWndProc = WndProc;
  wc.hInstance = hInstance;
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
  wc.lpszClassName = L"TexturedDX12";
  RegisterClassEx(&wc);

  RECT rect = { 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT };
  DWORD dwStyle = WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX;
  AdjustWindowRect(&rect, dwStyle, FALSE);
  hwnd = CreateWindow(wc.lpszClassName,
    wc.lpszClassName,
    dwStyle,
    CW_USEDEFAULT, CW_USEDEFAULT,
    rect.right - rect.left, rect.bottom - rect.top,
    NULL, NULL, hInstance, NULL);
  ShowWindow(hwnd, nCmdShow);
  UpdateWindow(hwnd);

  if (!InitializeDX12(hwnd)) {
    return FALSE;
  }
  if (!ResourceSetupDX12()) {
    return FALSE;
  }

  while (msg.message != WM_QUIT) {
    if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
    else {
      RenderDX12();
    }
  }

  TerminateDX12();
  return (int)msg.wParam;
}
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  PAINTSTRUCT ps;
  HDC hdc;

  switch (message)
  {
  case WM_PAINT:
    hdc = BeginPaint(hWnd, &ps);
    EndPaint(hWnd, &ps);
    break;
  case WM_DESTROY:
    PostQuitMessage(0);
    break;
  default:
    return DefWindowProc(hWnd, message, wParam, lParam);
  }
  return 0;
}


struct ShaderObject {
  void* binaryPtr;
  int   size;
};
ShaderObject gVertexShader;
ShaderObject gPixelShader;

using namespace Microsoft::WRL;
ComPtr<ID3D12Device> dxDevice;
ComPtr<IDXGISwapChain3> swapChain;
ComPtr<IDXGIFactory3> dxgiFactory;
ComPtr<ID3D12CommandAllocator> cmdAllocator;
ComPtr<ID3D12CommandQueue> cmdQueue;
ComPtr<ID3D12Fence> queueFence;
ComPtr<ID3D12DescriptorHeap> descriptorHeapRTV;
ComPtr<ID3D12DescriptorHeap> descriptorHeapSRVCBV;
ComPtr<ID3D12DescriptorHeap> descriptorHeapSMP;
ComPtr<ID3D12Resource> renderTarget[2];
ComPtr<ID3D12Resource> renderTargetDepth;
ComPtr<ID3D12RootSignature> rootSignature;
ComPtr<ID3D12PipelineState> pipelineState;
HANDLE hFenceEvent;

ComPtr<ID3D12GraphicsCommandList> cmdList;
D3D12_CPU_DESCRIPTOR_HANDLE handleRTV[2];
ComPtr<ID3D12Resource> vertexBuffer;
ComPtr<ID3D12Resource> indexBuffer;
ComPtr<ID3D12Resource> constantBuffer;
ComPtr<ID3D12Resource> texture;
D3D12_VERTEX_BUFFER_VIEW vertexView;
D3D12_INDEX_BUFFER_VIEW  indexView;
ComPtr<ID3D12DescriptorHeap> descriptorHeapDSB;
D3D12_CPU_DESCRIPTOR_HANDLE handleDSV;
D3D12_CPU_DESCRIPTOR_HANDLE handleSRV;
D3D12_CPU_DESCRIPTOR_HANDLE handleCBV;
D3D12_CPU_DESCRIPTOR_HANDLE handleSampler;

// DirectX12の初期化関数.
BOOL InitializeDX12(HWND hWnd)
{
  HRESULT hr;
  UINT flagsDXGI = 0;
  ID3D12Debug* debug = nullptr;

#ifdef _DEBUG
  D3D12GetDebugInterface(IID_PPV_ARGS(&debug));
  if (debug) {
    debug->EnableDebugLayer();
    debug->Release();
    debug = nullptr;
  }
  flagsDXGI |= DXGI_CREATE_FACTORY_DEBUG;
#endif
  hr = CreateDXGIFactory2(flagsDXGI, IID_PPV_ARGS(dxgiFactory.ReleaseAndGetAddressOf()));
  if (FAILED(hr)) {
    OutputDebugString(L"Failed CreateDXGIFactory2\n");
    return FALSE;
  }
  ComPtr<IDXGIAdapter> adapter;
  hr = dxgiFactory->EnumAdapters(0, adapter.GetAddressOf());
  if (FAILED(hr)) {
    return FALSE;
  }
  // デバイス生成.
  // D3D12は 最低でも D3D_FEATURE_LEVEL_11_0 を要求するようだ.
  hr = D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(dxDevice.GetAddressOf()));
  if (FAILED(hr)) {
    MessageBoxW(NULL, L"D3D12CreateDevice() Failed.", L"ERROR", MB_OK);
    return FALSE;
  }
  D3D12_FEATURE_DATA_D3D12_OPTIONS options;
  hr = dxDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, (void*)&options, sizeof(options));
  if (FAILED(hr)) {
    MessageBoxW(NULL, L"CheckFeatureSupport() Failed.", L"ERROR", MB_OK);
    return FALSE;
  }

  // コマンドアロケータを生成.
  hr = dxDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(cmdAllocator.GetAddressOf()));
  if (FAILED(hr)) {
    MessageBoxW(NULL, L"CreateCommandAllocator() Failed.", L"ERROR", MB_OK);
    return FALSE;
  }
  // コマンドキューを生成.
  D3D12_COMMAND_QUEUE_DESC descCommandQueue;
  ZeroMemory(&descCommandQueue, sizeof(descCommandQueue));
  descCommandQueue.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
  descCommandQueue.Priority = 0;
  descCommandQueue.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
  hr = dxDevice->CreateCommandQueue(&descCommandQueue, IID_PPV_ARGS(cmdQueue.GetAddressOf()));
  if (FAILED(hr)) {
    OutputDebugString(L"Failed CreateCommandQueue\n");
    return FALSE;
  }
  // コマンドキュー用のフェンスを準備.
  hFenceEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
  hr = dxDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(queueFence.GetAddressOf()));
  if (FAILED(hr)) {
    OutputDebugString(L"Failed CreateFence\n");
    return FALSE;
  }

  // スワップチェインを生成.
  DXGI_SWAP_CHAIN_DESC descSwapChain;
  ZeroMemory(&descSwapChain, sizeof(descSwapChain));
  descSwapChain.BufferCount = 2;
  descSwapChain.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  descSwapChain.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  descSwapChain.OutputWindow = hWnd;
  descSwapChain.SampleDesc.Count = 1;
  descSwapChain.Windowed = TRUE;
  descSwapChain.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
  descSwapChain.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
  hr = dxgiFactory->CreateSwapChain(cmdQueue.Get(), &descSwapChain, (IDXGISwapChain**)swapChain.GetAddressOf());
  if (FAILED(hr)) {
    OutputDebugString(L"Failed CreateSwapChain\n");
    return FALSE;
  }
  // コマンドリストの作成.
  hr = dxDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmdAllocator.Get(), nullptr, IID_PPV_ARGS(cmdList.GetAddressOf()));
  if (FAILED(hr)) {
    OutputDebugString(L"Failed CreateCommandList\n");
    return FALSE;
  }

  // バックバッファのターゲット用に１つで作成.
  // ディスクリプタヒープ(RenderTarget用)の作成.
  D3D12_DESCRIPTOR_HEAP_DESC descHeap;
  ZeroMemory(&descHeap, sizeof(descHeap));
  descHeap.NumDescriptors = 2;
  descHeap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
  descHeap.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
  hr = dxDevice->CreateDescriptorHeap(&descHeap, IID_PPV_ARGS(descriptorHeapRTV.GetAddressOf()));
  if (FAILED(hr)) {
    MessageBoxW(NULL, L"CreateDescriptorHeap() failed.", L"ERROR", MB_OK);
    return FALSE;
  }

  // レンダーターゲット(プライマリ用)の作成.
  UINT strideHandleBytes = dxDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
  for (UINT i = 0;i < descSwapChain.BufferCount;++i) {
    hr = swapChain->GetBuffer(i, IID_PPV_ARGS(renderTarget[i].GetAddressOf()));
    if (FAILED(hr)) {
      OutputDebugString(L"Failed swapChain->GetBuffer\n");
      return FALSE;
    }
    handleRTV[i] = descriptorHeapRTV->GetCPUDescriptorHandleForHeapStart();
    handleRTV[i].ptr += i*strideHandleBytes;
    dxDevice->CreateRenderTargetView(renderTarget[i].Get(), nullptr, handleRTV[i]);
  }

  // デプスバッファの準備.
  D3D12_DESCRIPTOR_HEAP_DESC descDescriptorHeapDSB;
  ZeroMemory(&descDescriptorHeapDSB, sizeof(descDescriptorHeapDSB));
  descDescriptorHeapDSB.NumDescriptors = 1;
  descDescriptorHeapDSB.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
  descDescriptorHeapDSB.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
  hr = dxDevice->CreateDescriptorHeap(
    &descDescriptorHeapDSB,
    IID_PPV_ARGS(descriptorHeapDSB.GetAddressOf()));
  if (FAILED(hr)) {
    MessageBoxW(NULL, L"CreateDescriptorHeap() failed.", L"ERROR", MB_OK);
    return FALSE;
  }
  renderTargetDepth = d3d12util::CreateDepthBuffer(dxDevice.Get(), WINDOW_WIDTH, WINDOW_HEIGHT);
  if (!renderTargetDepth) {
    MessageBoxW(NULL, L"CreateDepthBuffer() failed.", L"ERROR", MB_OK);
    return FALSE;
  }

  D3D12_DEPTH_STENCIL_VIEW_DESC descDSV;
  ZeroMemory(&descDSV, sizeof(descDSV));
  descDSV.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
  descDSV.Format = DXGI_FORMAT_D32_FLOAT;
  descDSV.Texture2D.MipSlice = 0;
  dxDevice->CreateDepthStencilView(
    renderTargetDepth.Get(),
    &descDSV,
    descriptorHeapDSB->GetCPUDescriptorHandleForHeapStart()
    );
  handleDSV = descriptorHeapDSB->GetCPUDescriptorHandleForHeapStart();

  D3D12_RESOURCE_DESC descResourceTex;
  D3D12_HEAP_PROPERTIES heapProps;
  ZeroMemory(&heapProps, sizeof(heapProps));
  ZeroMemory(&descResourceTex, sizeof(descResourceTex));
  heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
  heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
  heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
  heapProps.CreationNodeMask = 0;
  heapProps.VisibleNodeMask = 0;
  descResourceTex.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
  descResourceTex.Width = 256;
  descResourceTex.Height = 256;
  descResourceTex.DepthOrArraySize = 1;
  descResourceTex.MipLevels = 1;
  descResourceTex.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  descResourceTex.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
  descResourceTex.SampleDesc.Count = 1;
  hr = dxDevice->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &descResourceTex, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(texture.GetAddressOf()));
  {
    D3D12_BOX box = { 0 };
    box.right = 256;
    box.bottom = 256;
    box.back = 1;
    void* p = malloc(256 * 256 * 4);
    memset(p, 0xFF, 256 * 256 * 4);
    texture->WriteToSubresource(0, &box, p, 4 * 256, 4 * 256 * 256);
  }

  // シェーダーリソースビューのためのヒープディスクリプタ.
  D3D12_DESCRIPTOR_HEAP_DESC descDescriptorHeapSRV;
  ZeroMemory(&descDescriptorHeapSRV, sizeof(descDescriptorHeapSRV));
  descDescriptorHeapSRV.NumDescriptors = 4;
  descDescriptorHeapSRV.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
  descDescriptorHeapSRV.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
  hr = dxDevice->CreateDescriptorHeap(&descDescriptorHeapSRV, IID_PPV_ARGS(descriptorHeapSRVCBV.GetAddressOf()));
  if (FAILED(hr)) {
    MessageBox(hWnd, L"Failed CreateDescriptorHeap(SRV).", L"ERROR", MB_OK);
    return FALSE;
  }

  // サンプラーのためのヒープディスクリプタ.
  D3D12_DESCRIPTOR_HEAP_DESC descDescriptorHeapSmp;
  ZeroMemory(&descDescriptorHeapSmp, sizeof(descDescriptorHeapSmp));
  descDescriptorHeapSmp.NumDescriptors = 1;
  descDescriptorHeapSmp.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
  descDescriptorHeapSmp.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
  hr = dxDevice->CreateDescriptorHeap(&descDescriptorHeapSmp, IID_PPV_ARGS(descriptorHeapSMP.GetAddressOf()));
  if (FAILED(hr)) {
    MessageBox(hWnd, L"Failed CreateDescriptorHeap(Sampler).", L"ERROR", MB_OK);
    return FALSE;
  }
  return TRUE;
}

// DirectX12の終了関数.
void TerminateDX12()
{
  if (gVertexShader.binaryPtr) { free(gVertexShader.binaryPtr); }
  if (gPixelShader.binaryPtr) { free(gPixelShader.binaryPtr); }
  CloseHandle(hFenceEvent);
}

// リソースの準備.
BOOL ResourceSetupDX12()
{
  HRESULT hr;
  // PipelineStateのための RootSignature の作成.
  D3D12_ROOT_SIGNATURE_DESC descRootSignature;
  ZeroMemory(&descRootSignature, sizeof(descRootSignature));
  descRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
  D3D12_ROOT_PARAMETER rootParameters[2];
  D3D12_DESCRIPTOR_RANGE range[3];
  ZeroMemory(range, sizeof(range));
  range[0].NumDescriptors = 1;
  range[0].BaseShaderRegister = 0;
  range[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
  range[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
  range[1].NumDescriptors = 1;
  range[1].BaseShaderRegister = 0;
  range[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
  range[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

  range[2].NumDescriptors = 1;
  range[2].BaseShaderRegister = 0;
  range[2].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
  range[2].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

  rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
  rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
  rootParameters[0].DescriptorTable.pDescriptorRanges = &range[0];
  rootParameters[0].DescriptorTable.NumDescriptorRanges = 2;

  rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
  rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
  rootParameters[1].DescriptorTable.pDescriptorRanges = &range[2];
  rootParameters[1].DescriptorTable.NumDescriptorRanges = 1;

  descRootSignature.NumParameters = _countof(rootParameters);
  descRootSignature.pParameters = rootParameters;
  ComPtr<ID3DBlob> rootSigBlob, errorBlob;
  hr = D3D12SerializeRootSignature(&descRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, rootSigBlob.GetAddressOf(), errorBlob.GetAddressOf());
  hr = dxDevice->CreateRootSignature(0, rootSigBlob->GetBufferPointer(), rootSigBlob->GetBufferSize(), IID_PPV_ARGS(rootSignature.GetAddressOf()));
  if (FAILED(hr)) {
    MessageBoxW(NULL, L"CreateRootSignature() failed.", L"ERROR", MB_OK);
    return FALSE;
  }

  // コンパイル済みシェーダーの読み込み.
  // コンパイルそのものはVisualStudioのビルド時にやる.
  FILE* fpVS = nullptr;
  fopen_s(&fpVS, "VertexShader.cso", "rb");
  if (!fpVS) { return FALSE; }
  fseek(fpVS, 0, SEEK_END);
  gVertexShader.size = ftell(fpVS); rewind(fpVS);
  gVertexShader.binaryPtr = malloc(gVertexShader.size);
  fread(gVertexShader.binaryPtr, 1, gVertexShader.size, fpVS);
  fclose(fpVS); fpVS = nullptr;
  FILE* fpPS = nullptr;
  fopen_s(&fpPS, "PixelShader.cso", "rb");
  if (!fpPS) { return FALSE; }
  fseek(fpPS, 0, SEEK_END);
  gPixelShader.size = ftell(fpPS); rewind(fpPS);
  gPixelShader.binaryPtr = malloc(gPixelShader.size);
  fread(gPixelShader.binaryPtr, 1, gPixelShader.size, fpPS);
  fclose(fpPS); fpPS = nullptr;

  // 今回のための頂点レイアウト.
  D3D12_INPUT_ELEMENT_DESC descInputElements[] = {
    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "COLOR",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0,12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
  };

  // PipelineStateオブジェクトの作成.
  D3D12_GRAPHICS_PIPELINE_STATE_DESC descPipelineState;
  descPipelineState = d3d12util::CreateGraphicsPipelineStateDesc(
    rootSignature.Get(),
    gVertexShader.binaryPtr, gVertexShader.size,
    gPixelShader.binaryPtr, gPixelShader.size,
    descInputElements,
    _countof(descInputElements));
  descPipelineState.DepthStencilState.DepthEnable = TRUE;
  descPipelineState.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
  descPipelineState.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
  descPipelineState.DSVFormat = DXGI_FORMAT_D32_FLOAT;
  hr = dxDevice->CreateGraphicsPipelineState(&descPipelineState, IID_PPV_ARGS(pipelineState.GetAddressOf()));
  if (FAILED(hr)) {
    MessageBoxW(NULL, L"CreateGraphicsPipelineState() failed.", L"ERROR", MB_OK);
    return FALSE;
  }

  // 頂点データの準備.
  vertexBuffer = d3d12util::CreateVertexBuffer(
    dxDevice.Get(),
    sizeof(cubeVertices),
    D3D12_HEAP_TYPE_UPLOAD,
    D3D12_RESOURCE_STATE_GENERIC_READ);
  if (!vertexBuffer) {
    MessageBoxW(NULL, L"CreateCommittedResource() failed.", L"ERROR", MB_OK);
    return FALSE;
  }
  // 頂点データの書き込み
  void* mapped = nullptr;
  hr = vertexBuffer->Map(0, nullptr, &mapped);
  if (SUCCEEDED(hr)) {
    memcpy(mapped, cubeVertices, sizeof(cubeVertices));
    vertexBuffer->Unmap(0, nullptr);
  }
  if (FAILED(hr)) {
    MessageBoxW(NULL, L"vertexBuffer->Map() failed.", L"ERROR", MB_OK);
    return FALSE;
  }
  vertexView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
  vertexView.StrideInBytes = sizeof(MyVertex);
  vertexView.SizeInBytes = sizeof(cubeVertices);

  // インデックスバッファの準備.
  indexBuffer = d3d12util::CreateIndexBuffer(
    dxDevice.Get(),
    sizeof(cubeIndices),
    D3D12_HEAP_TYPE_UPLOAD,
    D3D12_RESOURCE_STATE_GENERIC_READ);
  if (!indexBuffer) {
    MessageBoxW(NULL, L"CreateCommittedResource() failed.", L"ERROR", MB_OK);
    return FALSE;
  }
  hr = indexBuffer->Map(0, nullptr, &mapped);
  if (SUCCEEDED(hr)) {
    memcpy(mapped, cubeIndices, sizeof(cubeIndices));
    indexBuffer->Unmap(0, nullptr);
  }
  indexView.BufferLocation = indexBuffer->GetGPUVirtualAddress();
  indexView.SizeInBytes = sizeof(cubeIndices);
  indexView.Format = DXGI_FORMAT_R16_UINT;

  // 定数バッファ Matrix4x4 分で確保.
  constantBuffer = d3d12util::CreateConstantBuffer(dxDevice.Get(), /*sizeof(XMFLOAT4X4)*/256);
  if (!constantBuffer) {
    MessageBoxW(NULL, L"[for constantBuffer] CreateCommittedResource() failed.", L"ERROR", MB_OK);
    return FALSE;
  }
  hr = constantBuffer->Map(0, nullptr, &mapped);
  if (SUCCEEDED(hr)) {
    XMFLOAT4X4 mtx;
    XMStoreFloat4x4(&mtx, XMMatrixIdentity());
    memcpy(mapped, &mtx, sizeof(mtx));
    constantBuffer->Unmap(0, nullptr);
  }

  UINT strideSize = dxDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

  D3D12_CONSTANT_BUFFER_VIEW_DESC descCBV;
  ZeroMemory(&descCBV, sizeof(descCBV));
  descCBV.BufferLocation = constantBuffer->GetGPUVirtualAddress();
  descCBV.SizeInBytes = 256;//sizeof(XMFLOAT4X4);
  handleCBV = descriptorHeapSRVCBV->GetCPUDescriptorHandleForHeapStart();
  dxDevice->CreateConstantBufferView(&descCBV, handleCBV);

  D3D12_SHADER_RESOURCE_VIEW_DESC descSRV;
  ZeroMemory(&descSRV, sizeof(descSRV));
  descSRV.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  descSRV.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
  descSRV.Texture2D.MipLevels = 1;
  descSRV.Texture2D.MostDetailedMip = 0;
  descSRV.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING; // これがないとおこられた.
  handleSRV = descriptorHeapSRVCBV->GetCPUDescriptorHandleForHeapStart();
  handleSRV.ptr += strideSize;
  dxDevice->CreateShaderResourceView(texture.Get(), &descSRV, handleSRV);


  handleSampler = descriptorHeapSMP->GetCPUDescriptorHandleForHeapStart();
  D3D12_SAMPLER_DESC descSampler;
  ZeroMemory(&descSampler, sizeof(descSampler));
  descSampler.Filter = D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
  descSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
  descSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
  descSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
  descSampler.MaxLOD = FLT_MAX;
  descSampler.MinLOD = -FLT_MAX;
  descSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
  dxDevice->CreateSampler(&descSampler, handleSampler);

  return TRUE;
}

void RenderDX12()
{
  static int count = 0;
  // ビューおよびプロジェクション行列を準備.
  XMMATRIX view = XMMatrixLookAtLH(
    XMVECTOR{ 0.0f, 2.0f, -4.5f },
    XMVECTOR{ 0.0f,0.0f,0.0f },
    XMVECTOR{ 0.0f, 1.0f, 0.0f }
  );
  float fov = XM_PI / 3.0f;
  XMMATRIX proj = XMMatrixPerspectiveFovLH(
    fov,
    float(WINDOW_WIDTH) / float(WINDOW_HEIGHT),
    0.1f, 100.f
    );
  // 適当にまわす.
  XMMATRIX world = XMMatrixIdentity();
  world = XMMatrixRotationY(count*0.02f);
  XMMATRIX wvp = XMMatrixMultiply(XMMatrixMultiply(world, view), proj);
  void* pCB;
  constantBuffer->Map(0, nullptr, &pCB);
  if (pCB) {
    XMStoreFloat4x4((XMFLOAT4X4*)pCB, wvp);
    constantBuffer->Unmap(0, nullptr);
  }

  ID3D12DescriptorHeap* heaps[] = { descriptorHeapSRVCBV.Get(), descriptorHeapSMP.Get() };
  //ターゲットのクリア処理
  int targetIndex = swapChain->GetCurrentBackBufferIndex();
  float clearColor[4] = { 0.2f, 0.45f, 0.8f, 0.0f };
  D3D12_VIEWPORT viewPort = { 0.0f, 0.0f, WINDOW_WIDTH, WINDOW_HEIGHT, 0.0f, 1.0f };
  D3D12_RECT rect = { 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT };
  d3d12util::SetResourceBarrier(cmdList.Get(), renderTarget[targetIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
  cmdList->SetGraphicsRootSignature(rootSignature.Get());
  cmdList->SetPipelineState(pipelineState.Get());
  //cmdList->SetGraphicsRootConstantBufferView(0, constantBuffer->GetGPUVirtualAddress());
  cmdList->SetDescriptorHeaps(_countof(heaps), heaps);
  cmdList->SetGraphicsRootDescriptorTable(0, descriptorHeapSRVCBV->GetGPUDescriptorHandleForHeapStart());
  cmdList->SetGraphicsRootDescriptorTable(1, descriptorHeapSMP->GetGPUDescriptorHandleForHeapStart() );

  cmdList->RSSetViewports(1, &viewPort);
  cmdList->RSSetScissorRects(1, &rect);
  cmdList->ClearDepthStencilView(handleDSV, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
  cmdList->ClearRenderTargetView(handleRTV[targetIndex], clearColor, 0, nullptr);
  cmdList->OMSetRenderTargets(1, &handleRTV[targetIndex], TRUE, &handleDSV);

  // 頂点データをセット.
  cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  cmdList->IASetVertexBuffers(0, 1, &vertexView);
  cmdList->IASetIndexBuffer(&indexView);
  cmdList->DrawIndexedInstanced(36, 1, 0, 0, 0);


  // ターゲットをPresent用のリソースとして状態変更.
  d3d12util::SetResourceBarrier(cmdList.Get(), renderTarget[targetIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
  cmdList->Close();

  // 上記で積んだコマンドを実行する.
  ID3D12CommandList* pCommandList = cmdList.Get();
  cmdQueue->ExecuteCommandLists(1, &pCommandList);
  swapChain->Present(1, 0);
  WaitForCommandQueue(cmdQueue.Get());
  cmdAllocator->Reset();
  cmdList->Reset(cmdAllocator.Get(), nullptr);
  count++;
}

void WaitForCommandQueue(ID3D12CommandQueue* pCommandQueue) {
  queueFence->Signal(0);
  queueFence->SetEventOnCompletion(1, hFenceEvent);
  pCommandQueue->Signal(queueFence.Get(), 1);
  WaitForSingleObject(hFenceEvent, INFINITE);
}

