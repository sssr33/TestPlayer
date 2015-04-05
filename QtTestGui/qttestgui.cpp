#include "qttestgui.h"
#include "Helpers/ImageUtils.h"
#include "Helpers/bit_view.h"

QtTestGui::QtTestGui(QWidget *parent)
	: QMainWindow(parent), trayIcon(this), stopD3d(false){
	ui.setupUi(this);

	this->trayIcon.setToolTip("Click me!");

	Qt::WindowFlags flags = this->windowFlags();
	this->setWindowFlags(flags | Qt::WindowStaysOnTopHint);
	this->setAttribute(Qt::WidgetAttribute::WA_PaintOnScreen);

	connect(&this->trayIcon, &QSystemTrayIcon::activated, this, &QtTestGui::TrayActivated);


	HRESULT hr = S_OK;
	D3D_FEATURE_LEVEL levels[] = {
		D3D_FEATURE_LEVEL::D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL::D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL::D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL::D3D_FEATURE_LEVEL_10_0,
		D3D_FEATURE_LEVEL::D3D_FEATURE_LEVEL_9_3,
		D3D_FEATURE_LEVEL::D3D_FEATURE_LEVEL_9_2,
		D3D_FEATURE_LEVEL::D3D_FEATURE_LEVEL_9_1
	};

	Microsoft::WRL::ComPtr<ID3D11Device> d3dDevTmp;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> d3dCtxTmp;

	uint32_t dxDevFlags = D3D11_CREATE_DEVICE_FLAG::D3D11_CREATE_DEVICE_BGRA_SUPPORT;

#ifdef _DEBUG
	dxDevFlags |= D3D11_CREATE_DEVICE_FLAG::D3D11_CREATE_DEVICE_DEBUG;
#endif

	std::lock_guard<std::mutex> lk(this->d3dMtx);

	hr = D3D11CreateDevice(
		nullptr,
		D3D_DRIVER_TYPE::D3D_DRIVER_TYPE_HARDWARE,
		nullptr, dxDevFlags,
		levels, sizeof(levels) / sizeof(levels[0]),
		D3D11_SDK_VERSION,
		d3dDevTmp.GetAddressOf(), &this->d3dLevel, d3dCtxTmp.GetAddressOf());

	hr = d3dDevTmp.As(&this->d3dDev);
	hr = d3dCtxTmp.As(&this->d3dCtx);

	this->TestTextureTypes();
	this->CreateGeometry();
	this->CreateShaders();
	this->CreateSampler();

	this->UpdateProjection();

	this->d3dThread = std::thread([=](){
		this->RenderD3d();
	});
}

QtTestGui::~QtTestGui(){
	this->stopD3d = true;

	if (this->d3dThread.joinable()){
		this->d3dThread.join();
	}
}

//void QtTestGui::changeEvent(QEvent *e){
//	switch (e->type())
//	{
//	case QEvent::LanguageChange:
//		//this->ui->retranslateUi(this);
//		break;
//	case QEvent::WindowStateChange:
//	{
//		if (this->windowState() & Qt::WindowMinimized)
//		{
//			//this->hide();
//			//this->trayIcon.show();
//		}
//
//		break;
//	}
//	default:
//		break;
//	}
//
//	QMainWindow::changeEvent(e);
//}

void QtTestGui::hideEvent(QHideEvent *e){
	this->hide();
	this->trayIcon.show();

	QMainWindow::hideEvent(e);
}

