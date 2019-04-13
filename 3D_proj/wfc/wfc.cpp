//--------------------------------------------------------------------------------------
// File: wfc.cpp
//
// Advanced DXUT
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "DXUTcamera.h"
#include "DXUTgui.h"
#include "DXUTsettingsDlg.h"
#include "SDKmisc.h"
#include "SDKmesh.h"

#pragma warning( disable : 4100 )

using namespace DirectX;

//--------------------------------------------------------------------------------------
// 结构体
//--------------------------------------------------------------------------------------
struct CBNeverChanges
{
    XMFLOAT4 vLightDir;
};

struct CBChangesEveryFrame
{
    XMFLOAT4X4 mWorldViewProj;
    XMFLOAT4X4 mWorld;
    XMFLOAT4   vMisc;
};


//--------------------------------------------------------------------------------------
// 全局变量
//--------------------------------------------------------------------------------------
CModelViewerCamera          g_Camera;               // viewing camera类
CDXUTDialogResourceManager  g_DialogResourceManager; // 声明dialog类控制器，传递消息和控制资源
CD3DSettingsDlg             g_SettingsDlg;          // 设备设置对话框
CDXUTTextHelper*            g_pTxtHelper = nullptr; // 绘制test类
CDXUTDialog                 g_HUD;                  // 控制3d设备
CDXUTDialog                 g_SampleUI;             // 对样例特殊的控制

// Direct3D 11 资源声明
ID3D11VertexShader*         g_pVertexShader = nullptr;
ID3D11PixelShader*          g_pPixelShader = nullptr;
ID3D11InputLayout*          g_pVertexLayout = nullptr;
CDXUTSDKMesh                g_Mesh;
ID3D11Buffer*               g_pCBNeverChanges = nullptr;
ID3D11Buffer*               g_pCBChangesEveryFrame = nullptr;
ID3D11SamplerState*         g_pSamplerLinear = nullptr;
XMMATRIX                    g_World;

static const XMVECTORF32 s_LightDir = { -0.577f, 0.577f, -0.577f, 0.f };

float                       g_fModelPuffiness = 0.0f;
bool                        g_bSpinning = true;
void in_wfc();

//--------------------------------------------------------------------------------------
// UI IDs
//--------------------------------------------------------------------------------------
#define IDC_TOGGLEFULLSCREEN    1
#define IDC_WFC                 2
#define IDC_CHANGEDEVICE        3
#define IDC_TOGGLESPIN          4
//#define IDC_PUFF_SCALE          5
//#define IDC_PUFF_STATIC         6


//--------------------------------------------------------------------------------------
// 提前声明
//--------------------------------------------------------------------------------------
void RenderText();
void InitApp();


//--------------------------------------------------------------------------------------
// 返回false 可拒绝不可行的任何D3D11 设备
//--------------------------------------------------------------------------------------
bool CALLBACK IsD3D11DeviceAcceptable( const CD3D11EnumAdapterInfo *AdapterInfo, UINT Output, const CD3D11EnumDeviceInfo *DeviceInfo,
                                       DXGI_FORMAT BackBufferFormat, bool bWindowed, void* pUserContext )
{
    return true;
}


//--------------------------------------------------------------------------------------
// 在创建一个D3D设备之前调用，允许APP按需求修改设备
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext )
{
    return true;
}


