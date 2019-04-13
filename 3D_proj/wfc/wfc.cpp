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
// �ṹ��
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
// ȫ�ֱ���
//--------------------------------------------------------------------------------------
CModelViewerCamera          g_Camera;               // viewing camera��
CDXUTDialogResourceManager  g_DialogResourceManager; // ����dialog���������������Ϣ�Ϳ�����Դ
CD3DSettingsDlg             g_SettingsDlg;          // �豸���öԻ���
CDXUTTextHelper*            g_pTxtHelper = nullptr; // ����test��
CDXUTDialog                 g_HUD;                  // ����3d�豸
CDXUTDialog                 g_SampleUI;             // ����������Ŀ���

// Direct3D 11 ��Դ����
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
// ��ǰ����
//--------------------------------------------------------------------------------------
void RenderText();
void InitApp();


//--------------------------------------------------------------------------------------
// ����false �ɾܾ������е��κ�D3D11 �豸
//--------------------------------------------------------------------------------------
bool CALLBACK IsD3D11DeviceAcceptable( const CD3D11EnumAdapterInfo *AdapterInfo, UINT Output, const CD3D11EnumDeviceInfo *DeviceInfo,
                                       DXGI_FORMAT BackBufferFormat, bool bWindowed, void* pUserContext )
{
    return true;
}


//--------------------------------------------------------------------------------------
// �ڴ���һ��D3D�豸֮ǰ���ã�����APP�������޸��豸
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext )
{
    return true;
}


