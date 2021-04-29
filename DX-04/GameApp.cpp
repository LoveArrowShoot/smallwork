#include "GameApp.h"
#include "d3dUtil.h"
#include "DXTrace.h"
using namespace DirectX;

GameApp::GameApp(HINSTANCE hInstance)
	: D3DApp(hInstance),
	m_CameraMode(CameraMode::Free),
	m_SkyBoxMode(SkyBoxMode::Daylight),
	m_mode(mode::existing)
{
}

GameApp::~GameApp()
{
}

bool GameApp::Init()
{
	if (!D3DApp::Init())
		return false;

	// 务必先初始化所有渲染状态，以供下面的特效使用
	RenderStates::InitAll(m_pd3dDevice.Get());

	if (!m_BasicEffect.InitAll(m_pd3dDevice.Get()))
		return false;

	if (!m_SkyEffect.InitAll(m_pd3dDevice.Get()))
		return false;

	if (!InitResource())
		return false;

	// 初始化鼠标，键盘不需要
	m_pMouse->SetWindow(m_hMainWnd);
	m_pMouse->SetMode(DirectX::Mouse::MODE_ABSOLUTE);
	return true;
}

void GameApp::OnResize()
{
	assert(m_pd2dFactory);
	assert(m_pdwriteFactory);
	// 释放D2D的相关资源
	m_pColorBrush.Reset();
	m_pd2dRenderTarget.Reset();

	D3DApp::OnResize();

	// 为D2D创建DXGI表面渲染目标
	ComPtr<IDXGISurface> surface;
	HR(m_pSwapChain->GetBuffer(0, __uuidof(IDXGISurface), reinterpret_cast<void**>(surface.GetAddressOf())));
	D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(
		D2D1_RENDER_TARGET_TYPE_DEFAULT,
		D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_PREMULTIPLIED));
	HRESULT hr = m_pd2dFactory->CreateDxgiSurfaceRenderTarget(surface.Get(), &props, m_pd2dRenderTarget.GetAddressOf());
	surface.Reset();

	if (hr == E_NOINTERFACE)
	{
		OutputDebugStringW(L"\n警告：Direct2D与Direct3D互操作性功能受限，你将无法看到文本信息。现提供下述可选方法：\n"
			L"1. 对于Win7系统，需要更新至Win7 SP1，并安装KB2670838补丁以支持Direct2D显示。\n"
			L"2. 自行完成Direct3D 10.1与Direct2D的交互。详情参阅："
			L"https://docs.microsoft.com/zh-cn/windows/desktop/Direct2D/direct2d-and-direct3d-interoperation-overview""\n"
			L"3. 使用别的字体库，比如FreeType。\n\n");
	}
	else if (hr == S_OK)
	{
		// 创建固定颜色刷和文本格式
		HR(m_pd2dRenderTarget->CreateSolidColorBrush(
			D2D1::ColorF(D2D1::ColorF::White),
			m_pColorBrush.GetAddressOf()));
		HR(m_pdwriteFactory->CreateTextFormat(L"宋体", nullptr, DWRITE_FONT_WEIGHT_NORMAL,
			DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 15, L"zh-cn",
			m_pTextFormat.GetAddressOf()));
	}
	else
	{
		// 报告异常问题
		assert(m_pd2dRenderTarget);
	}

	// 摄像机变更显示
	if (m_pCamera != nullptr)
	{
		m_pCamera->SetFrustum(XM_PI / 3, AspectRatio(), 1.0f, 1000.0f);
		m_pCamera->SetViewPort(0.0f, 0.0f, (float)m_ClientWidth, (float)m_ClientHeight);
		m_BasicEffect.SetProjMatrix(m_pCamera->GetProjXM());
	}
}

