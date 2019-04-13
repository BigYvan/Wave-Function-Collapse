//--------------------------------------------------------------------------------------
// File: WFC_2D.cpp
//
// Basic introduction to DXUT
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "DXUTcamera.h"
#include "DXUTgui.h"
#include "DXUTsettingsDlg.h"
#include "SDKmisc.h"
#include "SDKmesh.h"
//--------------------------------------------------------------------------------------
//加入wfc算法头文件
//--------------------------------------------------------------------------------------
#include "stdafx.h"
#include <iostream>
#include <fstream>
#include <string>
#include <chrono>
#include "wfc.hpp"
#include "overlapping_wfc.hpp"
#include "lib/rapidxml.hpp"
#include <random>
#include "utils/image.hpp"
#include "utils/array3D.hpp"
#include <optional>
//----------------------------------------------------------------------------------------
//by xgy  2018.07.19
//----------------------------------------------------------------------------------------
#include "tilemap.hpp"
#include <unordered_set>
#include "utils/utils.hpp"
#include "utils/rapidxml_utils.hpp"

#pragma warning( disable : 4100 )

using namespace DirectX;

//--------------------------------------------------------------------------------------
// 结构体
//--------------------------------------------------------------------------------------
struct SimpleVertex
{
	XMFLOAT3 Pos;
	XMFLOAT2 Tex;
};

struct CBChangesEveryFrame
{
	XMFLOAT4X4 mWorldViewProj;
	XMFLOAT4X4 mWorld;
	XMFLOAT4 vMeshColor;
};


CModelViewerCamera          g_Camera;               // viewing camera类
CDXUTDialogResourceManager  g_DialogResourceManager; // 声明dialog类控制器，传递消息和控制资源
CD3DSettingsDlg             g_SettingsDlg;          // 设备设置对话框
CDXUTTextHelper*            g_pTxtHelper = nullptr; // 绘制test类
CDXUTDialog                 g_HUD;                  // 控制3d设备

//--------------------------------------------------------------------------------------
// 全局变量
//--------------------------------------------------------------------------------------

ID3D11VertexShader*         g_pVertexShader = nullptr;
ID3D11PixelShader*          g_pPixelShader = nullptr;
ID3D11InputLayout*          g_pVertexLayout = nullptr;
ID3D11Buffer*               g_pVertexBuffer = nullptr;
ID3D11Buffer*               g_pIndexBuffer = nullptr;
ID3D11Buffer*               g_pCBChangesEveryFrame = nullptr;
ID3D11ShaderResourceView*   g_pTextureRV = nullptr;
ID3D11SamplerState*         g_pSamplerLinear = nullptr;
XMMATRIX                    g_World;
XMMATRIX                    g_View;
XMMATRIX                    g_Projection;
XMFLOAT4                    g_vMeshColor(0.7f, 0.7f, 0.7f, 1.0f);

//--------------------------------------------------------------------------------------
// 提前声明
//--------------------------------------------------------------------------------------
void in_wfc();
int flag = 0;

//--------------------------------------------------------------------------------------
// UI IDs
//--------------------------------------------------------------------------------------
#define IDC_TOGGLEFULLSCREEN    1
#define IDC_WFC                 2
#define IDC_CHANGEDEVICE        3


//--------------------------------------------------------------------------------------
// 返回false 可拒绝不可行的任何D3D11 设备
//--------------------------------------------------------------------------------------
bool CALLBACK IsD3D11DeviceAcceptable(const CD3D11EnumAdapterInfo *AdapterInfo, UINT Output, const CD3D11EnumDeviceInfo *DeviceInfo,
	DXGI_FORMAT BackBufferFormat, bool bWindowed, void* pUserContext)
{
	return true;
}


//--------------------------------------------------------------------------------------
//	在创建一个D3D设备之前调用，允许APP按需求修改设备
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings(DXUTDeviceSettings* pDeviceSettings, void* pUserContext)
{
	return true;
}