//--------------------------------------------------------------------------------------
// �����κβ������ڱ��󻺴����Դ
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D11CreateDevice( ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc,
                                      void* pUserContext )
{
    HRESULT hr = S_OK;

    auto pd3dImmediateContext = DXUTGetD3D11DeviceContext();
	//��ʼ����Դ������
    V_RETURN( g_DialogResourceManager.OnD3D11CreateDevice( pd3dDevice, pd3dImmediateContext ) );
    V_RETURN( g_SettingsDlg.OnD3D11CreateDevice( pd3dDevice ) );
    g_pTxtHelper = new CDXUTTextHelper( pd3dDevice, pd3dImmediateContext, &g_DialogResourceManager, 15 );

    DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;

#ifdef _DEBUG

    dwShaderFlags |= D3DCOMPILE_DEBUG;
    dwShaderFlags |= D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    // ����vertex shader
    ID3DBlob* pVSBlob = nullptr;
    V_RETURN( DXUTCompileFromFile( L"wfc.fx", nullptr, "VS", "vs_4_0", dwShaderFlags, 0, &pVSBlob ) );

    // ����vertex shader
    hr = pd3dDevice->CreateVertexShader( pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), nullptr, &g_pVertexShader );
    if( FAILED( hr ) )
    {    
        SAFE_RELEASE( pVSBlob );
        return hr;
    }

    // �������벼��
    D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    UINT numElements = ARRAYSIZE( layout );

    // �������벼��
    hr = pd3dDevice->CreateInputLayout( layout, numElements, pVSBlob->GetBufferPointer(),
                                        pVSBlob->GetBufferSize(), &g_pVertexLayout );
    SAFE_RELEASE( pVSBlob );
    if( FAILED( hr ) )
        return hr;

    // �������벼��
    pd3dImmediateContext->IASetInputLayout( g_pVertexLayout );

    // ����pixel shader
    ID3DBlob* pPSBlob  = nullptr;
    V_RETURN( DXUTCompileFromFile( L"wfc.fx", nullptr, "PS", "ps_4_0", dwShaderFlags, 0, &pPSBlob  ) );

    // ����pixel shader
    hr = pd3dDevice->CreatePixelShader( pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), nullptr, &g_pPixelShader );
    SAFE_RELEASE( pPSBlob );
    if( FAILED( hr ) )
        return hr;

    // ����ģ��
    V_RETURN( g_Mesh.Create( pd3dDevice, L"Tiny\\tiny.sdkmesh" ) );
	
    // ������������
    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.ByteWidth = sizeof(CBChangesEveryFrame);
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    V_RETURN( pd3dDevice->CreateBuffer( &bd, nullptr, &g_pCBChangesEveryFrame ) );

    bd.ByteWidth = sizeof(CBNeverChanges);
    V_RETURN( pd3dDevice->CreateBuffer( &bd, nullptr, &g_pCBNeverChanges ) );

	//���³��������е�����
    D3D11_MAPPED_SUBRESOURCE MappedResource;
    V( pd3dImmediateContext->Map( g_pCBNeverChanges, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource ) );
    auto pCB = reinterpret_cast<CBNeverChanges*>( MappedResource.pData );
    XMStoreFloat3( reinterpret_cast<XMFLOAT3*>( &pCB->vLightDir ), s_LightDir );
    pCB->vLightDir.w = 1.f;
    pd3dImmediateContext->Unmap( g_pCBNeverChanges , 0 );

    // ����������ʽ
    D3D11_SAMPLER_DESC sampDesc = {};
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampDesc.MinLOD = 0;
    sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
    V_RETURN( pd3dDevice->CreateSamplerState( &sampDesc, &g_pSamplerLinear ) );

    // ��ʼ������任����
    g_World = XMMatrixIdentity();

    //���������ͼ��λ��
    static const XMVECTORF32 s_Eye = { 0.0f, 0.0f, -800.0f, 0.f };
    static const XMVECTORF32 s_At = { 0.0f, 1.0f, 0.0f, 0.f };
    g_Camera.SetViewParams( s_Eye, s_At );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// �����κ������ڱ��󻺴����Դ
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D11ResizedSwapChain( ID3D11Device* pd3dDevice, IDXGISwapChain* pSwapChain,
                                          const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext )
{
    HRESULT hr;

    V_RETURN( g_DialogResourceManager.OnD3D11ResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc ) );
    V_RETURN( g_SettingsDlg.OnD3D11ResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc ) );

    //����������ӽǡ�Զ�����ý���
    float fAspectRatio = pBackBufferSurfaceDesc->Width / ( FLOAT )pBackBufferSurfaceDesc->Height;
    g_Camera.SetProjParams( XM_PI / 4, fAspectRatio, 0.1f, 1000.0f );
    g_Camera.SetWindow( pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height );
	//�����򵥵�����¼�����
    g_Camera.SetButtonMasks( MOUSE_LEFT_BUTTON, MOUSE_WHEEL, MOUSE_MIDDLE_BUTTON );

	//�������Ի��������Ļ��
    g_HUD.SetLocation( pBackBufferSurfaceDesc->Width - 170, 0 );
    g_HUD.SetSize( 170, 170 );
    g_SampleUI.SetLocation( pBackBufferSurfaceDesc->Width - 170, pBackBufferSurfaceDesc->Height - 300 );
    g_SampleUI.SetSize( 170, 300 );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// ���³����ľ��
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
// ��Ⱦ�ı�
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
//	ÿ֡��Ⱦ����
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
    // ������󻺴�
    //
    auto pRTV = DXUTGetD3D11RenderTargetView();
    pd3dImmediateContext->ClearRenderTargetView( pRTV, Colors::RosyBrown );

    //
    // �����Ⱥͱ��󻺴�
    //
    auto pDSV = DXUTGetD3D11DepthStencilView();
    pd3dImmediateContext->ClearDepthStencilView( pDSV, D3D11_CLEAR_DEPTH, 1.0, 0 );
	//��ȡ���ת����ľ���
    XMMATRIX mView = g_Camera.GetViewMatrix();
    XMMATRIX mProj = g_Camera.GetProjMatrix();
    XMMATRIX mWorldViewProjection = g_World * mView * mProj;

    // ���³���������Դ
    HRESULT hr;
    D3D11_MAPPED_SUBRESOURCE MappedResource;
    V( pd3dImmediateContext->Map( g_pCBChangesEveryFrame , 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource ) );
    auto pCB = reinterpret_cast<CBChangesEveryFrame*>( MappedResource.pData );
    XMStoreFloat4x4( &pCB->mWorldViewProj, XMMatrixTranspose( mWorldViewProjection ) );
    XMStoreFloat4x4( &pCB->mWorld, XMMatrixTranspose( g_World ) );
    pCB->vMisc.x = g_fModelPuffiness;
    pd3dImmediateContext->Unmap( g_pCBChangesEveryFrame , 0 );

    //
    // ���ö������벼��
    //
    pd3dImmediateContext->IASetInputLayout( g_pVertexLayout );

    //
    // ��Ⱦģ��
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

        // ����ģ�Ͳ�����Ϣ��ֻ����򵥵���ɫ��
        auto pDiffuseRV = g_Mesh.GetMaterial( pSubset->MaterialID )->pDiffuseRV11;
        pd3dImmediateContext->PSSetShaderResources( 0, 1, &pDiffuseRV );

        pd3dImmediateContext->DrawIndexed( ( UINT )pSubset->IndexCount, 0, ( UINT )pSubset->VertexStart );
    }
	//��Ⱦ�Ի���
    g_HUD.OnRender( fElapsedTime );
    g_SampleUI.OnRender( fElapsedTime );
    RenderText();
}


//--------------------------------------------------------------------------------------
// �ͷ���OnD3D11ResizedSwapChain�д�������Դ 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11ReleasingSwapChain( void* pUserContext )
{
    g_DialogResourceManager.OnD3D11ReleasingSwapChain();
}