//--------------------------------------------------------------------------------------
// 创建任何不依赖于背后缓存的资源
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D11CreateDevice( ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc,
                                      void* pUserContext )
{
    HRESULT hr = S_OK;

    auto pd3dImmediateContext = DXUTGetD3D11DeviceContext();
	//初始化资源管理器
    V_RETURN( g_DialogResourceManager.OnD3D11CreateDevice( pd3dDevice, pd3dImmediateContext ) );
    V_RETURN( g_SettingsDlg.OnD3D11CreateDevice( pd3dDevice ) );
    g_pTxtHelper = new CDXUTTextHelper( pd3dDevice, pd3dImmediateContext, &g_DialogResourceManager, 15 );

    DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;

#ifdef _DEBUG

    dwShaderFlags |= D3DCOMPILE_DEBUG;
    dwShaderFlags |= D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    // 编译vertex shader
    ID3DBlob* pVSBlob = nullptr;
    V_RETURN( DXUTCompileFromFile( L"wfc.fx", nullptr, "VS", "vs_4_0", dwShaderFlags, 0, &pVSBlob ) );

    // 创建vertex shader
    hr = pd3dDevice->CreateVertexShader( pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), nullptr, &g_pVertexShader );
    if( FAILED( hr ) )
    {    
        SAFE_RELEASE( pVSBlob );
        return hr;
    }

    // 定义输入布局
    D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    UINT numElements = ARRAYSIZE( layout );

    // 创建输入布局
    hr = pd3dDevice->CreateInputLayout( layout, numElements, pVSBlob->GetBufferPointer(),
                                        pVSBlob->GetBufferSize(), &g_pVertexLayout );
    SAFE_RELEASE( pVSBlob );
    if( FAILED( hr ) )
        return hr;

    // 设置输入布局
    pd3dImmediateContext->IASetInputLayout( g_pVertexLayout );

    // 编译pixel shader
    ID3DBlob* pPSBlob  = nullptr;
    V_RETURN( DXUTCompileFromFile( L"wfc.fx", nullptr, "PS", "ps_4_0", dwShaderFlags, 0, &pPSBlob  ) );

    // 创建pixel shader
    hr = pd3dDevice->CreatePixelShader( pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), nullptr, &g_pPixelShader );
    SAFE_RELEASE( pPSBlob );
    if( FAILED( hr ) )
        return hr;

    // 加载模型
    V_RETURN( g_Mesh.Create( pd3dDevice, L"Tiny\\tiny.sdkmesh" ) );
	
    // 创建常量缓存
    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.ByteWidth = sizeof(CBChangesEveryFrame);
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    V_RETURN( pd3dDevice->CreateBuffer( &bd, nullptr, &g_pCBChangesEveryFrame ) );

    bd.ByteWidth = sizeof(CBNeverChanges);
    V_RETURN( pd3dDevice->CreateBuffer( &bd, nullptr, &g_pCBNeverChanges ) );

	//更新常量缓存中的数据
    D3D11_MAPPED_SUBRESOURCE MappedResource;
    V( pd3dImmediateContext->Map( g_pCBNeverChanges, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource ) );
    auto pCB = reinterpret_cast<CBNeverChanges*>( MappedResource.pData );
    XMStoreFloat3( reinterpret_cast<XMFLOAT3*>( &pCB->vLightDir ), s_LightDir );
    pCB->vLightDir.w = 1.f;
    pd3dImmediateContext->Unmap( g_pCBNeverChanges , 0 );

    // 创建采样方式
    D3D11_SAMPLER_DESC sampDesc = {};
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampDesc.MinLOD = 0;
    sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
    V_RETURN( pd3dDevice->CreateSamplerState( &sampDesc, &g_pSamplerLinear ) );

    // 初始化世界变换矩阵
    g_World = XMMatrixIdentity();

    //设置相机视图和位置
    static const XMVECTORF32 s_Eye = { 0.0f, 0.0f, -800.0f, 0.f };
    static const XMVECTORF32 s_At = { 0.0f, 1.0f, 0.0f, 0.f };
    g_Camera.SetViewParams( s_Eye, s_At );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// 创建任何依赖于背后缓存的资源
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D11ResizedSwapChain( ID3D11Device* pd3dDevice, IDXGISwapChain* pSwapChain,
                                          const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext )
{
    HRESULT hr;

    V_RETURN( g_DialogResourceManager.OnD3D11ResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc ) );
    V_RETURN( g_SettingsDlg.OnD3D11ResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc ) );

    //设置相机的视角、远近、裁截面
    float fAspectRatio = pBackBufferSurfaceDesc->Width / ( FLOAT )pBackBufferSurfaceDesc->Height;
    g_Camera.SetProjParams( XM_PI / 4, fAspectRatio, 0.1f, 1000.0f );
    g_Camera.SetWindow( pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height );
	//创建简单的鼠标事件反馈
    g_Camera.SetButtonMasks( MOUSE_LEFT_BUTTON, MOUSE_WHEEL, MOUSE_MIDDLE_BUTTON );

	//将两个对话框放置屏幕上
    g_HUD.SetLocation( pBackBufferSurfaceDesc->Width - 170, 0 );
    g_HUD.SetSize( 170, 170 );
    g_SampleUI.SetLocation( pBackBufferSurfaceDesc->Width - 170, pBackBufferSurfaceDesc->Height - 300 );
    g_SampleUI.SetSize( 170, 300 );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// 更新场景的句柄