void QtTestGui::resizeEvent(QResizeEvent *e){
	HRESULT hr = S_OK;

	uint32_t width = static_cast<uint32_t>((std::max)(this->width(), 1));
	uint32_t height = static_cast<uint32_t>((std::max)(this->height(), 1));
	std::lock_guard<std::mutex> lk(this->d3dMtx);

	this->d3dRtView = nullptr;

	if (!this->d3dSwapChain){
		Microsoft::WRL::ComPtr<IDXGIDevice2> dxgiDevice;
		Microsoft::WRL::ComPtr<IDXGIAdapter> dxgiAdapter;
		Microsoft::WRL::ComPtr<IDXGIFactory2> dxgiFactory;
		Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChainTmp;

		hr = this->d3dDev.As(&dxgiDevice);
		hr = dxgiDevice->GetParent(IID_PPV_ARGS(dxgiAdapter.GetAddressOf()));
		hr = dxgiAdapter->GetParent(IID_PPV_ARGS(dxgiFactory.GetAddressOf()));

		HWND hwnd = reinterpret_cast<HWND>(this->winId());
		HWND hwnd2 = reinterpret_cast<HWND>(this->effectiveWinId());
		DXGI_SWAP_CHAIN_DESC1 swapChainDesc;
		DXGI_SWAP_CHAIN_FULLSCREEN_DESC fullscreenDesc;

		swapChainDesc.Width = width;
		swapChainDesc.Height = height;
		swapChainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		swapChainDesc.Stereo = FALSE;
		swapChainDesc.SampleDesc.Count = 1;
		swapChainDesc.SampleDesc.Quality = 0;
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.BufferCount = 2;
		swapChainDesc.Scaling = DXGI_SCALING::DXGI_SCALING_STRETCH;
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT::DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;// DXGI_SWAP_EFFECT::DXGI_SWAP_EFFECT_SEQUENTIAL;
		swapChainDesc.AlphaMode = DXGI_ALPHA_MODE::DXGI_ALPHA_MODE_IGNORE;
		swapChainDesc.Flags = 0;

		fullscreenDesc.RefreshRate.Numerator = 0;
		fullscreenDesc.RefreshRate.Denominator = 0;
		fullscreenDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER::DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
		fullscreenDesc.Scaling = DXGI_MODE_SCALING::DXGI_MODE_SCALING_CENTERED;
		fullscreenDesc.Windowed = TRUE;

		hr = dxgiFactory->CreateSwapChainForHwnd(this->d3dDev.Get(), hwnd, &swapChainDesc, &fullscreenDesc, nullptr, swapChainTmp.GetAddressOf());
		hr = swapChainTmp.As(&this->d3dSwapChain);
	}
	else{
		hr = this->d3dSwapChain->ResizeBuffers(2, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, 0);
	}

	Microsoft::WRL::ComPtr<IDXGISurface> dxgiSurface;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> d3dTex;

	hr = this->d3dSwapChain->GetBuffer(0, IID_PPV_ARGS(dxgiSurface.GetAddressOf()));
	hr = dxgiSurface.As(&d3dTex);

	D3D11_TEXTURE2D_DESC tex2dDec;
	D3D11_RENDER_TARGET_VIEW_DESC d3dRtDesc;

	d3dTex->GetDesc(&tex2dDec);

	d3dRtDesc.ViewDimension = D3D11_RTV_DIMENSION::D3D11_RTV_DIMENSION_TEXTURE2D;
	d3dRtDesc.Format = tex2dDec.Format;
	d3dRtDesc.Texture2D.MipSlice = 0;

	this->d3dDev->CreateRenderTargetView(d3dTex.Get(), &d3dRtDesc, this->d3dRtView.ReleaseAndGetAddressOf());

	D3D11_VIEWPORT viewPort;

	viewPort.TopLeftX = viewPort.TopLeftY = 0.0f;
	viewPort.Width = static_cast<float>(width);
	viewPort.Height = static_cast<float>(height);
	viewPort.MinDepth = 0.0f;
	viewPort.MaxDepth = 1.0f;

	this->d3dCtx->RSSetViewports(1, &viewPort);

	this->UpdateProjection();
}

void QtTestGui::TrayActivated(QSystemTrayIcon::ActivationReason reason){
	if (reason == QSystemTrayIcon::ActivationReason::DoubleClick){
		if (this->isHidden()){
			Qt::WindowFlags flags = this->windowFlags();
			this->setWindowFlags(flags | Qt::WindowStaysOnTopHint);
			this->showNormal();
			this->raise();
			this->trayIcon.hide();
		}
	}
}