//--------------------------------------------------------------------------------------
// �ͷ���OnD3D11CreateDevice�д�������Դ 
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
//�����ڽ��յ���Ϣʱ���ô˺���
//--------------------------------------------------------------------------------------
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
                          bool* pbNoFurtherProcessing, void* pUserContext )
{
	//����Ϣ���Ի�����Դ����������֤GUI��������
    *pbNoFurtherProcessing = g_DialogResourceManager.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;

    // ����Ϣ��dialog����
    if( g_SettingsDlg.IsActive() )
    {
        g_SettingsDlg.MsgProc( hWnd, uMsg, wParam, lParam );
        return 0;
    }

    // �öԻ����л������ȴ�����Ϣ
    *pbNoFurtherProcessing = g_HUD.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;
    *pbNoFurtherProcessing = g_SampleUI.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;

    // Ϊ�����Ӽ���������Ӧ�û��������
    g_Camera.HandleMessages( hWnd, uMsg, wParam, lParam );

    return 0;
}


//--------------------------------------------------------------------------------------
// ����������Ϣ
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
// ����GUI�¼�
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
// �����Ƴ�ʱ����
//--------------------------------------------------------------------------------------
bool CALLBACK OnDeviceRemoved( void* pUserContext )
{
    return true;
}


//--------------------------------------------------------------------------------------
// ��ʼ����������ѭ��
//--------------------------------------------------------------------------------------
int WINAPI wWinMain( _In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow ){
    // ����debugʱ������ʱ�ڴ�ĺ˶�
#ifdef _DEBUG
    _CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif

	// DXUT��������ʹ����õ��豸
    // ϵͳ�Ͽ��ã�����ȡ�����������õ�D3D�ص�

    // ������ͨ��DXUT�ص��¼�
    DXUTSetCallbackFrameMove( OnFrameMove );
    DXUTSetCallbackKeyboard( OnKeyboard );
    DXUTSetCallbackMsgProc( MsgProc );
    DXUTSetCallbackDeviceChanging( ModifyDeviceSettings );
    DXUTSetCallbackDeviceRemoved( OnDeviceRemoved );

    // ����D3D11 DXUT�ص�
    DXUTSetCallbackD3D11DeviceAcceptable( IsD3D11DeviceAcceptable );
    DXUTSetCallbackD3D11DeviceCreated( OnD3D11CreateDevice );
    DXUTSetCallbackD3D11SwapChainResized( OnD3D11ResizedSwapChain );
    DXUTSetCallbackD3D11FrameRender( OnD3D11FrameRender );
    DXUTSetCallbackD3D11SwapChainReleasing( OnD3D11ReleasingSwapChain );
    DXUTSetCallbackD3D11DeviceDestroyed( OnD3D11DestroyDevice );

    // ���������application-level ��ʼ��

    DXUTInit( true, true, nullptr ); // ���������У�����ʱ��ʾmsgboxes��û�ж���������в���
    DXUTSetCursorSettings( true, true ); // ȫ��ʱ��ʾ���

    InitApp();
    DXUTCreateWindow( L"wfc" );

    // ����Ҫ�� DX 10-level ֮�� 
    DXUTCreateDevice( D3D_FEATURE_LEVEL_10_0, true, 800, 600 );
    DXUTMainLoop(); // ������ѭ��

    // �������� application-level �����

    return DXUTGetExitCode();
}


//--------------------------------------------------------------------------------------
// ��������ĳ�ʼ�� 
//--------------------------------------------------------------------------------------
void InitApp()
{
    g_fModelPuffiness = 0.0f;
    g_bSpinning = false;
	//��ʼ��dialog
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
	//����
    //swprintf_s( sz, 100, L"Puffiness: %0.2f", g_fModelPuffiness );
    //g_SampleUI.AddStatic( IDC_PUFF_STATIC, sz, 0 , iY += 26, 170, 22 );
    //g_SampleUI.AddSlider( IDC_PUFF_SCALE, 50, iY += 26, 100, 22, 0, 2000, ( int )( g_fModelPuffiness * 100.0f ) );

    iY += 24;
    g_SampleUI.AddCheckBox( IDC_TOGGLESPIN, L"Toggle Spinning", 0, iY += 26, 170, 22, g_bSpinning );
}

//---------------------------------------------------------------------------------------------
// ����wfc�ӿ�
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
* ��������Ϣ��һ��ת��
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
* ��ȡ��ש������
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
* ����ש
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
* ��ȡԼ������
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
	* ��ȡ��Ҫ���������
	*/
	string name = rapidxml::get_attribute(node, "name");
	string subset = rapidxml::get_attribute(node, "subset", "tiles");
	bool periodic_output = (rapidxml::get_attribute(node, "periodic", "False") == "True");
	unsigned width = stoi(rapidxml::get_attribute(node, "width", "5"));
	unsigned height = stoi(rapidxml::get_attribute(node, "height", "5"));
	unsigned depth = stoi(rapidxml::get_attribute(node, "depth", "5"));

	/**
	* ��ȡԼ������
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