//--------------------------------------------------------------------------------------
//	创建任何不依赖于背后缓存的资源
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D11CreateDevice(ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc,
	void* pUserContext)
{
	HRESULT hr = S_OK;

	auto pd3dImmediateContext = DXUTGetD3D11DeviceContext();


	DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
	dwShaderFlags |= D3DCOMPILE_DEBUG;

	dwShaderFlags |= D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

	//  组建加载顶点着色器
	ID3DBlob* pVSBlob = nullptr;
	V_RETURN(DXUTCompileFromFile(L"WFC_2D.fx", nullptr, "VS", "vs_4_0", dwShaderFlags, 0, &pVSBlob));

	//	创建顶点着色器
	hr = pd3dDevice->CreateVertexShader(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), nullptr, &g_pVertexShader);
	if (FAILED(hr))
	{
		SAFE_RELEASE(pVSBlob);
		return hr;
	}

	// 定义输入布局
	D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	UINT numElements = ARRAYSIZE(layout);

	// 创建输入布局
	hr = pd3dDevice->CreateInputLayout(layout, numElements, pVSBlob->GetBufferPointer(),
		pVSBlob->GetBufferSize(), &g_pVertexLayout);
	SAFE_RELEASE(pVSBlob);
	if (FAILED(hr))
		return hr;

	// 设置输入布局
	pd3dImmediateContext->IASetInputLayout(g_pVertexLayout);

	// 组建加载像素着色器
	ID3DBlob* pPSBlob = nullptr;
	V_RETURN(DXUTCompileFromFile(L"WFC_2D.fx", nullptr, "PS", "ps_4_0", dwShaderFlags, 0, &pPSBlob));

	// 创建像素着色器
	hr = pd3dDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), nullptr, &g_pPixelShader);
	SAFE_RELEASE(pPSBlob);
	if (FAILED(hr))
		return hr;

	// 创建顶点
	SimpleVertex vertices[] =
	{

		{ XMFLOAT3(-2.0f, -2.0f, -2.0f), XMFLOAT2(0.0f, 1.0f) },
		{ XMFLOAT3(2.0f, -2.0f, -2.0f), XMFLOAT2(1.0f, 1.0f) },
		{ XMFLOAT3(2.0f, 2.0f, -2.0f), XMFLOAT2(1.0f, 0.0f) },
		{ XMFLOAT3(-2.0f, 2.0f, -2.0f), XMFLOAT2(0.0f, 0.0f) },

	};

	D3D11_BUFFER_DESC bd = {};
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(SimpleVertex) * 24;
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = 0;

	D3D11_SUBRESOURCE_DATA InitData = {};
	InitData.pSysMem = vertices;
	V_RETURN(pd3dDevice->CreateBuffer(&bd, &InitData, &g_pVertexBuffer));

	// Set vertex buffer
	UINT stride = sizeof(SimpleVertex);
	UINT offset = 0;
	pd3dImmediateContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);

	// 定义顶点索引
	DWORD indices[] =
	{
		3,1,0,
		2,1,3,
	};

	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(DWORD) * 36;
	bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
	bd.CPUAccessFlags = 0;
	bd.MiscFlags = 0;
	InitData.pSysMem = indices;
	V_RETURN(pd3dDevice->CreateBuffer(&bd, &InitData, &g_pIndexBuffer));

	// Set index buffer
	pd3dImmediateContext->IASetIndexBuffer(g_pIndexBuffer, DXGI_FORMAT_R32_UINT, 0);

	// Set primitive topology
	pd3dImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// 创建常量缓存
	bd.Usage = D3D11_USAGE_DYNAMIC;
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	bd.ByteWidth = sizeof(CBChangesEveryFrame);
	V_RETURN(pd3dDevice->CreateBuffer(&bd, nullptr, &g_pCBChangesEveryFrame));

	// 初始化世界变换矩阵
	g_World = XMMatrixIdentity();

	// Initialize the view matrix
	static const XMVECTORF32 s_Eye = { 0.0f, 0.0f, -7.0f, 0.f };
	static const XMVECTORF32 s_At = { 0.0f, 0.0f, 0.0f, 0.f };
	static const XMVECTORF32 s_Up = { 0.0f, 1.0f, 0.0f, 0.f };
	g_View = XMMatrixLookAtLH(s_Eye, s_At, s_Up);

	// 加载纹理
	V_RETURN(DXUTCreateShaderResourceViewFromFile(pd3dDevice, L"samples\\Office.png", &g_pTextureRV));


	// 创建采样方式
	D3D11_SAMPLER_DESC sampDesc = {};
	sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	sampDesc.MinLOD = 0;
	sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
	V_RETURN(pd3dDevice->CreateSamplerState(&sampDesc, &g_pSamplerLinear));

	return S_OK;
}