void QtTestGui::RenderD3d(){
	while (!this->stopD3d){
		std::lock_guard<std::mutex> lk(this->d3dMtx);

		if (this->d3dRtView){
			HRESULT hr = S_OK;
			static float c = 0.0f;
			float rgba[4] = { 0, c, 0, 1 };

			/*c += 0.01f;

			if (c >= 1){
				c = 0.0f;
			}*/

			DirectX::XMMATRIX transform = DirectX::XMMatrixIdentity();

			/*static float scale = 2.0f;*/

			static float scale = 0.125f;

			/*scale -= 0.005f;

			if (scale < 0.2f){
				scale = 2.0f;
			}*/

			/*transform = DirectX::XMMatrixMultiply(transform, DirectX::XMMatrixScaling(scale, scale, 1.0f));
			transform = DirectX::XMMatrixMultiply(transform, DirectX::XMMatrixTranslation(0.0f, 0.0f, 1.0f));*/

			transform = DirectX::XMMatrixMultiply(transform, DirectX::XMMatrixScaling(1024, 1024, 1.0f));
			transform = DirectX::XMMatrixMultiply(transform, DirectX::XMMatrixTranslation(0.0f, 0.0f, 1.0f));

			DirectX::XMMATRIX proj = DirectX::XMLoadFloat4x4(&this->projection);

			transform = DirectX::XMMatrixMultiplyTranspose(transform, proj);

			this->d3dCtx->UpdateSubresource(this->cbuffer.Get(), 0, nullptr, &transform, sizeof DirectX::XMMATRIX, 0);

			uint32_t stride = sizeof DirectX::XMFLOAT2;
			uint32_t offset = 0;

			this->d3dCtx->ClearRenderTargetView(this->d3dRtView.Get(), rgba);
			this->d3dCtx->OMSetRenderTargets(1, this->d3dRtView.GetAddressOf(), nullptr);

			this->d3dCtx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY::D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
			this->d3dCtx->IASetInputLayout(this->layout.Get());
			this->d3dCtx->IASetVertexBuffers(0, 1, this->positions.GetAddressOf(), &stride, &offset);

			this->d3dCtx->VSSetConstantBuffers(0, 1, this->cbuffer.GetAddressOf());
			this->d3dCtx->VSSetShader(this->vshader.Get(), nullptr, 0);

			this->d3dCtx->PSSetShaderResources(0, 1, this->mipMapSrv0.GetAddressOf());
			this->d3dCtx->PSSetShaderResources(1, 1, this->mipMapSrv1.GetAddressOf());
			this->d3dCtx->PSSetShaderResources(2, 1, this->mipMapSrv2.GetAddressOf());
			this->d3dCtx->PSSetShaderResources(3, 1, this->mipMapSrv3.GetAddressOf());
			this->d3dCtx->PSSetSamplers(0, 1, this->sampler.GetAddressOf());
			this->d3dCtx->PSSetShader(this->pshader.Get(), nullptr, 0);

			this->d3dCtx->Draw(4, 0);

			hr = this->d3dSwapChain->Present(1, 0);
		}
	}
}

std::vector<uint8_t> QtTestGui::LoadTexture(const std::wstring &path){
	ImageUtils imgUtils;
	auto decoder = imgUtils.CreateDecoder(path.c_str());
	auto frame = imgUtils.CreateFrameForDecode(decoder.Get());
	auto frameSize = imgUtils.GetFrameSize(frame.Get());

	std::vector<uint8_t> pixels(imgUtils.GetFrameByteSize(frame.Get()));

	imgUtils.DecodePixels(frame.Get(), pixels.size(), pixels.data());

	/*bit_view<big_endian_block_order> pixelBits;
	pixelBits.set_data(pixels.data(), frameSize.x * frameSize.y);

	bool p0 = pixelBits[138 + 19 * frameSize.x];
	bool p1 = pixelBits[137 + 19 * frameSize.x];
	bool p2 = pixelBits[137 + 20 * frameSize.x];*/

	return pixels;
}