void GameApp::UpdateScene(float dt)
{
	if (m_KeyboardTracker.IsKeyPressed(Keyboard::Q))
	{
		mouse = 1;
	}
	if (m_KeyboardTracker.IsKeyPressed(Keyboard::E))
	{
		mouse = 0;
	}
	if (mouse == 0) {
		m_pMouse->SetMode(DirectX::Mouse::MODE_ABSOLUTE);
		m_pMouse->SetVisible(true);
	}
	else {
		m_pMouse->SetMode(DirectX::Mouse::MODE_RELATIVE);
		m_pMouse->SetVisible(false);
	}
	// 更新鼠标事件，获取相对偏移量
	Mouse::State mouseState = m_pMouse->GetState();
	Mouse::State lastMouseState = m_MouseTracker.GetLastState();
	m_MouseTracker.Update(mouseState);
	Keyboard::State keyState = m_pKeyboard->GetState();
	m_KeyboardTracker.Update(keyState);

	auto cam1st = std::dynamic_pointer_cast<FirstPersonCamera>(m_pCamera);
	auto cam3rd = std::dynamic_pointer_cast<ThirdPersonCamera>(m_pCamera);
	
	Transform& woodCrateTransform = m_Cylinder.GetTransform();
	// ******************

	// 在鼠标没进入窗口前仍为ABSOLUTE模式
	if (mouseState.positionMode == Mouse::MODE_RELATIVE)
	{
		cam1st->Pitch(mouseState.y * dt * 1.25f);
		cam1st->RotateY(mouseState.x * dt * 1.25f);
	}

	// 更新观察矩阵
	m_BasicEffect.SetViewMatrix(m_pCamera->GetViewXM());
	m_BasicEffect.SetEyePos(m_pCamera->GetPosition());

	// 重置滚轮值
	m_pMouse->ResetScrollWheelValue();

	// 选择天空盒
	if (m_KeyboardTracker.IsKeyPressed(Keyboard::D1))
	{
		m_SkyBoxMode = SkyBoxMode::Daylight;
		m_BasicEffect.SetTextureCube(m_pDaylight->GetTextureCube());
	}
	if (m_KeyboardTracker.IsKeyPressed(Keyboard::D2))
	{
		m_SkyBoxMode = SkyBoxMode::Sunset;
		m_BasicEffect.SetTextureCube(m_pSunset->GetTextureCube());
	}
	if (m_KeyboardTracker.IsKeyPressed(Keyboard::D3))
	{
		m_SkyBoxMode = SkyBoxMode::Desert;
		m_BasicEffect.SetTextureCube(m_pDesert->GetTextureCube());
	}

	if (m_CameraMode == CameraMode::FirstPerson || m_CameraMode == CameraMode::Free)
	{
		// 第一人称/自由摄像机的操作

		// 方向移动
		if (keyState.IsKeyDown(Keyboard::W))
		{
			if (m_CameraMode == CameraMode::FirstPerson)
				cam1st->Walk(dt * 6.0f);
			else
				cam1st->MoveForward(dt * 6.0f);
		}
		if (keyState.IsKeyDown(Keyboard::S))
		{
			if (m_CameraMode == CameraMode::FirstPerson)
				cam1st->Walk(dt * -6.0f);
			else
				cam1st->MoveForward(dt * -6.0f);
		}
		if (keyState.IsKeyDown(Keyboard::A))
			cam1st->Strafe(dt * -6.0f);
		if (keyState.IsKeyDown(Keyboard::D))
			cam1st->Strafe(dt * 6.0f);

		// 将摄像机位置限制在[-8.9, 8.9]x[-8.9, 8.9]x[0.0, 8.9]的区域内
		// 不允许穿地
		XMFLOAT3 adjustedPos(0.0f,2.5f,-2.0f);
		XMStoreFloat3(&adjustedPos, 
		XMVectorClamp(cam1st->GetPositionXM(), XMVectorSet(-5.0f, -5.0f, -5.0f, 0.0f), XMVectorReplicate(5.0f)));
		cam1st->SetPosition(adjustedPos);

		// 仅在第一人称模式移动摄像机的同时移动圆柱,并限定圆柱移动范围
		if (m_CameraMode == CameraMode::FirstPerson)
			woodCrateTransform.SetPosition(adjustedPos);
		if (m_CameraMode == CameraMode::FirstPerson) {
			m_PickedObjStr = L"无";
			Ray ray = Ray::ScreenToRay(*m_pCamera, (float)mouseState.x, (float)mouseState.y);
			bool hitObject = false;
			if (ray.Hit(m_BoundingSphere))
			{
				hitObject = true;
			}
			if (hitObject == true && keyState.IsKeyDown(Keyboard::R))
			{
				std::wstring wstr = L"球体被破坏(无法恢复)";
				MessageBox(nullptr, wstr.c_str(), L"注意", 0);
				m_mode = mode::destroyed;
			}
			if (m_MouseTracker.rightButton == Mouse::ButtonStateTracker::PRESSED)
			{
				if (m_mode == mode::falling)m_mode = mode::existing;
				else {
					std::wstring wstr = L"当前无物体可放置";
					MessageBox(nullptr, wstr.c_str(), L"注意", 0);
				}
			}
			if (hitObject == true && m_MouseTracker.leftButton == Mouse::ButtonStateTracker::PRESSED)
				m_mode = mode::falling;
		}
	}

	if (m_KeyboardTracker.IsKeyPressed(Keyboard::D7) && m_CameraMode != CameraMode::FirstPerson)
	{
		//mouseState.positionMode = Mouse::MODE_ABSOLUTE;
		//m_pMouse->SetVisible(true);
		if (!cam1st)
		{
			cam1st.reset(new FirstPersonCamera);
			cam1st->SetFrustum(XM_PI / 3, AspectRatio(), 0.5f, 1000.0f);
			m_pCamera = cam1st;
		}
		cam1st->LookTo(woodCrateTransform.GetPosition(),
			XMFLOAT3(0.0f, 0.0f, 1.0f),
			XMFLOAT3(0.0f, 1.0f, 0.0f));
		m_CameraMode = CameraMode::FirstPerson;
	}

	else if (m_KeyboardTracker.IsKeyPressed(Keyboard::D8) && m_CameraMode != CameraMode::Free)
	{
		//mouseState.positionMode = Mouse::MODE_RELATIVE;
		//m_pMouse->SetVisible(false);
		if (!cam1st)
		{
			cam1st.reset(new FirstPersonCamera);
			cam1st->SetFrustum(XM_PI / 3, AspectRatio(), 0.5f, 1000.0f);
			m_pCamera = cam1st;
		}
		// 从箱子上方开始
		XMFLOAT3 pos = woodCrateTransform.GetPosition();
		XMFLOAT3 to = XMFLOAT3(0.0f, 0.0f, 1.0f);
		XMFLOAT3 up = XMFLOAT3(0.0f, 1.0f, 0.0f);
		pos.y += 3;
		cam1st->LookTo(pos, to, up);

		m_CameraMode = CameraMode::Free;
	}
	// 退出程序，这里应向窗口发送销毁信息
	if (m_KeyboardTracker.IsKeyPressed(Keyboard::Escape))
		SendMessage(MainWnd(), WM_DESTROY, 0, 0);
}

