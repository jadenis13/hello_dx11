#define WIN32_LEAN_AND_MEAN
#define STRICT
#include <windows.h>

#include <wrl/client.h>
using Microsoft::WRL::ComPtr;

#include <dxgi.h>
#include <d3d11.h>
#include <DirectXMath.h>
using namespace DirectX;
#include <d3dcompiler.h>

#include <codecvt>
#include <locale>
#include <memory>

#include "DirectXTex/WICTextureLoader/WICTextureLoader.h"

#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")

struct Settings {
    long screenWidth, screenHeight;
    bool isFullscreen;
    bool isVsyncEnabled;
};

class Model {
public:
    struct Vertex {
        XMFLOAT3 position;
        XMFLOAT2 texcoord;
    };

};

struct ConstantBufferPerObject {
    XMMATRIX wvpMatrix;
};

class Graphics {
public:
    static std::unique_ptr<Graphics> GetInstance();
    ~Graphics();
    Graphics();

    HRESULT Initialize(HWND, Settings const &);
    void Terminate();

    bool Render();
    void Update();

private:
    const int MAX_FRAME_BUFFERS = 2;

    ComPtr<ID3D11Device> m_pDevice;
    ComPtr<ID3D11DeviceContext> m_pDeviceContext;

    bool m_isVsyncEnabled;
    ComPtr<IDXGISwapChain> m_pSwapChain;
    ComPtr<ID3D11RenderTargetView> m_pRtv;
    ComPtr<ID3D11Texture2D> m_pDepthStencilBuffer;
    ComPtr<ID3D11DepthStencilView> m_pDsv;

    ComPtr<ID3D11Buffer> m_pVertexBuffer;
    ComPtr<ID3D11InputLayout> m_pVertexLayout;
    ComPtr<ID3D11Buffer> m_pIndexBuffer;

    ComPtr<ID3D11VertexShader> m_pVertexShader;
    ComPtr<ID3D11PixelShader> m_pPixelShader;

    ComPtr<ID3D11Buffer> m_pVsConstantBuffer;
    ConstantBufferPerObject m_cbPerObject;

    XMMATRIX cameraProjection;
    XMMATRIX cameraView;

    XMVECTOR cameraPosition;
    XMVECTOR cameraTarget;
    XMVECTOR cameraUp;

    XMMATRIX cube1World;
    XMMATRIX cube2World;

    XMMATRIX rotation, scale, translation;
    float rot = 0.01f;

    ComPtr<ID3D11Resource> m_pTextureBuffer;
    ComPtr<ID3D11ShaderResourceView> m_pSrvTexture;

    ComPtr<ID3D11SamplerState> m_pSamplerState;
};

std::unique_ptr<Graphics> Graphics::GetInstance() {
    return std::make_unique<Graphics>();
}

Graphics::Graphics() : m_isVsyncEnabled(false) { }

Graphics::~Graphics() {
    Terminate();
}