//--------------------------------------------------------------------------------------
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext )
{
    // Rotate cube around the origin
    

    // Update the camera's position based on user input 
    g_Camera.FrameMove( fElapsedTime );

    if( g_bSpinning )
    {
        g_World = XMMatrixRotationY( 60.0f * XMConvertToRadians((float)fTime) );
    }
    else
    {
        g_World = XMMatrixRotationY( XMConvertToRadians( 180.f ) );
    }

    XMMATRIX mRot = XMMatrixRotationX( XMConvertToRadians( -90.0f ) );
    g_World = mRot * g_World;
}


//--------------------------------------------------------------------------------------
// 渲染文本
//--------------------------------------------------------------------------------------
void RenderText()
{
    g_pTxtHelper->Begin();
    g_pTxtHelper->SetInsertionPos( 5, 5 );
    g_pTxtHelper->SetForegroundColor( Colors::Black );
    g_pTxtHelper->DrawTextLine( DXUTGetFrameStats( DXUTIsVsyncEnabled() ) );
    g_pTxtHelper->DrawTextLine( DXUTGetDeviceStats() );
    g_pTxtHelper->End();
}


//--------------------------------------------------------------------------------------
//	每帧渲染场景
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11FrameRender( ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dImmediateContext,
                                  double fTime, float fElapsedTime, void* pUserContext )
{
    // If the settings dialog is being shown, then render it instead of rendering the app's scene
    if( g_SettingsDlg.IsActive() )
    {
        g_SettingsDlg.OnRender( fElapsedTime );
        return;
    }       

    //
    // 清除背后缓存
    //
    auto pRTV = DXUTGetD3D11RenderTargetView();
    pd3dImmediateContext->ClearRenderTargetView( pRTV, Colors::RosyBrown );

    //
    // 清除深度和背后缓存
    //
    auto pDSV = DXUTGetD3D11DepthStencilView();
    pd3dImmediateContext->ClearDepthStencilView( pDSV, D3D11_CLEAR_DEPTH, 1.0, 0 );
	//获取相机转换后的矩阵
    XMMATRIX mView = g_Camera.GetViewMatrix();
    XMMATRIX mProj = g_Camera.GetProjMatrix();
    XMMATRIX mWorldViewProjection = g_World * mView * mProj;

    // 更新常量缓存资源
    HRESULT hr;
    D3D11_MAPPED_SUBRESOURCE MappedResource;
    V( pd3dImmediateContext->Map( g_pCBChangesEveryFrame , 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource ) );
    auto pCB = reinterpret_cast<CBChangesEveryFrame*>( MappedResource.pData );
    XMStoreFloat4x4( &pCB->mWorldViewProj, XMMatrixTranspose( mWorldViewProjection ) );
    XMStoreFloat4x4( &pCB->mWorld, XMMatrixTranspose( g_World ) );
    pCB->vMisc.x = g_fModelPuffiness;
    pd3dImmediateContext->Unmap( g_pCBChangesEveryFrame , 0 );

    //
    // 设置顶点输入布局
    //
    pd3dImmediateContext->IASetInputLayout( g_pVertexLayout );

    //
    // 渲染模型
    //
    UINT Strides[1];
    UINT Offsets[1];
    ID3D11Buffer* pVB[1];
    pVB[0] = g_Mesh.GetVB11( 0, 0 );
    Strides[0] = ( UINT )g_Mesh.GetVertexStride( 0, 0 );
    Offsets[0] = 0;
    pd3dImmediateContext->IASetVertexBuffers( 0, 1, pVB, Strides, Offsets );
    pd3dImmediateContext->IASetIndexBuffer( g_Mesh.GetIB11( 0 ), g_Mesh.GetIBFormat11( 0 ), 0 );

    pd3dImmediateContext->VSSetShader( g_pVertexShader, nullptr, 0 );
    pd3dImmediateContext->VSSetConstantBuffers( 0, 1, &g_pCBNeverChanges );
    pd3dImmediateContext->VSSetConstantBuffers( 1, 1, &g_pCBChangesEveryFrame );

    pd3dImmediateContext->PSSetShader( g_pPixelShader, nullptr, 0 );
    pd3dImmediateContext->PSSetConstantBuffers( 1, 1, &g_pCBChangesEveryFrame );
    pd3dImmediateContext->PSSetSamplers( 0, 1, &g_pSamplerLinear );

    for( UINT subset = 0; subset < g_Mesh.GetNumSubsets( 0 ); ++subset )
    {
        auto pSubset = g_Mesh.GetSubset( 0, subset );

        auto PrimType = g_Mesh.GetPrimitiveType11( ( SDKMESH_PRIMITIVE_TYPE )pSubset->PrimitiveType );
        pd3dImmediateContext->IASetPrimitiveTopology( PrimType );

        // 忽略模型材质信息，只用最简单的着色器
        auto pDiffuseRV = g_Mesh.GetMaterial( pSubset->MaterialID )->pDiffuseRV11;
        pd3dImmediateContext->PSSetShaderResources( 0, 1, &pDiffuseRV );

        pd3dImmediateContext->DrawIndexed( ( UINT )pSubset->IndexCount, 0, ( UINT )pSubset->VertexStart );
    }
	//渲染对话框
    g_HUD.OnRender( fElapsedTime );
    g_SampleUI.OnRender( fElapsedTime );
    RenderText();
}


