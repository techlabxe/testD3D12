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
    XMFLOAT4 Color;
};
MyVertex triangleVertices[] = {
    { XMFLOAT3(0.0f, 0.5f, 0.0f),  XMFLOAT4(1.0f,0.0f,0.0f,1.0f) },
    { XMFLOAT3(0.45f,-0.5f, 0.0f), XMFLOAT4(0.0f,1.0f,0.0f,1.0f) },
    { XMFLOAT3(-.45f,-0.5f, 0.0f), XMFLOAT4(0.0f,0.0f,1.0f,1.0f) },
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
    wc.lpszClassName = L"MovingTriangleDX12";
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
ComPtr<ID3D12Resource> renderTarget[2];
ComPtr<ID3D12RootSignature> rootSignature;
ComPtr<ID3D12PipelineState> pipelineState;
HANDLE hFenceEvent;

ComPtr<ID3D12GraphicsCommandList> cmdList;
D3D12_CPU_DESCRIPTOR_HANDLE handleRTV[2]; 
ComPtr<ID3D12Resource> vertexBuffer;
ComPtr<ID3D12Resource> constantBuffer;
D3D12_VERTEX_BUFFER_VIEW vertexView;
D3D12_CONSTANT_BUFFER_VIEW_DESC constantView;
ComPtr<ID3D12DescriptorHeap> descriptorHeapCB;
D3D12_CPU_DESCRIPTOR_HANDLE descriptorCB;

// DirectX12の初期化関数.
BOOL InitializeDX12( HWND hwnd )
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
  descSwapChain.OutputWindow = hwnd;
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
    D3D12_ROOT_PARAMETER rootParameters[1];
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[0].Descriptor.ShaderRegister = 0;
    rootParameters[0].Descriptor.RegisterSpace = 0;
    descRootSignature.NumParameters = 1;
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
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    // PipelineStateオブジェクトの作成.
    D3D12_GRAPHICS_PIPELINE_STATE_DESC descPipelineState;
    descPipelineState = d3d12util::CreateGraphicsPipelineStateDesc(
      rootSignature.Get(),
      gVertexShader.binaryPtr, gVertexShader.size,
      gPixelShader.binaryPtr, gPixelShader.size,
      descInputElements,
      _countof(descInputElements));
    hr = dxDevice->CreateGraphicsPipelineState(&descPipelineState, IID_PPV_ARGS(pipelineState.GetAddressOf()));
    if (FAILED(hr)) {
        MessageBoxW(NULL, L"CreateGraphicsPipelineState() failed.", L"ERROR", MB_OK);
        return FALSE;
    }

    // 頂点データの準備.
    vertexBuffer= d3d12util::CreateVertexBuffer(
      dxDevice.Get(),
      sizeof(triangleVertices),
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
        memcpy(mapped, triangleVertices, sizeof(triangleVertices));
        vertexBuffer->Unmap(0, nullptr);
    }
    if (FAILED(hr)) {
        MessageBoxW(NULL, L"vertexBuffer->Map() failed.", L"ERROR", MB_OK);
        return FALSE;
    }
    vertexView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
    vertexView.StrideInBytes = sizeof(MyVertex);
    vertexView.SizeInBytes = sizeof(triangleVertices);

    // 定数バッファ Matrix4x4 分で確保.
    constantBuffer = d3d12util::CreateConstantBuffer(dxDevice.Get(), sizeof(XMFLOAT4X4));
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
    return TRUE;
}

void RenderDX12()
{
    static int count = 0;
    // ビューおよびプロジェクション行列を準備.
    XMMATRIX view = XMMatrixLookAtLH(
        XMVECTOR{ 0.0f, 0.0f, -1.5f },
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
    world = XMMatrixMultiply(XMMatrixRotationX(count * 0.01f), XMMatrixRotationY(count*0.02f));
    XMMATRIX wvp = XMMatrixMultiply(XMMatrixMultiply(world, view), proj);
    void* pCB;
    constantBuffer->Map(0, nullptr, &pCB);
    if (pCB) {
        XMStoreFloat4x4( (XMFLOAT4X4*)pCB, wvp);
        constantBuffer->Unmap(0, nullptr);
    }

    ID3D12DescriptorHeap* heaps[] = { descriptorHeapCB.Get() };
    //ターゲットのクリア処理
    int targetIndex = swapChain->GetCurrentBackBufferIndex();
    float clearColor[4] = { 0.2f, 0.45f, 0.8f, 0.0f };
    D3D12_VIEWPORT viewPort = { 0.0f, 0.0f, WINDOW_WIDTH, WINDOW_HEIGHT, 0.0f, 1.0f };
    D3D12_RECT rect = { 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT };
    d3d12util::SetResourceBarrier(cmdList.Get(), renderTarget[targetIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    cmdList->SetGraphicsRootSignature(rootSignature.Get());
    cmdList->SetPipelineState(pipelineState.Get());
    cmdList->SetGraphicsRootConstantBufferView(0, constantBuffer->GetGPUVirtualAddress() );
    cmdList->RSSetViewports(1, &viewPort);
    cmdList->RSSetScissorRects(1, &rect);
    cmdList->ClearRenderTargetView(handleRTV[targetIndex], clearColor, 0, nullptr);
    cmdList->OMSetRenderTargets(1, &handleRTV[targetIndex], TRUE, nullptr);

    // 頂点データをセット.
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmdList->IASetVertexBuffers(0, 1, &vertexView);
    cmdList->DrawInstanced(3, 1, 0, 0);

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
  static UINT64 frames = 0;
  queueFence->SetEventOnCompletion(frames, hFenceEvent);
  pCommandQueue->Signal(queueFence.Get(), frames);
  WaitForSingleObject(hFenceEvent, INFINITE);
  frames++;
}