HRESULT Graphics::Initialize(HWND hwnd, Settings const & settings) {
    auto retVal = S_OK;

    DXGI_MODE_DESC bufferDesc;
    ZeroMemory(&bufferDesc, sizeof bufferDesc);
    bufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    bufferDesc.Height = settings.screenHeight;
    bufferDesc.RefreshRate.Numerator = 60;
    bufferDesc.RefreshRate.Denominator = 1;
    bufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
    bufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;

    DXGI_SWAP_CHAIN_DESC swapChainDesc;
    ZeroMemory(&swapChainDesc, sizeof swapChainDesc);
    swapChainDesc.BufferCount = 1;
    swapChainDesc.BufferDesc = bufferDesc;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.Flags = 0;
    swapChainDesc.OutputWindow = hwnd;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    swapChainDesc.Windowed = !settings.isFullscreen;

    retVal = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION,
        &swapChainDesc, &m_pSwapChain, &m_pDevice, nullptr, &m_pDeviceContext);
    if (FAILED(retVal)) {
        MessageBox(hwnd, L"Failed to create device/swap chain", L"ERROR", MB_ICONERROR | MB_OK);
        return retVal;
    }

    ComPtr<ID3D11Texture2D> pBackBuffer;
    if (FAILED(retVal = m_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer)))) {
        MessageBox(hwnd, L"Failed to get back buffer", L"ERROR", MB_ICONERROR | MB_OK);
        return retVal;
    }

    if (FAILED(retVal = m_pDevice->CreateRenderTargetView(pBackBuffer.Get(), nullptr, &m_pRtv))) {
        MessageBox(hwnd, L"Failed to create rtv", L"ERROR", MB_ICONERROR | MB_OK);
        return retVal;
    }

    pBackBuffer.Reset();

    auto dsDesc = D3D11_TEXTURE2D_DESC{};
    ZeroMemory(&dsDesc, sizeof dsDesc);
    dsDesc.ArraySize = 1;
    dsDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    dsDesc.CPUAccessFlags = 0;
    dsDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dsDesc.Height = settings.screenHeight;
    dsDesc.MipLevels = 1;
    dsDesc.MiscFlags = 0;
    dsDesc.SampleDesc.Count = 1;
    dsDesc.SampleDesc.Quality = 0;
    dsDesc.Usage = D3D11_USAGE_DEFAULT;
    dsDesc.Width = settings.screenWidth;

    if(FAILED(retVal = m_pDevice->CreateTexture2D(&dsDesc, nullptr, &m_pDepthStencilBuffer))) {
        MessageBox(hwnd, L"Failed to create depth/stencil buffer", L"ERROR", MB_ICONERROR | MB_OK);
        return retVal;
    }

    if(FAILED(retVal = m_pDevice->CreateDepthStencilView(m_pDepthStencilBuffer.Get(), nullptr, &m_pDsv))) {
        MessageBox(hwnd, L"Failed to create dsv", L"ERROR", MB_ICONERROR | MB_OK);
        return retVal;
    }

    ID3D11RenderTargetView* ppRtvs[] = { m_pRtv.Get() };
    m_pDeviceContext->OMSetRenderTargets(1, ppRtvs, m_pDsv.Get());

    ComPtr<ID3D10Blob> pVertexShaderBlob;
    ComPtr<ID3D10Blob> pErrorMessages;
    retVal = D3DCompileFromFile(L"vs.hlsl", nullptr, nullptr, "vs", "vs_5_0", D3D10_SHADER_ENABLE_STRICTNESS, 0,
        &pVertexShaderBlob, &pErrorMessages);
    if (FAILED(retVal)) {
        if (pErrorMessages.Get()) {
            std::wstring_convert<std::codecvt_utf8<wchar_t>> myconv;
            auto wstr = myconv.from_bytes(static_cast<char*>(pErrorMessages->GetBufferPointer()));
            MessageBox(hwnd, wstr.c_str(), L"ERROR (VS)", MB_ICONERROR | MB_OK);
            return retVal;
        }
        else {
            MessageBox(hwnd, L"Error loading shader", L"ERROR", MB_ICONERROR | MB_OK);
            return retVal;
        }
    }

    retVal = m_pDevice->CreateVertexShader(pVertexShaderBlob->GetBufferPointer(), pVertexShaderBlob->GetBufferSize(), nullptr,
        &m_pVertexShader);
    if (FAILED(retVal)) {
        MessageBox(hwnd, L"Error creating vertex shader", L"ERROR", MB_ICONERROR | MB_OK);
        return retVal;
    }

    ComPtr<ID3D10Blob> pPixelShaderBlob;
    retVal = D3DCompileFromFile(L"ps.hlsl", nullptr, nullptr, "ps", "ps_5_0", D3D10_SHADER_ENABLE_STRICTNESS, 0,
        &pPixelShaderBlob, &pErrorMessages);
    if (FAILED(retVal)) {
        if (pErrorMessages.Get()) {
            std::wstring_convert<std::codecvt_utf8<wchar_t>> myconv;
            auto wstr = myconv.from_bytes(static_cast<char*>(pErrorMessages->GetBufferPointer()));
            MessageBox(hwnd, wstr.c_str(), L"ERROR (PS)", MB_ICONERROR | MB_OK);
            return retVal;
        }
        else {
            MessageBox(hwnd, L"Error loading pixel shader", L"ERROR", MB_ICONERROR | MB_OK);
            return retVal;
        }
    }

    retVal = m_pDevice->CreatePixelShader(pPixelShaderBlob->GetBufferPointer(), pPixelShaderBlob->GetBufferSize(), nullptr,
        &m_pPixelShader);
    if (FAILED(retVal)) {
        MessageBox(hwnd, L"Error creating pixel shader", L"ERROR", MB_ICONERROR | MB_OK);
        return retVal;
    }

    pPixelShaderBlob.Reset();

    D3D11_INPUT_ELEMENT_DESC vertexLayout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    int numElements = sizeof vertexLayout / sizeof vertexLayout[0];

    retVal = m_pDevice->CreateInputLayout(vertexLayout, numElements, pVertexShaderBlob->GetBufferPointer(), pVertexShaderBlob->GetBufferSize(),
        &m_pVertexLayout);
    if (FAILED(retVal)) {
        MessageBox(hwnd, L"Error creating vertex layout", L"ERROR", MB_ICONERROR | MB_OK);
        return retVal;
    }

    pVertexShaderBlob.Reset();

    m_pDeviceContext->IASetInputLayout(m_pVertexLayout.Get());
    m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    
    m_pDeviceContext->VSSetShader(m_pVertexShader.Get(), nullptr, 0);
    m_pDeviceContext->PSSetShader(m_pPixelShader.Get(), nullptr, 0);

    Model::Vertex vertices[] = {
        { { -1.0f, -1.0f, -1.0f }, { 0.0f, 1.0f } },
        { { -1.0f,  1.0f, -1.0f }, { 0.0f, 0.0f } },
        { {  1.0f,  1.0f, -1.0f }, { 1.0f, 0.0f } },
        { {  1.0f, -1.0f, -1.0f }, { 1.0f, 1.0f } },

        { { -1.0f, -1.0f,  1.0f }, { 0.0f, 1.0f } },
        { {  1.0f, -1.0f,  1.0f }, { 0.0f, 0.0f } },
        { {  1.0f,  1.0f,  1.0f }, { 1.0f, 0.0f } },
        { { -1.0f,  1.0f,  1.0f }, { 1.0f, 1.0f } },

        { { -1.0f,  1.0f, -1.0f }, { 0.0f, 1.0f } },
        { { -1.0f,  1.0f,  1.0f }, { 0.0f, 0.0f } },
        { {  1.0f,  1.0f,  1.0f }, { 1.0f, 0.0f } },
        { {  1.0f,  1.0f, -1.0f }, { 1.0f, 1.0f } },

        { { -1.0f, -1.0f, -1.0f }, { 0.0f, 1.0f } },
        { {  1.0f, -1.0f, -1.0f }, { 0.0f, 0.0f } },
        { {  1.0f, -1.0f,  1.0f }, { 1.0f, 0.0f } },
        { { -1.0f, -1.0f,  1.0f }, { 1.0f, 1.0f } },

        { { -1.0f, -1.0f,  1.0f }, { 0.0f, 1.0f } },
        { { -1.0f,  1.0f,  1.0f }, { 0.0f, 0.0f } },
        { { -1.0f,  1.0f, -1.0f }, { 1.0f, 0.0f } },
        { { -1.0f, -1.0f, -1.0f }, { 1.0f, 1.0f } },

        { {  1.0f, -1.0f, -1.0f }, { 0.0f, 1.0f } },
        { {  1.0f,  1.0f, -1.0f }, { 0.0f, 0.0f } },
        { {  1.0f,  1.0f,  1.0f }, { 1.0f, 0.0f } },
        { {  1.0f, -1.0f,  1.0f }, { 1.0f, 1.0f } },
    };

    auto vbDesc = D3D11_BUFFER_DESC{};
    ZeroMemory(&vbDesc, sizeof vbDesc);
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vbDesc.ByteWidth = sizeof vertices;
    vbDesc.CPUAccessFlags = 0;
    vbDesc.MiscFlags = 0;
    vbDesc.StructureByteStride = 0;
    vbDesc.Usage = D3D11_USAGE_DEFAULT;

    auto vbData = D3D11_SUBRESOURCE_DATA{};
    ZeroMemory(&vbData, sizeof vbData);
    vbData.pSysMem = vertices;

    if(FAILED(retVal = m_pDevice->CreateBuffer(&vbDesc, &vbData, &m_pVertexBuffer))) {
        MessageBox(hwnd, L"Error creating vertex buffer", L"ERROR", MB_ICONERROR | MB_OK);
        return retVal;
    }

    ID3D11Buffer* ppVertexBuffers[] = { m_pVertexBuffer.Get() };
    auto stride = static_cast<UINT>(sizeof Model::Vertex);
    auto offset = 0U;
    m_pDeviceContext->IASetVertexBuffers(0, 1, ppVertexBuffers, &stride, &offset);

    uint32_t indices[] = {
         0,  1,  2,
         0,  2,  3,

         4,  5,  6,
         4,  6,  7,

         8,  9, 10,
         8, 10, 11,

        12, 13, 14,
        12, 14, 15,

        16, 17, 18,
        16, 18, 19,

        20, 21, 22,
        20, 22, 23
    };

    auto ibDesc = D3D11_BUFFER_DESC{};
    ZeroMemory(&ibDesc, sizeof ibDesc);
    ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    ibDesc.ByteWidth = sizeof indices;
    ibDesc.Usage = D3D11_USAGE_DEFAULT;

    auto ibData = D3D11_SUBRESOURCE_DATA{};
    ZeroMemory(&ibData, sizeof ibData);
    ibData.pSysMem = indices;

    if(FAILED(retVal = m_pDevice->CreateBuffer(&ibDesc, &ibData, &m_pIndexBuffer))) {
        MessageBox(hwnd, L"Error creating index buffer", L"ERROR", MB_ICONERROR | MB_OK);
        return retVal;
    }

    m_pDeviceContext->IASetIndexBuffer(m_pIndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);

    auto matrixBufferDesc = D3D11_BUFFER_DESC{};
    matrixBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    matrixBufferDesc.ByteWidth = sizeof(ConstantBufferPerObject);
    matrixBufferDesc.CPUAccessFlags = 0;
    matrixBufferDesc.MiscFlags = 0;
    matrixBufferDesc.StructureByteStride = 0;
    matrixBufferDesc.Usage = D3D11_USAGE_DEFAULT;

    if(FAILED(retVal = m_pDevice->CreateBuffer(&matrixBufferDesc, nullptr, &m_pVsConstantBuffer))) {
        MessageBox(hwnd, L"Error creating constant buffer", L"ERROR", MB_ICONERROR | MB_OK);
        return retVal;
    }

    if(FAILED(retVal = CreateWICTextureFromFile(m_pDevice.Get(), L"image.png", &m_pTextureBuffer, &m_pSrvTexture))) {
        MessageBox(hwnd, L"Error creating texture buffer", L"ERROR", MB_ICONERROR | MB_OK);
        return retVal;
    }

    auto samplerDesc = D3D11_SAMPLER_DESC{};
    ZeroMemory(&samplerDesc, sizeof samplerDesc);
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    samplerDesc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
    samplerDesc.MaxAnisotropy = 16;
    samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
    samplerDesc.MinLOD = 0;

    if(FAILED(retVal = m_pDevice->CreateSamplerState(&samplerDesc, &m_pSamplerState))) {
        MessageBox(hwnd, L"Error creating sampler state", L"ERROR", MB_ICONERROR | MB_OK);
        return retVal;
    }

    cameraPosition = XMVectorSet(0.0f, 3.0f, -8.0f, 0.0f);
    cameraTarget = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
    cameraUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    cameraView = XMMatrixLookAtLH(cameraPosition, cameraTarget, cameraUp);

    cameraProjection = XMMatrixPerspectiveFovLH(XM_PI*0.4f, static_cast<float>(settings.screenWidth) / settings.screenHeight, 1.0f, 1000.0f);

    D3D11_VIEWPORT viewport;
    ZeroMemory(&viewport, sizeof viewport);
    viewport.Height = static_cast<float>(settings.screenHeight);
    viewport.MaxDepth = 1.0f;
    viewport.MinDepth = 0.0f;
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.Width = static_cast<float>(settings.screenWidth);

    m_pDeviceContext->RSSetViewports(1, &viewport);
    
    return retVal;
}