//--------------------------------------------------------------------------------------
// 释放在OnD3D11ResizedSwapChain中创建的资源 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11ReleasingSwapChain( void* pUserContext )
{
    g_DialogResourceManager.OnD3D11ReleasingSwapChain();
}


//--------------------------------------------------------------------------------------
// 释放在OnD3D11CreateDevice中创建的资源 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11DestroyDevice( void* pUserContext )
{
    g_DialogResourceManager.OnD3D11DestroyDevice();
    g_SettingsDlg.OnD3D11DestroyDevice();
    DXUTGetGlobalResourceCache().OnDestroyDevice();
    SAFE_DELETE( g_pTxtHelper );

    g_Mesh.Destroy();

    SAFE_RELEASE( g_pVertexLayout );
    SAFE_RELEASE( g_pVertexShader );
    SAFE_RELEASE( g_pPixelShader );
    SAFE_RELEASE( g_pCBNeverChanges );
    SAFE_RELEASE( g_pCBChangesEveryFrame );
    SAFE_RELEASE( g_pSamplerLinear );
}


//--------------------------------------------------------------------------------------
//当窗口接收到消息时调用此函数
//--------------------------------------------------------------------------------------
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
                          bool* pbNoFurtherProcessing, void* pUserContext )
{
	//传消息给对话框资源管理器，保证GUI正常更新
    *pbNoFurtherProcessing = g_DialogResourceManager.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;

    // 传消息给dialog设置
    if( g_SettingsDlg.IsActive() )
    {
        g_SettingsDlg.MsgProc( hWnd, uMsg, wParam, lParam );
        return 0;
    }

    // 让对话框有机会首先处理消息
    *pbNoFurtherProcessing = g_HUD.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;
    *pbNoFurtherProcessing = g_SampleUI.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;

    // 为相机添加监听器，响应用户鼠标输入
    g_Camera.HandleMessages( hWnd, uMsg, wParam, lParam );

    return 0;
}


//--------------------------------------------------------------------------------------
// 操作键的消息
//--------------------------------------------------------------------------------------
void CALLBACK OnKeyboard( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext )
{
    if( bKeyDown )
    {
        switch( nChar )
        {
            case VK_F1:
				in_wfc();
                break;
        }
    }
}


//--------------------------------------------------------------------------------------
// 处理GUI事件
//--------------------------------------------------------------------------------------
void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext )
{
    switch( nControlID )
    {
        case IDC_TOGGLEFULLSCREEN:
            DXUTToggleFullScreen();
            break;
        case IDC_WFC:
            break;
        case IDC_CHANGEDEVICE:
            g_SettingsDlg.SetActive( !g_SettingsDlg.IsActive() );
            break;

        case IDC_TOGGLESPIN:
        {
            g_bSpinning = g_SampleUI.GetCheckBox( IDC_TOGGLESPIN )->GetChecked();
            break;
        }

        //case IDC_PUFF_SCALE:
        //{
        //    WCHAR sz[100];
        //    g_fModelPuffiness = ( float )( g_SampleUI.GetSlider( IDC_PUFF_SCALE )->GetValue() * 0.01f );
        //    swprintf_s( sz, 100, L"Puffiness: %0.2f", g_fModelPuffiness );
        //    g_SampleUI.GetStatic( IDC_PUFF_STATIC )->SetText( sz );
        //    break;
        //}

    }
}