void GameApp::DrawScene()
{
	assert(m_pd3dImmediateContext);
	assert(m_pSwapChain);

	// ******************
	// 绘制Direct3D部分
	//
	m_pd3dImmediateContext->ClearRenderTargetView(m_pRenderTargetView.Get(), reinterpret_cast<const float*>(&Colors::Black));
	m_pd3dImmediateContext->ClearDepthStencilView(m_pDepthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

	// 绘制模型
	m_BasicEffect.SetRenderDefault(m_pd3dImmediateContext.Get(), BasicEffect::RenderObject);
	m_BasicEffect.SetReflectionEnabled(true);
	m_BasicEffect.SetTextureUsed(true);
	if (m_mode != mode::existing) {
		m_BoundingSphere.Radius = 0.0f;
	}
	else {
		m_Sphere.Draw(m_pd3dImmediateContext.Get(), m_BasicEffect);
		m_BoundingSphere.Radius = 1.0f;
	}
	m_BasicEffect.SetReflectionEnabled(false);
	m_Ground.Draw(m_pd3dImmediateContext.Get(), m_BasicEffect);
	m_Cylinder.Draw(m_pd3dImmediateContext.Get(), m_BasicEffect);


	// 绘制天空盒
	m_SkyEffect.SetRenderDefault(m_pd3dImmediateContext.Get());
	switch (m_SkyBoxMode)
	{
	case SkyBoxMode::Daylight: m_pDaylight->Draw(m_pd3dImmediateContext.Get(), m_SkyEffect, *m_pCamera); break;
	case SkyBoxMode::Sunset: m_pSunset->Draw(m_pd3dImmediateContext.Get(), m_SkyEffect, *m_pCamera); break;
	case SkyBoxMode::Desert: m_pDesert->Draw(m_pd3dImmediateContext.Get(), m_SkyEffect, *m_pCamera); break;
	}
	


	// ******************
	// 绘制Direct2D部分
	//
	if (m_pd2dRenderTarget != nullptr)
	{
		m_pd2dRenderTarget->BeginDraw();
		std::wstring text1 = L"Esc退出\n"
			L"第一人称下W/S/A/D移动\n"
			L"Q/E键切换鼠标相对/绝对模式\n"
			L"第一人称鼠标绝对模式下:\n"
			L"鼠标左键/右键可拾取/放置球体,,R键破坏球体\n"
			L"切换天空盒: 1-白天 2-日落 3-沙漠\n"
			L"当前天空盒: ";
		std::wstring text2 =L"切换摄像机模式: 7-第一人称 8-自由视角 \n"
			L"当前模式: ";

		switch (m_SkyBoxMode)
		{
		case SkyBoxMode::Daylight: text1 += L"白天"; break;
		case SkyBoxMode::Sunset: text1 += L"日落"; break;
		case SkyBoxMode::Desert: text1 += L"沙漠"; break;
		}
		switch (m_CameraMode) {
		case CameraMode::Free:				text2 += L"自由视角"; break;
		case CameraMode::FirstPerson:		text2 += L"第一人称"; break;
		}
		m_pd2dRenderTarget->DrawTextW(text1.c_str(), (UINT32)text1.length(), m_pTextFormat.Get(),
			D2D1_RECT_F{ 0.0f, 0.0f, 600.0f, 200.0f }, m_pColorBrush.Get());

		m_pd2dRenderTarget->DrawTextW(text2.c_str(), (UINT32)text2.length(), m_pTextFormat.Get(),
			D2D1_RECT_F{ 520.0f, 0.0f,900.0f, 200.0f }, m_pColorBrush.Get());

		HR(m_pd2dRenderTarget->EndDraw());
	}
	HR(m_pSwapChain->Present(0, 0));
}



bool GameApp::InitResource()
{
	// ******************
	// 初始化天空盒相关

	m_pDaylight = std::make_unique<SkyRender>();
	HR(m_pDaylight->InitResource(m_pd3dDevice.Get(), m_pd3dImmediateContext.Get(),
		L"Texture\\daylight.jpg", 
		5000.0f));

	m_pSunset = std::make_unique<SkyRender>();
	HR(m_pSunset->InitResource(m_pd3dDevice.Get(), m_pd3dImmediateContext.Get(),
		std::vector<std::wstring>{
		L"Texture\\Resource\\test.bmp", L"Texture\\Resource\\test.bmp",
			L"Texture\\Resource\\test.bmp", L"Texture\\Resource\\test.bmp",
			L"Texture\\Resource\\test.bmp", L"Texture\\Resource\\test.bmp", 
	},
		5000.0f));

	m_pDesert = std::make_unique<SkyRender>();
	HR(m_pDesert->InitResource(m_pd3dDevice.Get(), m_pd3dImmediateContext.Get(),
		L"Texture\\desertcube1024.dds",
		5000.0f));

	m_BasicEffect.SetTextureCube(m_pDaylight->GetTextureCube());
	// ******************
	// 初始化游戏对象
	//
	
	Model model;
	// 球体
	model.SetMesh(m_pd3dDevice.Get(), Geometry::CreateSphere(1.0f, 30, 30));
	model.modelParts[0].material.ambient = XMFLOAT4(0.2f, 0.2f, 0.2f, 1.0f);
	model.modelParts[0].material.diffuse = XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f);
	model.modelParts[0].material.specular = XMFLOAT4(0.8f, 0.8f, 0.8f, 16.0f);
	model.modelParts[0].material.reflect = XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f);
	HR(CreateDDSTextureFromFile(m_pd3dDevice.Get(), 
		L"Texture\\stone.dds", 
		nullptr, 
		model.modelParts[0].texDiffuse.GetAddressOf()));
	m_Sphere.SetModel(std::move(model));
	model.SetMesh(m_pd3dDevice.Get(), Geometry::CreatePlane(XMFLOAT2(10.0f, 10.0f), XMFLOAT2(5.0f,  5.0f)));
	model.modelParts[0].material.ambient = XMFLOAT4(0.2f, 0.2f, 0.2f, 1.0f);
	model.modelParts[0].material.diffuse = XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f);
	model.modelParts[0].material.specular = XMFLOAT4(0.2f, 0.2f, 0.2f, 16.0f); 
	model.modelParts[0].material.reflect = XMFLOAT4();
	//创建球体的包围盒
	m_BoundingSphere.Center = m_Sphere.GetTransform().GetPosition();
	m_BoundingSphere.Radius = 1.0f;

	HR(CreateDDSTextureFromFile(m_pd3dDevice.Get(),
		L"Texture\\floor.dds",
		nullptr,
		model.modelParts[0].texDiffuse.GetAddressOf()));
	m_Ground.SetModel(std::move(model));
	m_Ground.GetTransform().SetPosition(0.0f, -3.0f, 0.0f);
	
	// 柱体
	model.SetMesh(m_pd3dDevice.Get(),
		Geometry::CreateCylinder(0.5f, 2.0f));
	model.modelParts[0].material.ambient = XMFLOAT4(0.2f, 0.2f, 0.2f, 1.0f);
	model.modelParts[0].material.diffuse = XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f);
	model.modelParts[0].material.specular = XMFLOAT4(0.2f, 0.2f, 0.2f, 16.0f);
	model.modelParts[0].material.reflect = XMFLOAT4();
	HR(CreateDDSTextureFromFile(m_pd3dDevice.Get(),
		L"Texture\\bricks.dds",
		nullptr,
		model.modelParts[0].texDiffuse.GetAddressOf()));
	m_Cylinder.SetModel(std::move(model));
	m_Cylinder.GetTransform().SetPosition(0.0f, -1.99f, 0.0f);

	// ******************
	// 初始化摄像机
	//
	auto camera = std::shared_ptr<FirstPersonCamera>(new FirstPersonCamera);
	m_pCamera = camera;
	camera->SetViewPort(0.0f, 0.0f, (float)m_ClientWidth, (float)m_ClientHeight);
	camera->SetFrustum(XM_PI / 3, AspectRatio(), 1.0f, 1000.0f);
	camera->LookTo(XMFLOAT3(0.0f, 0.0f, -10.0f), XMFLOAT3(0.0f, 0.0f, 1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f));

	m_BasicEffect.SetViewMatrix(camera->GetViewXM());
	m_BasicEffect.SetProjMatrix(camera->GetProjXM());


	// ******************
	// 初始化不会变化的值
	//

	// 方向光
	DirectionalLight dirLight[4];
	dirLight[0].ambient = XMFLOAT4(0.15f, 0.15f, 0.15f, 1.0f);
	dirLight[0].diffuse = XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f);
	dirLight[0].specular = XMFLOAT4(0.1f, 0.1f, 0.1f, 1.0f);
	dirLight[0].direction = XMFLOAT3(-0.577f, -0.577f, 0.577f);
	dirLight[1] = dirLight[0];
	dirLight[1].direction = XMFLOAT3(0.577f, -0.577f, 0.577f);
	dirLight[2] = dirLight[0];
	dirLight[2].direction = XMFLOAT3(0.577f, -0.577f, -0.577f);
	dirLight[3] = dirLight[0];
	dirLight[3].direction = XMFLOAT3(-0.577f, -0.577f, -0.577f);
	for (int i = 0; i < 4; ++i)
		m_BasicEffect.SetDirLight(i, dirLight[i]);


	// ******************
	// 设置调试对象名
	//
	m_Cylinder.SetDebugObjectName("Cylinder");
	m_Ground.SetDebugObjectName("Ground");
	m_Sphere.SetDebugObjectName("Sphere");
	m_pDaylight->SetDebugObjectName("DayLight");
	m_pSunset->SetDebugObjectName("Sunset");
	m_pDesert->SetDebugObjectName("Desert");
	return true;
}