//--------------------------------------------------------------------------------------
// 创建任何依赖于背后缓存的资源
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D11ResizedSwapChain(ID3D11Device* pd3dDevice, IDXGISwapChain* pSwapChain,
	const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext)
{
	float fAspect = static_cast<float>(pBackBufferSurfaceDesc->Width) / static_cast<float>(pBackBufferSurfaceDesc->Height);
	g_Projection = XMMatrixPerspectiveFovLH(XM_PI * 0.25f, fAspect, 0.1f, 100.0f);

	return S_OK;
}


//--------------------------------------------------------------------------------------
// 更新场景的句柄
//--------------------------------------------------------------------------------------
void CALLBACK OnFrameMove(double fTime, float fElapsedTime, void* pUserContext)
{
	//// Rotate cube around the origin
	//g_World = XMMatrixRotationY( 60.0f * XMConvertToRadians((float)fTime) );

	//// Modify the color
	//g_vMeshColor.x = ( sinf( ( float )fTime * 1.0f ) + 1.0f ) * 0.5f;
	//g_vMeshColor.y = ( cosf( ( float )fTime * 3.0f ) + 1.0f ) * 0.5f;
	//g_vMeshColor.z = ( sinf( ( float )fTime * 5.0f ) + 1.0f ) * 0.5f;
}


//--------------------------------------------------------------------------------------
// 每帧渲染场景
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11FrameRender(ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dImmediateContext,
	double fTime, float fElapsedTime, void* pUserContext)
{
	//
	// Clear the back buffer
	//
	auto pRTV = DXUTGetD3D11RenderTargetView();
	pd3dImmediateContext->ClearRenderTargetView(pRTV, Colors::MidnightBlue);

	//
	// Clear the depth stencil
	//
	auto pDSV = DXUTGetD3D11DepthStencilView();
	pd3dImmediateContext->ClearDepthStencilView(pDSV, D3D11_CLEAR_DEPTH, 1.0, 0);

	XMMATRIX mWorldViewProjection = g_World * g_View * g_Projection;

	// Update constant buffer that changes once per frame
	HRESULT hr;
	D3D11_MAPPED_SUBRESOURCE MappedResource;
	V(pd3dImmediateContext->Map(g_pCBChangesEveryFrame, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource));
	auto pCB = reinterpret_cast<CBChangesEveryFrame*>(MappedResource.pData);
	XMStoreFloat4x4(&pCB->mWorldViewProj, XMMatrixTranspose(mWorldViewProjection));
	XMStoreFloat4x4(&pCB->mWorld, XMMatrixTranspose(g_World));
	pCB->vMeshColor = g_vMeshColor;
	pd3dImmediateContext->Unmap(g_pCBChangesEveryFrame, 0);

	//
	// Render the cube
	//
	pd3dImmediateContext->VSSetShader(g_pVertexShader, nullptr, 0);
	pd3dImmediateContext->VSSetConstantBuffers(0, 1, &g_pCBChangesEveryFrame);
	pd3dImmediateContext->PSSetShader(g_pPixelShader, nullptr, 0);
	pd3dImmediateContext->PSSetConstantBuffers(0, 1, &g_pCBChangesEveryFrame);
	pd3dImmediateContext->PSSetShaderResources(0, 1, &g_pTextureRV);
	pd3dImmediateContext->PSSetSamplers(0, 1, &g_pSamplerLinear);
	pd3dImmediateContext->DrawIndexed(36, 0, 0);

	if (flag == 1)
	{
		DXUTCreateShaderResourceViewFromFile(pd3dDevice, L"results\\Summer_tiles.png", &g_pTextureRV);
	}
}


//--------------------------------------------------------------------------------------
// 释放在OnD3D11ResizedSwapChain中创建的资源  
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11ReleasingSwapChain(void* pUserContext)
{
}


