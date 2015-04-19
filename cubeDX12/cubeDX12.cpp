#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <wrl/client.h>
#include <DirectXMath.h>
#include <d3d12.h>
#include <dxgi1_4.h>

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
void WaitForCommandQueue();

struct MyVertex {
    XMFLOAT3 Pos;
    XMFLOAT3 Normal;
    DWORD    Color;
};
MyVertex cubeVertices[] = {
    // 正面.
    { XMFLOAT3(-1.0f, 1.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f), 0xFFFFFFFF },
    { XMFLOAT3(-1.0f,-1.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f), 0xFFFF0000 },
    { XMFLOAT3( 1.0f, 1.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f), 0xFF00FF00 },
    { XMFLOAT3( 1.0f,-1.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f), 0xFF00FFFF },
    // 右.
    { XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f), 0xFF00FF00 },
    { XMFLOAT3(1.0f,-1.0f, 1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f), 0xFF00FFFF },
    { XMFLOAT3(1.0f, 1.0f,-1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f), 0xFFFFFF00 },
    { XMFLOAT3(1.0f,-1.0f,-1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f), 0xFFFFFFFF },
    // 左
    { XMFLOAT3(-1.0f, 1.0f,-1.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f), 0xFF808080 },
    { XMFLOAT3(-1.0f,-1.0f,-1.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f), 0xFF0000FF },
    { XMFLOAT3(-1.0f, 1.0f, 1.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f), 0xFFFFFFFF },
    { XMFLOAT3(-1.0f,-1.0f, 1.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f), 0xFFFF0000 },
    // 裏.
    { XMFLOAT3(1.0f, 1.0f,-1.0f),  XMFLOAT3(0.0f, 0.0f,-1.0f), 0xFFFFFF00 },
    { XMFLOAT3(1.0f,-1.0f,-1.0f),  XMFLOAT3(0.0f, 0.0f,-1.0f), 0xFFFFFFFF },
    { XMFLOAT3(-1.0f, 1.0f,-1.0f), XMFLOAT3(0.0f, 0.0f,-1.0f), 0xFF808080 },
    { XMFLOAT3(-1.0f,-1.0f,-1.0f), XMFLOAT3(0.0f, 0.0f,-1.0f), 0xFF0000FF },
    // 上.
    { XMFLOAT3(-1.0f, 1.0f,-1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f), 0xFF808080 },
    { XMFLOAT3(-1.0f, 1.0f, 1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f), 0xFFFFFFFF },
    { XMFLOAT3(1.0f, 1.0f,-1.0f),  XMFLOAT3(0.0f, 1.0f, 0.0f), 0xFFFFFF00 },
    { XMFLOAT3(1.0f, 1.0f, 1.0f),  XMFLOAT3(0.0f, 1.0f, 0.0f), 0xFF00FF00 },
    // 底.
    { XMFLOAT3(-1.0f,-1.0f, 1.0f), XMFLOAT3(0.0f,-1.0f, 0.0f), 0xFFFF0000 },
    { XMFLOAT3(-1.0f,-1.0f,-1.0f), XMFLOAT3(0.0f,-1.0f, 0.0f), 0xFF0000FF },
    { XMFLOAT3(1.0f,-1.0f, 1.0f),  XMFLOAT3(0.0f,-1.0f, 0.0f), 0xFF00FFFF },
    { XMFLOAT3(1.0f,-1.0f,-1.0f),  XMFLOAT3(0.0f,-1.0f, 0.0f), 0xFFFFFFFF },
};
uint16_t cubeIndices[] = {
    0, 1, 2,   2, 1, 3,
    4, 5, 6,   6, 5, 7,
    8, 9 ,10,  10, 9, 11,
    12, 13, 14,  14, 13, 15,
    16, 17, 18,  18, 17, 19,
    20, 21, 22,  22, 21, 23,
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
    wc.lpszClassName = L"CubeDX12";
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
ComPtr<ID3D12CommandAllocator> commandAllocator;
ComPtr<ID3D12CommandQueue> commandQueue;
ComPtr<ID3D12Fence> commandFence;
ComPtr<ID3D12DescriptorHeap> descriptorHeap;
ComPtr<ID3D12Resource> renderTargetPrimary;
ComPtr<ID3D12Resource> renderTargetDepth;
ComPtr<ID3D12RootSignature> rootSignature;
ComPtr<ID3D12PipelineState> pipelineState;
HANDLE hFenceEvent;

ComPtr<ID3D12GraphicsCommandList> commandList;
D3D12_CPU_DESCRIPTOR_HANDLE descriptorRTV;
ComPtr<ID3D12Resource> vertexBuffer;
ComPtr<ID3D12Resource> indexBuffer;
ComPtr<ID3D12Resource> constantBuffer;
D3D12_VERTEX_BUFFER_VIEW vertexView;
D3D12_INDEX_BUFFER_VIEW  indexView;
D3D12_CONSTANT_BUFFER_VIEW_DESC constantView;
ComPtr<ID3D12DescriptorHeap> descriptorHeapCB;
D3D12_CPU_DESCRIPTOR_HANDLE descriptorCB;
ComPtr<ID3D12DescriptorHeap> descriptorHeapDSB;
D3D12_CPU_DESCRIPTOR_HANDLE descriptorDSV;

void AddResourceBarrier(
    ID3D12GraphicsCommandList* command,
    ID3D12Resource* pResource,
    D3D12_RESOURCE_USAGE before,
    D3D12_RESOURCE_USAGE after
    );

// DirectX12の初期化関数.
BOOL InitializeDX12(HWND hWnd)
{
    HRESULT hr;
    D3D12_CREATE_DEVICE_FLAG dxflags = D3D12_CREATE_DEVICE_DEBUG;
#ifndef _DEBUG
    dxflags = D3D12_CREATE_DEVICE_NONE;
#endif
    hr = D3D12CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        dxflags,
        D3D_FEATURE_LEVEL_11_1,
        D3D12_SDK_VERSION,
        IID_PPV_ARGS(dxDevice.GetAddressOf())
        );

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
    // SwapChain作成時に必要になるのでコマンドキューを作成.
    hr = dxDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(commandAllocator.GetAddressOf()));
    if (FAILED(hr)) {
        MessageBoxW(NULL, L"CreateCommandAllocator() Failed.", L"ERROR", MB_OK);
        return FALSE;
    }
    D3D12_COMMAND_QUEUE_DESC descCommandQueue;
    ZeroMemory(&descCommandQueue, sizeof(descCommandQueue));
    descCommandQueue.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    hr = dxDevice->CreateCommandQueue(&descCommandQueue, IID_PPV_ARGS(commandQueue.GetAddressOf()));

    // DXGIの初期化を行う.
    DXGI_SWAP_CHAIN_DESC descSwapChain;
    ZeroMemory(&descSwapChain, sizeof(descSwapChain));
    descSwapChain.BufferCount = 2;
    descSwapChain.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    descSwapChain.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    descSwapChain.OutputWindow = hWnd;
    descSwapChain.SampleDesc.Count = 1;
    descSwapChain.Windowed = TRUE;
    descSwapChain.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    descSwapChain.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    ComPtr<IDXGIFactory3> dxgiFactory;
    hr = CreateDXGIFactory2(0, IID_PPV_ARGS(dxgiFactory.GetAddressOf()));
    if (FAILED(hr)) {
        MessageBoxW(NULL, L"CreateDXGIFactory2() failed.", L"ERROR", MB_OK);
        return FALSE;
    }
    hr = dxgiFactory->CreateSwapChain(commandQueue.Get(), &descSwapChain, (IDXGISwapChain**)(swapChain.GetAddressOf()));
    if (FAILED(hr)) {
        MessageBoxW(NULL, L"CreateSwapChain() failed.", L"ERROR", MB_OK);
        return FALSE;
    }

    // バックバッファのターゲット用に１つで作成.
    D3D12_DESCRIPTOR_HEAP_DESC descDescriptorHeap;
    ZeroMemory(&descDescriptorHeap, sizeof(descDescriptorHeap));
    descDescriptorHeap.NumDescriptors = 1;
    descDescriptorHeap.Type = D3D12_RTV_DESCRIPTOR_HEAP;
    descDescriptorHeap.Flags = D3D12_DESCRIPTOR_HEAP_NONE;
    hr = dxDevice->CreateDescriptorHeap(&descDescriptorHeap, IID_PPV_ARGS(descriptorHeap.GetAddressOf()));
    if (FAILED(hr)) {
        MessageBoxW(NULL, L"CreateDescriptorHeap() failed.", L"ERROR", MB_OK);
        return FALSE;
    }
    // コマンドキューの同期用オブジェクトを作成.
    hFenceEvent = CreateEventA(nullptr, FALSE, FALSE, nullptr);
    hr = dxDevice->CreateFence(0, D3D12_FENCE_MISC_NONE, IID_PPV_ARGS(commandFence.GetAddressOf()));

    // バックバッファをプライマリのターゲットとして.
    hr = swapChain->GetBuffer(0, IID_PPV_ARGS(renderTargetPrimary.GetAddressOf()));
    if (FAILED(hr)) {
        MessageBoxW(NULL, L"swapChain->GetBuffer() failed.", L"ERROR", MB_OK);
        return FALSE;
    }
    descriptorRTV = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
    dxDevice->CreateRenderTargetView(renderTargetPrimary.Get(), nullptr, descriptorRTV);

    // デプスバッファの準備.
    D3D12_DESCRIPTOR_HEAP_DESC descDescriptorHeapDSB;
    ZeroMemory(&descDescriptorHeapDSB, sizeof(descDescriptorHeapDSB));
    descDescriptorHeapDSB.NumDescriptors = 1;
    descDescriptorHeapDSB.Type = D3D12_DSV_DESCRIPTOR_HEAP;
    descDescriptorHeapDSB.Flags = D3D12_DESCRIPTOR_HEAP_NONE;
    hr = dxDevice->CreateDescriptorHeap(
        &descDescriptorHeapDSB,
        IID_PPV_ARGS(descriptorHeapDSB.GetAddressOf()));
    if (FAILED(hr)) {
        MessageBoxW(NULL, L"CreateDescriptorHeap() failed.", L"ERROR", MB_OK);
        return FALSE;
    }
    D3D12_RESOURCE_DESC descDepth = CD3D12_RESOURCE_DESC::Tex2D(
        DXGI_FORMAT_R32_TYPELESS,
        WINDOW_WIDTH, WINDOW_HEIGHT,
        1, 1, 1, 0,
        D3D12_RESOURCE_MISC_ALLOW_DEPTH_STENCIL
        );
    D3D12_CLEAR_VALUE dsvClearValue;
    dsvClearValue.Format = DXGI_FORMAT_D32_FLOAT;
    dsvClearValue.DepthStencil.Depth = 1.0f;
    dsvClearValue.DepthStencil.Stencil = 0;
    hr = dxDevice->CreateCommittedResource(
        &CD3D12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_MISC_NONE,
        &descDepth,
        D3D12_RESOURCE_USAGE_DEPTH,
        &dsvClearValue,
        IID_PPV_ARGS(renderTargetDepth.ReleaseAndGetAddressOf())
        );
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
    descriptorDSV = descriptorHeapDSB->GetCPUDescriptorHandleForHeapStart();

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
    D3D12_ROOT_SIGNATURE descRootSignature;
    descRootSignature.Flags = D3D12_ROOT_SIGNATURE_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    D3D12_ROOT_PARAMETER rootParameters[1];
    rootParameters[0].InitAsConstantBufferView(0);
    descRootSignature.NumParameters = 1;
    descRootSignature.pParameters = rootParameters;
    ComPtr<ID3DBlob> rootSigBlob, errorBlob;
    hr = D3D12SerializeRootSignature(&descRootSignature, D3D_ROOT_SIGNATURE_V1, rootSigBlob.GetAddressOf(), errorBlob.GetAddressOf());
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
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R8G8B8A8_UNORM, 0,24, D3D12_INPUT_PER_VERTEX_DATA, 0 },
    };

    // PipelineStateオブジェクトの作成.
    D3D12_GRAPHICS_PIPELINE_STATE_DESC descPipelineState;
    ZeroMemory(&descPipelineState, sizeof(descPipelineState));
    descPipelineState.VS.pShaderBytecode = gVertexShader.binaryPtr;
    descPipelineState.VS.BytecodeLength = gVertexShader.size;
    descPipelineState.PS.pShaderBytecode = gPixelShader.binaryPtr;
    descPipelineState.PS.BytecodeLength = gPixelShader.size;
    descPipelineState.SampleDesc.Count = 1;
    descPipelineState.SampleMask = UINT_MAX;
    descPipelineState.InputLayout.pInputElementDescs = descInputElements;
    descPipelineState.InputLayout.NumElements = _countof(descInputElements);
    descPipelineState.pRootSignature = rootSignature.Get();
    descPipelineState.NumRenderTargets = 1;
    descPipelineState.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    descPipelineState.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    descPipelineState.RasterizerState = CD3D12_RASTERIZER_DESC(D3D12_DEFAULT);
    descPipelineState.DepthStencilState = CD3D12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    descPipelineState.BlendState = CD3D12_BLEND_DESC(D3D12_DEFAULT);
    descPipelineState.DepthStencilState.DepthEnable = TRUE;
    descPipelineState.DepthStencilState.DepthFunc = D3D12_COMPARISON_LESS_EQUAL;
    descPipelineState.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    descPipelineState.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    descPipelineState.RasterizerState.CullMode = D3D12_CULL_NONE;
    hr = dxDevice->CreateGraphicsPipelineState(&descPipelineState, IID_PPV_ARGS(pipelineState.GetAddressOf()));
    if (FAILED(hr)) {
        MessageBoxW(NULL, L"CreateGraphicsPipelineState() failed.", L"ERROR", MB_OK);
        return FALSE;
    }
    // 今回用のコマンドリストをここで作成.
    hr = dxDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator.Get(), pipelineState.Get(), IID_PPV_ARGS(commandList.GetAddressOf()));
    if (FAILED(hr)) {
        MessageBoxW(NULL, L"CreateCommandList() failed.", L"ERROR", MB_OK);
        return FALSE;
    }
    commandList->Close(); // 後で投入する.
    commandAllocator->Reset();
    commandList->Reset(commandAllocator.Get(), nullptr);

    // 頂点データの準備.
    hr = dxDevice->CreateCommittedResource(
        &CD3D12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_MISC_NONE,
        &CD3D12_RESOURCE_DESC::Buffer(sizeof(cubeVertices)),
        D3D12_RESOURCE_USAGE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(vertexBuffer.GetAddressOf())
        );
    if (FAILED(hr)) {
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
    hr = dxDevice->CreateCommittedResource(
        &CD3D12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_MISC_NONE,
        &CD3D12_RESOURCE_DESC::Buffer(sizeof(cubeIndices)),
        D3D12_RESOURCE_USAGE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(indexBuffer.GetAddressOf())
        );
    if (FAILED(hr)) {
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

    // 定数バッファ Matrix4x4を３つ分で確保.
    hr = dxDevice->CreateCommittedResource(
        &CD3D12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_MISC_NONE,
        &CD3D12_RESOURCE_DESC::Buffer(sizeof(XMFLOAT4X4) * 3),
        D3D12_RESOURCE_USAGE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(constantBuffer.GetAddressOf())
        );
    if (FAILED(hr)) {
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

    ID3D12DescriptorHeap* heaps[] = { descriptorHeapCB.Get() };
    //ターゲットのクリア処理
    float clearColor[4] = { 0.2f, 0.45f, 0.8f, 0.0f };
    D3D12_VIEWPORT viewPort = { 0.0f, 0.0f, WINDOW_WIDTH, WINDOW_HEIGHT, 0.0f, 1.0f };
    D3D12_RECT rect = { 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT };
    AddResourceBarrier(commandList.Get(), renderTargetPrimary.Get(), D3D12_RESOURCE_USAGE_PRESENT, D3D12_RESOURCE_USAGE_RENDER_TARGET);
    commandList->SetGraphicsRootSignature(rootSignature.Get());
    commandList->SetPipelineState(pipelineState.Get());
    commandList->SetGraphicsRootConstantBufferView(0, constantBuffer->GetGPUVirtualAddress());
    commandList->RSSetViewports(1, &viewPort);
    commandList->RSSetScissorRects(1, &rect);
    commandList->ClearRenderTargetView(descriptorRTV, clearColor, nullptr, 0);
    commandList->SetRenderTargets(&descriptorRTV, FALSE, 1, &descriptorDSV);
    commandList->ClearDepthStencilView(descriptorDSV, D3D12_CLEAR_DEPTH, 1.0f, 0, nullptr, 0);

    // 頂点データをセット.
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->SetVertexBuffers(0, &vertexView, 1);
    commandList->SetIndexBuffer(&indexView);
    commandList->DrawIndexedInstanced(36, 1, 0, 0, 0);


    // ターゲットをPresent用のリソースとして状態変更.
    AddResourceBarrier(commandList.Get(), renderTargetPrimary.Get(), D3D12_RESOURCE_USAGE_RENDER_TARGET, D3D12_RESOURCE_USAGE_PRESENT);
    commandList->Close();

    // 上記で積んだコマンドを実行する.
    commandQueue->ExecuteCommandLists(1, CommandListCast(commandList.GetAddressOf()));
    swapChain->Present(1, 0);
    WaitForCommandQueue();
    commandAllocator->Reset();
    commandList->Reset(commandAllocator.Get(), nullptr);
    count++;
}

void WaitForCommandQueue()
{
    commandFence->Signal(0);
    commandFence->SetEventOnCompletion(1, hFenceEvent);
    commandQueue->Signal(commandFence.Get(), 1);
    WaitForSingleObject(hFenceEvent, INFINITE);
}

// リソース状態変更のための関数.
void AddResourceBarrier(
    ID3D12GraphicsCommandList* command,
    ID3D12Resource* pResource,
    D3D12_RESOURCE_USAGE before,
    D3D12_RESOURCE_USAGE after
    )
{
    D3D12_RESOURCE_BARRIER_DESC desc;
    ZeroMemory(&desc, sizeof(desc));
    desc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    desc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    desc.Transition.pResource = pResource;
    desc.Transition.StateBefore = before;
    desc.Transition.StateAfter = after;
    command->ResourceBarrier(1, &desc);
}