void QtTestGui::TestTextureTypes(){
	HRESULT hr = S_OK;

	ImageUtils imgUtils;
	auto decoder = imgUtils.CreateDecoder(L"mipSrc2.png");
	auto frame = imgUtils.CreateFrameForDecode(decoder.Get());
	auto frameSize = imgUtils.GetFrameSize(frame.Get());

	/*std::vector<uint8_t> pixels(imgUtils.GetFrameByteSize(frame.Get()));

	imgUtils.DecodePixels(frame.Get(), pixels.size(), pixels.data());*/

	Microsoft::WRL::ComPtr<ID3D11Texture2D> tex;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
	D3D11_TEXTURE2D_DESC texDesc;

	texDesc.Width = frameSize.x / 8;// frameSize.x;
	texDesc.Height = frameSize.y;
	texDesc.MipLevels = 1;// 8;
	texDesc.ArraySize = 1;
	texDesc.Format = DXGI_FORMAT::DXGI_FORMAT_R8_UNORM;// DXGI_FORMAT::DXGI_FORMAT_R8G8B8A8_UNORM;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Usage = D3D11_USAGE::D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_FLAG::D3D11_BIND_SHADER_RESOURCE;// | D3D11_BIND_FLAG::D3D11_BIND_RENDER_TARGET;
	texDesc.CPUAccessFlags = 0;
	texDesc.MiscFlags = 0;// D3D11_RESOURCE_MISC_GENERATE_MIPS;

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;

	srvDesc.Format = texDesc.Format;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION::D3D_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = texDesc.MipLevels;

	D3D11_BOX box;
	box.left = box.top = 0;
	box.right = frameSize.x;
	box.bottom = frameSize.y;
	box.front = 0;
	box.back = 1;

	/*this->d3dCtx->UpdateSubresource(tex.Get(), 0, &box, pixels.data(), frameSize.x * 4, frameSize.x * frameSize.y * 4);*/
	/*if (texDesc.MipLevels > 1){
		this->d3dCtx->GenerateMips(srv.Get());
		}*/

	auto pixels0 = this->LoadTexture(L"Gray_mipSrc0.png");
	auto pixels1 = this->LoadTexture(L"Gray_mipSrc1.png");
	auto pixels2 = this->LoadTexture(L"Gray_mipSrc2.png");
	auto pixels3 = this->LoadTexture(L"Gray_mipSrc3.png");

	hr = this->d3dDev->CreateTexture2D(&texDesc, nullptr, tex.ReleaseAndGetAddressOf());
	hr = this->d3dDev->CreateShaderResourceView(tex.Get(), &srvDesc, srv.ReleaseAndGetAddressOf());
	this->d3dCtx->UpdateSubresource(tex.Get(), 0, &box, pixels0.data(), frameSize.x  / 8, (frameSize.x * frameSize.y) / 8);
	this->mipMapSrv0 = srv;

	hr = this->d3dDev->CreateTexture2D(&texDesc, nullptr, tex.ReleaseAndGetAddressOf());
	hr = this->d3dDev->CreateShaderResourceView(tex.Get(), &srvDesc, srv.ReleaseAndGetAddressOf());
	this->d3dCtx->UpdateSubresource(tex.Get(), 0, &box, pixels1.data(), frameSize.x / 8, (frameSize.x * frameSize.y) / 8);
	this->mipMapSrv1 = srv;

	hr = this->d3dDev->CreateTexture2D(&texDesc, nullptr, tex.ReleaseAndGetAddressOf());
	hr = this->d3dDev->CreateShaderResourceView(tex.Get(), &srvDesc, srv.ReleaseAndGetAddressOf());
	this->d3dCtx->UpdateSubresource(tex.Get(), 0, &box, pixels2.data(), frameSize.x / 8, (frameSize.x * frameSize.y) / 8);
	this->mipMapSrv2 = srv;

	hr = this->d3dDev->CreateTexture2D(&texDesc, nullptr, tex.ReleaseAndGetAddressOf());
	hr = this->d3dDev->CreateShaderResourceView(tex.Get(), &srvDesc, srv.ReleaseAndGetAddressOf());
	this->d3dCtx->UpdateSubresource(tex.Get(), 0, &box, pixels3.data(), frameSize.x / 8, (frameSize.x * frameSize.y) / 8);
	this->mipMapSrv3 = srv;
}