//--------------------------------------------------------------------------------------
// 释放在OnD3D11CreateDevice中创建的资源 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11DestroyDevice(void* pUserContext)
{
	SAFE_RELEASE(g_pVertexBuffer);
	SAFE_RELEASE(g_pIndexBuffer);
	SAFE_RELEASE(g_pVertexLayout);
	SAFE_RELEASE(g_pTextureRV);
	SAFE_RELEASE(g_pVertexShader);
	SAFE_RELEASE(g_pPixelShader);
	SAFE_RELEASE(g_pCBChangesEveryFrame);
	SAFE_RELEASE(g_pSamplerLinear);
}


//--------------------------------------------------------------------------------------
// 当窗口接收到消息时调用此函数
//--------------------------------------------------------------------------------------
LRESULT CALLBACK MsgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
	bool* pbNoFurtherProcessing, void* pUserContext)
{
	return 0;
}


//--------------------------------------------------------------------------------------
// 操作键的消息
//--------------------------------------------------------------------------------------
void CALLBACK OnKeyboard(UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext)
{
	if (bKeyDown)
	{
		switch (nChar)
		{
		case VK_F1:
			in_wfc();
			break;
		}
	}
}


//--------------------------------------------------------------------------------------
// 设置移除时调用
//--------------------------------------------------------------------------------------
bool CALLBACK OnDeviceRemoved(void* pUserContext)
{
	return true;
}


//--------------------------------------------------------------------------------------
// 初始化，并进入循环
//--------------------------------------------------------------------------------------
int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
	// 允许debug时候运行时内存的核对
#ifdef _DEBUG
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	// DXUT将创建和使用最好的设备
	// 系统上可用，具体取决于下面设置的D3D回调

	// 设置普通的DXUT回调事件
	DXUTSetCallbackFrameMove(OnFrameMove);
	DXUTSetCallbackKeyboard(OnKeyboard);
	DXUTSetCallbackMsgProc(MsgProc);
	DXUTSetCallbackDeviceChanging(ModifyDeviceSettings);
	DXUTSetCallbackDeviceRemoved(OnDeviceRemoved);

	// 设置D3D11 DXUT回调
	DXUTSetCallbackD3D11DeviceAcceptable(IsD3D11DeviceAcceptable);
	DXUTSetCallbackD3D11DeviceCreated(OnD3D11CreateDevice);
	DXUTSetCallbackD3D11SwapChainResized(OnD3D11ResizedSwapChain);
	DXUTSetCallbackD3D11FrameRender(OnD3D11FrameRender);
	DXUTSetCallbackD3D11SwapChainReleasing(OnD3D11ReleasingSwapChain);
	DXUTSetCallbackD3D11DeviceDestroyed(OnD3D11DestroyDevice);

	// 进行任意的application-level 初始化

	DXUTInit(true, true, nullptr); // 解析命令行，出错时显示msgboxes，没有额外的命令行参数
	DXUTSetCursorSettings(true, true); // 全屏时显示光标

	DXUTCreateWindow(L"WFC_2D");

	// 仅仅要求 10-level 之后
	DXUTCreateDevice(D3D_FEATURE_LEVEL_10_0, true, 800, 600);
	DXUTMainLoop(); // 进入主循环

					// 进行任意 application-level 清除

	return DXUTGetExitCode();
}



//----------------------------------------------------------------------------------------------------------------
//读入xml文件，执行wfc算法
//----------------------------------------------------------------------------------------------------------------
using namespace std;
using namespace rapidxml;