void Graphics::Terminate() {
    m_pSamplerState.Reset();
    m_pSrvTexture.Reset();
    m_pTextureBuffer.Reset();
    m_pVsConstantBuffer.Reset();
    m_pIndexBuffer.Reset();
    m_pVertexBuffer.Reset();
    m_pVertexLayout.Reset();
    m_pPixelShader.Reset();
    m_pVertexShader.Reset();
    m_pDsv.Reset();
    m_pDepthStencilBuffer.Reset();
    m_pRtv.Reset();
    m_pSwapChain.Reset();
    m_pDeviceContext.Reset();
    m_pDevice.Reset();
}

bool Graphics::Render() {
    auto clearColor = XMVECTORF32{ 1.0f, 0.0f, 1.0f, 1.0f };
    m_pDeviceContext->ClearRenderTargetView(m_pRtv.Get(), clearColor);
    m_pDeviceContext->ClearDepthStencilView(m_pDsv.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    auto wvp = cube1World * cameraView * cameraProjection;
    m_cbPerObject.wvpMatrix = XMMatrixTranspose(wvp);
    m_pDeviceContext->UpdateSubresource(m_pVsConstantBuffer.Get(), 0, nullptr, &m_cbPerObject, 0, 0);
    ID3D11Buffer* ppConstantBuffers[] = { m_pVsConstantBuffer.Get() };
    m_pDeviceContext->VSSetConstantBuffers(0, 1, ppConstantBuffers);
    ID3D11ShaderResourceView* ppSrvTextures[] = { m_pSrvTexture.Get() };
    m_pDeviceContext->PSSetShaderResources(0, 1, ppSrvTextures);
    ID3D11SamplerState* ppSamplers[] = { m_pSamplerState.Get() };
    m_pDeviceContext->PSSetSamplers(0, 1, ppSamplers);

    m_pDeviceContext->DrawIndexed(36, 0, 0);

    wvp = cube2World * cameraView * cameraProjection;
    m_cbPerObject.wvpMatrix = XMMatrixTranspose(wvp);
    m_pDeviceContext->UpdateSubresource(m_pVsConstantBuffer.Get(), 0, nullptr, &m_cbPerObject, 0, 0);
    m_pDeviceContext->VSSetConstantBuffers(0, 1, ppConstantBuffers);
    m_pDeviceContext->PSSetShaderResources(0, 1, ppSrvTextures);
    m_pDeviceContext->PSSetSamplers(0, 1, ppSamplers);

    m_pDeviceContext->DrawIndexed(36, 0, 0);

    m_pSwapChain->Present(0, 0);
    return true;
}

void Graphics::Update() {
    rot += 0.0005f;
    if(rot > XM_PI * 2) {
        rot = 0.0f;
    }

    cube1World = XMMatrixIdentity();
    auto rotAxis = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    rotation = XMMatrixRotationAxis(rotAxis, rot);
    translation = XMMatrixTranslation(0.0f, 0.0f, 4.0f);

    cube1World = translation * rotation;

    cube2World = XMMatrixIdentity();
    rotation = XMMatrixRotationAxis(rotAxis, -rot);
    scale = XMMatrixScaling(1.3f, 1.3f, 1.3f);

    cube2World = rotation * scale;
}

//
// Windows set up
//

auto g_isRunning = true;

LRESULT CALLBACK MessageHandler(HWND hwnd, UINT msgId, WPARAM wParam, LPARAM lParam) {
    switch (msgId) {
    case WM_DESTROY:
        g_isRunning = false;
        PostQuitMessage(0);
        return 0;

    case WM_KEYDOWN:
        if (VK_ESCAPE == wParam) {
            g_isRunning = false;
            DestroyWindow(hwnd);
        }
        return 0;

    default:
        break;
    }

    return DefWindowProc(hwnd, msgId, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hinst, HINSTANCE, PSTR, int) {

    auto wcex = WNDCLASSEX{};
    ZeroMemory(&wcex, sizeof wcex);
    wcex.cbSize = sizeof wcex;
    wcex.lpszClassName = L"WNDCLASSNAME";
    wcex.lpfnWndProc = MessageHandler;
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.hInstance = hinst;

    if (!RegisterClassEx(&wcex)) {
        MessageBox(nullptr, L"Error registering window class", L"ERROR", MB_ICONERROR | MB_OK);
        return -1;
    }

    auto const INITIAL_WIDTH = 1440;
    auto const INITIAL_HEIGHT = 1080;

    auto hwnd = CreateWindowEx(
        WS_EX_APPWINDOW,
        wcex.lpszClassName,
        L"Hello DX11",
        WS_POPUP,
        (GetSystemMetrics(SM_CXSCREEN) - INITIAL_WIDTH) / 2,
        (GetSystemMetrics(SM_CYSCREEN) - INITIAL_HEIGHT) / 2,
        INITIAL_WIDTH,
        INITIAL_HEIGHT,
        nullptr,
        nullptr,
        wcex.hInstance,
        nullptr
    );
    if (nullptr == hwnd) {
        UnregisterClass(wcex.lpszClassName, wcex.hInstance);
        MessageBox(nullptr, L"Error creating window", L"ERROR", MB_ICONERROR | MB_OK);
        return -2;
    }

    ShowWindow(hwnd, SW_SHOW);
    SetForegroundWindow(hwnd);
    ShowCursor(false);

    auto graphics = Graphics::GetInstance();

    auto rc = RECT{};
    GetClientRect(hwnd, &rc);

    auto settings = Settings{};
    ZeroMemory(&settings, sizeof settings);
    settings.screenWidth = rc.right - rc.left;
    settings.screenHeight = rc.bottom - rc.top;
    settings.isFullscreen = false;
    settings.isVsyncEnabled = false;

    if (FAILED(graphics->Initialize(hwnd, settings))) {
        DestroyWindow(hwnd);
        UnregisterClass(wcex.lpszClassName, wcex.hInstance);
        MessageBox(hwnd, L"Error initializing graphics", L"ERROR", MB_ICONERROR | MB_OK);
        return -3;
    }

    auto msg = MSG{ nullptr, 0 };
    while (g_isRunning) {
        if (PeekMessage(&msg, hwnd, 0, 0, PM_REMOVE)) {
            if (WM_QUIT == msg.message) {
                //
                // EARLY TERMINATION
                break;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else {
            graphics->Update();
            g_isRunning = graphics->Render();
        }
    }

    graphics.reset();

    return static_cast<int>(msg.wParam);
}