//--------------------------------------------------------------------------------------
// 设置移除时调用
//--------------------------------------------------------------------------------------
bool CALLBACK OnDeviceRemoved( void* pUserContext )
{
    return true;
}


//--------------------------------------------------------------------------------------
// 初始化，并进入循环
//--------------------------------------------------------------------------------------
int WINAPI wWinMain( _In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow ){
    // 允许debug时候运行时内存的核对
#ifdef _DEBUG
    _CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif

	// DXUT将创建和使用最好的设备
    // 系统上可用，具体取决于下面设置的D3D回调

    // 设置普通的DXUT回调事件
    DXUTSetCallbackFrameMove( OnFrameMove );
    DXUTSetCallbackKeyboard( OnKeyboard );
    DXUTSetCallbackMsgProc( MsgProc );
    DXUTSetCallbackDeviceChanging( ModifyDeviceSettings );
    DXUTSetCallbackDeviceRemoved( OnDeviceRemoved );

    // 设置D3D11 DXUT回调
    DXUTSetCallbackD3D11DeviceAcceptable( IsD3D11DeviceAcceptable );
    DXUTSetCallbackD3D11DeviceCreated( OnD3D11CreateDevice );
    DXUTSetCallbackD3D11SwapChainResized( OnD3D11ResizedSwapChain );
    DXUTSetCallbackD3D11FrameRender( OnD3D11FrameRender );
    DXUTSetCallbackD3D11SwapChainReleasing( OnD3D11ReleasingSwapChain );
    DXUTSetCallbackD3D11DeviceDestroyed( OnD3D11DestroyDevice );

    // 进行任意的application-level 初始化

    DXUTInit( true, true, nullptr ); // 解析命令行，出错时显示msgboxes，没有额外的命令行参数
    DXUTSetCursorSettings( true, true ); // 全屏时显示光标

    InitApp();
    DXUTCreateWindow( L"wfc" );

    // 仅仅要求 DX 10-level 之后 
    DXUTCreateDevice( D3D_FEATURE_LEVEL_10_0, true, 800, 600 );
    DXUTMainLoop(); // 进入主循环

    // 进行任意 application-level 的清除

    return DXUTGetExitCode();
}


//--------------------------------------------------------------------------------------
// 整个程序的初始化 
//--------------------------------------------------------------------------------------
void InitApp()
{
    g_fModelPuffiness = 0.0f;
    g_bSpinning = false;
	//初始化dialog
    g_SettingsDlg.Init( &g_DialogResourceManager );
    g_HUD.Init( &g_DialogResourceManager );
    g_SampleUI.Init( &g_DialogResourceManager );

    g_HUD.SetCallback( OnGUIEvent ); int iY = 500;
    g_HUD.AddButton( IDC_TOGGLEFULLSCREEN, L"Toggle full screen", 0, iY, 170, 22 );
    g_HUD.AddButton( IDC_CHANGEDEVICE, L"Change device (F2)", 0, iY += 26, 170, 22, VK_F2 );
    g_HUD.AddButton( IDC_WFC, L"WFC (F3)", 0, iY += 26, 170, 22, VK_F3 );

    g_SampleUI.SetCallback( OnGUIEvent ); iY = 10;

    WCHAR sz[100];
    iY += 24;
	//滑块
    //swprintf_s( sz, 100, L"Puffiness: %0.2f", g_fModelPuffiness );
    //g_SampleUI.AddStatic( IDC_PUFF_STATIC, sz, 0 , iY += 26, 170, 22 );
    //g_SampleUI.AddSlider( IDC_PUFF_SCALE, 50, iY += 26, 100, 22, 0, 2000, ( int )( g_fModelPuffiness * 100.0f ) );

    iY += 24;
    g_SampleUI.AddCheckBox( IDC_TOGGLESPIN, L"Toggle Spinning", 0, iY += 26, 170, 22, g_bSpinning );
}

//---------------------------------------------------------------------------------------------
// 加入wfc接口
//---------------------------------------------------------------------------------------------