/**
* 重叠模型
*/
void read_overlapping_element(xml_node<>* node) noexcept {
	string name = rapidxml::get_attribute(node, "name");
	unsigned N = stoi(rapidxml::get_attribute(node, "N"));
	bool periodic_output =
		(rapidxml::get_attribute(node, "periodic", "False") == "True");
	bool periodic_input =
		(rapidxml::get_attribute(node, "periodicInput", "True") == "True");
	bool ground = (stoi(rapidxml::get_attribute(node, "ground", "0")) != 0);
	unsigned symmetry = stoi(rapidxml::get_attribute(node, "symmetry", "8"));
	unsigned screenshots =
		stoi(rapidxml::get_attribute(node, "screenshots", "2"));
	unsigned width = stoi(rapidxml::get_attribute(node, "width", "48"));
	unsigned height = stoi(rapidxml::get_attribute(node, "height", "48"));

	cout << name << " started!" << endl;
	std::optional<Array2D<Color>> m = read_image("C:/Users/xugaoyuan/Desktop/wfc_2d_test/2D_test/samples/" + name + ".png");
	OverlappingWFCOptions options = { periodic_input, periodic_output, height, width, symmetry, ground, N };
	for (unsigned i = 0; i < screenshots; i++) {
		for (unsigned test = 0; test < 10; test++) {
			int seed = random_device()();
			OverlappingWFC<Color> wfc(*m, options, seed);
			std::optional<Array2D<Color>> success = wfc.run();
			if (success.has_value()) {
				write_image_png("C:/Users/xugaoyuan/Desktop/wfc_2d_test/2D_test/results/" + name + to_string(i) + ".png", *success);
				cout << name << " finished!" << endl;
				break;
			}
			else {
				cout << "failed!" << endl;
			}
		}
	}
}

//-----------------------------------------------------------------------------------------------

/**
* 将文字信息做一个转换
*/
Symmetry to_symmetry(const string &symmetry_name) {
	if (symmetry_name == "X") {
		return Symmetry::X;
	}
	if (symmetry_name == "T") {
		return Symmetry::T;
	}
	if (symmetry_name == "I") {
		return Symmetry::I;
	}
	if (symmetry_name == "L") {
		return Symmetry::L;
	}
	if (symmetry_name == "\\") {
		return Symmetry::backslash;
	}
	if (symmetry_name == "P") {
		return Symmetry::P;
	}
	throw symmetry_name + "is an invalid Symmetry";
}

/**
* 读取瓷砖名字
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
* 读取所有瓷砖
*/
unordered_map<string, Tile<Color>> read_tiles(xml_node<> *root_node,
	const string &current_dir,
	const string &subset,
	unsigned size) {
	std::optional<unordered_set<string>> subset_names =
		read_subset_names(root_node, subset);
	unordered_map<string, Tile<Color>> tiles;
	xml_node<> *tiles_node = root_node->first_node("tiles");
	for (xml_node<> *node = tiles_node->first_node("tile"); node;
		node = node->next_sibling("tile")) {
		string name = rapidxml::get_attribute(node, "name");
		if (subset_names != nullopt &&
			subset_names->find(name) == subset_names->end()) {
			continue;
		}
		Symmetry symmetry =
			to_symmetry(rapidxml::get_attribute(node, "symmetry", "X"));
		double weight = stod(rapidxml::get_attribute(node, "weight", "1.0"));
		const std::string image_path = current_dir + "/" + name + ".png";
		optional<Array2D<Color>> image = read_image(image_path);

		if (image == nullopt) {
			vector<Array2D<Color>> images;
			for (unsigned i = 0; i < nb_of_possible_orientations(symmetry); i++) {
				const std::string image_path =
					current_dir + "/" + name + " " + to_string(i) + ".png";
				optional<Array2D<Color>> image = read_image(image_path);
				if (image == nullopt) {
					throw "Error while loading " + image_path;
				}
				if ((image->width != size) || (image->height != size)) {
					throw "Image " + image_path + " has wrond size";
				}
				images.push_back(*image);
			}
			Tile<Color> tile = { images, symmetry, weight };
			tiles.insert({ name, tile });
		}
		else {
			if ((image->width != size) || (image->height != size)) {
				throw "Image " + image_path + " has wrong size";
			}

			Tile<Color> tile(*image, symmetry, weight);
			tiles.insert({ name, tile });
		}
	}

	return tiles;
}

/**
* 读取约束条件
*/
vector<tuple<string, unsigned, string, unsigned>>
read_neighbors(xml_node<> *root_node) {
	vector<tuple<string, unsigned, string, unsigned>> neighbors;
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
		neighbors.push_back(
			{ left_tile, left_orientation, right_tile, right_orientation });
	}
	return neighbors;
}