void QtTestGui::CreateGeometry(){
	DirectX::XMFLOAT2 verts[] = {
		DirectX::XMFLOAT2(-0.5f, 0.5f),
		DirectX::XMFLOAT2(0.5f, 0.5f),
		DirectX::XMFLOAT2(-0.5f, -0.5f),
		DirectX::XMFLOAT2(0.5f, -0.5f)
	};

	D3D11_BUFFER_DESC bufDesc;

	bufDesc.ByteWidth = sizeof verts;
	bufDesc.Usage = D3D11_USAGE::D3D11_USAGE_IMMUTABLE;
	bufDesc.BindFlags = D3D11_BIND_FLAG::D3D11_BIND_VERTEX_BUFFER;
	bufDesc.CPUAccessFlags = 0;
	bufDesc.MiscFlags = 0;
	bufDesc.StructureByteStride = 0;

	D3D11_SUBRESOURCE_DATA bufData = {};
	bufData.pSysMem = verts;

	this->d3dDev->CreateBuffer(&bufDesc, &bufData, this->positions.GetAddressOf());

	bufDesc.ByteWidth = sizeof DirectX::XMFLOAT4X4;
	bufDesc.Usage = D3D11_USAGE::D3D11_USAGE_DEFAULT;
	bufDesc.BindFlags = D3D11_BIND_FLAG::D3D11_BIND_CONSTANT_BUFFER;

	this->d3dDev->CreateBuffer(&bufDesc, nullptr, this->cbuffer.GetAddressOf());
}

void QtTestGui::CreateShaders(){
	auto vshaderData = H::System::LoadPackageFile(L"VertexShader.cso");
	/*auto pshaderData = H::System::LoadPackageFile(L"PixelShader.cso");*/
	auto pshaderData = H::System::LoadPackageFile(L"BitSamplingPixelShader.cso");

	this->d3dDev->CreateVertexShader(vshaderData.data(), vshaderData.size(), nullptr, this->vshader.GetAddressOf());
	this->d3dDev->CreatePixelShader(pshaderData.data(), pshaderData.size(), nullptr, this->pshader.GetAddressOf());

	D3D11_INPUT_ELEMENT_DESC inputDesc[] = {
		{ "POSITION", 0, DXGI_FORMAT::DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_CLASSIFICATION::D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};

	this->d3dDev->CreateInputLayout(inputDesc, ARRAY_SIZE(inputDesc), vshaderData.data(), vshaderData.size(), this->layout.GetAddressOf());
}

void QtTestGui::CreateSampler(){
	D3D11_SAMPLER_DESC samplerDesc;

	/*samplerDesc.Filter = D3D11_FILTER::D3D11_FILTER_MIN_MAG_MIP_LINEAR;*/
	samplerDesc.Filter = D3D11_FILTER::D3D11_FILTER_MIN_MAG_MIP_POINT;
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.MipLODBias = 0.0f;
	samplerDesc.MaxAnisotropy = 16;
	samplerDesc.ComparisonFunc = D3D11_COMPARISON_FUNC::D3D11_COMPARISON_NEVER;
	std::memset(samplerDesc.BorderColor, 0, sizeof samplerDesc.BorderColor);
	samplerDesc.MinLOD = -FLT_MAX;
	samplerDesc.MaxLOD = FLT_MAX;

	this->d3dDev->CreateSamplerState(&samplerDesc, this->sampler.GetAddressOf());
}

void QtTestGui::UpdateProjection(){
	float ar = 1.0f;

	if (this->width() != 0 && this->height() != 0){
		ar = static_cast<float>(this->width()) / static_cast<float>(this->height());
	}

	//auto projTmp = DirectX::XMMatrixPerspectiveFovLH(DirectX::XMConvertToRadians(90.0f), ar, 0.1f, 10.0f);
	auto projTmp = DirectX::XMMatrixOrthographicOffCenterLH(-(this->width() / 2), (this->width() / 2), -(this->height() / 2), (this->height() / 2), 0.1f, 10.0f);
	DirectX::XMStoreFloat4x4(&this->projection, projTmp);
}