#include <iostream>
#include <fstream>
#include <string>
#include <chrono>
#include "genericwfc.hpp"
#include "rapidxml.hpp"
#include <random>
#include "array4d.hpp"
#include <optional>
#include "tilesmap.hpp"
#include <unordered_set>
#include "rapidxml_utils.hpp"
#include "model.hpp"

using namespace std;
using namespace rapidxml;

/**
* 将文字信息做一个转换
*/
Symmetry to_symmetry(const string &symmetry_name) {
	if (symmetry_name == "T") {
		return Symmetry::T;
	}
	if (symmetry_name == "L") {
		return Symmetry::L;
	}
	if (symmetry_name == "X") {
		return Symmetry::X;
	}
	if (symmetry_name == "I") {
		return Symmetry::I;
	}

	throw symmetry_name + "is an invalid Symmetry";
}

/**
* 读取瓷砖的名字
*/
std::optional<unordered_set<string>> read_subset_names(xml_node<> *root_node,
	const string &subset) {
	unordered_set<string> subset_names;
	xml_node<> *subsets_node = root_node->first_node("subsets");
	if (!subsets_node) {
		return std::nullopt;
	}
	xml_node<> *subset_node = subsets_node->first_node("subset");
	while (subset_node &&
		rapidxml::get_attribute(subset_node, "name") != subset) {
		subset_node = subset_node->next_sibling("subset");
	}
	if (!subset_node) {
		return std::nullopt;
	}
	for (xml_node<> *node = subset_node->first_node("tile"); node;
		node = node->next_sibling("tile")) {
		subset_names.insert(rapidxml::get_attribute(node, "name"));
	}
	return subset_names;
}


/**
* 读瓷砖
*/
unordered_map<string, Tile> read_tiles(xml_node<> * root_node,
	const string &current_dir,
	const string &subset,
	unsigned size) {
	std::optional<unordered_set<string>> subset_names =
		read_subset_names(root_node, subset);
	unordered_map<string, Tile> tiles;
	xml_node<> *tiles_node = root_node->first_node("tiles");
	for (xml_node<> *node = tiles_node->first_node("tile"); node; 
		node = node->next_sibling("tile")){
		string name = rapidxml::get_attribute(node, "name");
		if (subset_names != nullopt && 
			subset_names->find(name) == subset_names->end()){
			continue;
		}
		Symmetry symmetry =
			to_symmetry(rapidxml::get_attribute(node, "symmetry", "X"));
		double weight = stod(rapidxml::get_attribute(node, "weight", "1.0"));
		int low = stoi(rapidxml::get_attribute(node, "low", "0"));
		int high = stoi(rapidxml::get_attribute(node, "high", "999"));
		const std::string obj_path = current_dir + "/" + name + ".obj";
		optional<ObjModel> model = ReadModel(obj_path);

		if (model == nullopt){
			vector<ObjModel> models;
			for (unsigned i = 0; i < nb_of_possible_orientations(symmetry); i++){
				const std::string obj_path =
					current_dir + "/" + name + " " + to_string(i) + "obj";
				optional<ObjModel> model = ReadModel(obj_path);
				if (model == nullopt){
					throw "Error while loading" + obj_path;
				}
				models.push_back(*model);
			}
			Tile tile = { models, symmetry, weight, low, high };
			tiles.insert({ name, tile });
		}
		else {
			Tile tile( *model, symmetry, weight, low, high);
			tiles.insert({ name, tile });
		}
	}

	return tiles;
}

/**
* 读取约束条件
*/
vector<tuple<string, unsigned, string, unsigned, string>>
read_neighbors(xml_node<> *root_node) {
	vector<tuple<string, unsigned, string, unsigned, string>> neighbors;
	xml_node<> *neighbor_node = root_node->first_node("neighbors");
	for (xml_node<> *node = neighbor_node->first_node("neighbor"); node;
		node = node->next_sibling("neighbor")) {
		string left = rapidxml::get_attribute(node, "left");
		string::size_type left_delimiter = left.find(" ");
		string left_tile = left.substr(0, left_delimiter);
		unsigned left_orientation = 0;
		if (left_delimiter != string::npos) {
			left_orientation = stoi(left.substr(left_delimiter, string::npos));
		}

		string right = rapidxml::get_attribute(node, "right");
		string::size_type right_delimiter = right.find(" ");
		string right_tile = right.substr(0, right_delimiter);
		unsigned right_orientation = 0;
		if (right_delimiter != string::npos) {
			right_orientation = stoi(right.substr(right_delimiter, string::npos));
		}

		string state = rapidxml::get_attribute(node, "state");
		string::size_type state_delimiter = state.find(" ");
		string state_state = state.substr(0, state_delimiter);

		neighbors.push_back(
			{ left_tile, left_orientation, right_tile, right_orientation, state_state });
	}
	return neighbors;
}