/**
* 读取tilemap参数进入算法
*/
void read_simpletiled_instance(xml_node<> *node,
	const string &current_dir) noexcept {
	string name = rapidxml::get_attribute(node, "name");
	string subset = rapidxml::get_attribute(node, "subset", "tiles");
	bool periodic_output =
		(rapidxml::get_attribute(node, "periodic", "False") == "True");
	unsigned width = stoi(rapidxml::get_attribute(node, "width", "48"));
	unsigned height = stoi(rapidxml::get_attribute(node, "height", "48"));

	cout << name << " " << subset << " started!" << endl;

	ifstream config_file("C:/Users/xugaoyuan/Desktop/wfc_2d_test/2D_test/samples/" + name + "/data.xml");
	vector<char> buffer((istreambuf_iterator<char>(config_file)),
		istreambuf_iterator<char>());
	buffer.push_back('\0');
	xml_document<> data_document;
	data_document.parse<0>(&buffer[0]);
	xml_node<> *data_root_node = data_document.first_node("set");
	unsigned size = stoi(rapidxml::get_attribute(data_root_node, "size"));

	unordered_map<string, Tile<Color>> tiles_map =
		read_tiles(data_root_node, current_dir + "/" + name, subset, size);
	unordered_map<string, unsigned> tiles_id;
	vector<Tile<Color>> tiles;
	unsigned id = 0;
	for (pair<string, Tile<Color>> tile : tiles_map) {
		tiles_id.insert({ tile.first, id });
		tiles.push_back(tile.second);
		id++;
	}

	vector<tuple<string, unsigned, string, unsigned>> neighbors =
		read_neighbors(data_root_node);
	vector<tuple<unsigned, unsigned, unsigned, unsigned>> neighbors_ids;
	for (auto neighbor : neighbors) {
		const string &neighbor1 = get<0>(neighbor);
		const int &orientation1 = get<1>(neighbor);
		const string &neighbor2 = get<2>(neighbor);
		const int &orientation2 = get<3>(neighbor);
		if (tiles_id.find(neighbor1) == tiles_id.end()) {
			continue;
		}
		if (tiles_id.find(neighbor2) == tiles_id.end()) {
			continue;
		}
		neighbors_ids.push_back(make_tuple(tiles_id[neighbor1], orientation1,
			tiles_id[neighbor2], orientation2));
	}

	for (unsigned test = 0; test < 10; test++) {
		int seed = random_device()();
		TilingWFC<Color> wfc(tiles, neighbors_ids, height, width, { periodic_output },
			seed);
		std::optional<Array2D<Color>> success = wfc.run();
		if (success.has_value()) {
			write_image_png("C:/Users/xugaoyuan/Desktop/wfc_2d_test/2D_test/results/" + name + "_" + subset + ".png", *success);
			cout << name << " finished!" << endl;
			break;
		}
		else {
			cout << "failed!" << endl;
		}
	}
}


void read_config_file(const string& config_path) noexcept {
	xml_document<> doc;
	xml_node<>* root_node;
	ifstream config_file(config_path);
	std::cout << "path test" << config_path << std::endl;
	vector<char> buffer((istreambuf_iterator<char>(config_file)), istreambuf_iterator<char>());
	buffer.push_back('\0');
	doc.parse<0>(&buffer[0]);
	root_node = doc.first_node("samples");
	string dir_path = "C:/Users/xugaoyuan/Desktop/wfc_2d_test/2D_test/samples";
	// overlapping 部分
	//for (xml_node<> * node = root_node->first_node("overlapping"); node; node = node->next_sibling("overlapping")) {
	//	read_overlapping_element(node);
	//}
	// 添加tilemap部分
	for (xml_node<> * node = root_node->first_node("simpletiled"); node; node = node->next_sibling("simpletiled")) {
		read_simpletiled_instance(node, dir_path);
	}
}

void in_wfc()
{
	std::chrono::time_point<std::chrono::system_clock> start, end;
	start = std::chrono::system_clock::now();

	read_config_file("C:/Users/xugaoyuan/Desktop/wfc_2d_test/2D_test/fastwfc/samples.xml");
	flag = 1;
	end = std::chrono::system_clock::now();
	int elapsed_s = std::chrono::duration_cast<std::chrono::seconds> (end - start).count();
	int elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds> (end - start).count();
	std::cout << "All samples done in " << elapsed_s << "s, " << elapsed_ms % 1000 << "ms.\n";
}