void read_simpletiled_instance(xml_node<> *node, const string &current_dir) noexcept {
	
	/**
	* 读取需要处理的问题
	*/
	string name = rapidxml::get_attribute(node, "name");
	string subset = rapidxml::get_attribute(node, "subset", "tiles");
	bool periodic_output = (rapidxml::get_attribute(node, "periodic", "False") == "True");
	unsigned width = stoi(rapidxml::get_attribute(node, "width", "5"));
	unsigned height = stoi(rapidxml::get_attribute(node, "height", "5"));
	unsigned depth = stoi(rapidxml::get_attribute(node, "depth", "5"));

	/**
	* 读取约束条件
	*/
	ifstream config_file("../../Media/test/data.xml");
	vector<char> buffer((istreambuf_iterator<char>(config_file)), istreambuf_iterator<char>());
	buffer.push_back('\0');
	xml_document<> data_document;
	data_document.parse<0>(&buffer[0]);
	xml_node<> *data_root_node = data_document.first_node("set");
	unsigned size = stoi(rapidxml::get_attribute(data_root_node, "size"));

	unordered_map<string, Tile> tiles_map =
		read_tiles(data_root_node, "../../Media/test", subset, size);
	unordered_map<string, unsigned> tiles_id;
	vector<Tile> tiles;
	unsigned id = 0;
	for (pair<string, Tile> tile : tiles_map){
		tiles_id.insert({ tile.first, id });
		tiles.push_back(tile.second);
		id++;
	}

	vector<tuple<string, unsigned, string, unsigned, string>> neighbors =
		read_neighbors(data_root_node);
	vector<tuple<unsigned, unsigned, unsigned, unsigned, unsigned>> neighbors_ids;
	for (auto neighbor : neighbors) {
		const string &neighbor1 = get<0>(neighbor);
		const int &orientation1 = get<1>(neighbor);
		const string &neighbor2 = get<2>(neighbor);
		const int &orientation2 = get<3>(neighbor);
		const string &state = get<4>(neighbor);
		if (tiles_id.find(neighbor1) == tiles_id.end()) {
			continue;
		}
		if (tiles_id.find(neighbor2) == tiles_id.end()) {
			continue;
		}
		int horizontal = 0;
		if (state == "horizontal") horizontal = 1;
		neighbors_ids.push_back(make_tuple(tiles_id[neighbor1], orientation1,
			tiles_id[neighbor2], orientation2, horizontal));
	}

	for (unsigned test = 0; test < 10; test++){
		int seed = random_device()();
		TilingWFC<ObjModel> wfc(tiles, neighbors_ids, height, width, depth, { periodic_output }, seed);
		std::optional<ObjModel> success = wfc.run();
		if (success.has_value()){
			WriteModel("../results/" + name + ".obj", *success);
			cout << name << "finished!" << endl;
			break;
		}
		else{
			cout << "failed!" << endl;
		}
	}
}


void read_config_file(const string& config_path) noexcept {
	xml_document<> doc;
	xml_node<>* root_node;
	ifstream config_file(config_path);
	vector<char> buffer((istreambuf_iterator<char>(config_file)), istreambuf_iterator<char>());
	buffer.push_back('\0');
	doc.parse<0>(&buffer[0]);
	root_node = doc.first_node("samples");
	string dir_path = "../../Media/test";
	for (xml_node<> * node = root_node->first_node("simpletiled"); node; node = node->next_sibling("simpletiled")){
		read_simpletiled_instance(node, dir_path);
	}
}


void in_wfc() {
	std::chrono::time_point<std::chrono::system_clock> start, end;
	start = std::chrono::system_clock::now();
	read_config_file("samples.xml");
	end = std::chrono::system_clock::now();
	int elapsed_s = std::chrono::duration_cast<std::chrono::seconds> (end - start).count();
	int elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds> (end - start).count();
	std::cout << "All done in" << elapsed_s << "s, " << elapsed_ms % 1000 << "ms.\n";
}