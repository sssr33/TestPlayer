#ifndef QTTESTGUI_H
#define QTTESTGUI_H

#include "ui_qttestgui.h"

#include <QtWidgets/QMainWindow>
#include <QSystemTrayIcon>
#include <wrl.h>
#include <d3d11_2.h>
#include <dxgi1_3.h>
#include <DirectXMath.h>
#include <cstdint>
#include <thread>
#include <mutex>

class QtTestGui : public QMainWindow{
	Q_OBJECT
public:
	QtTestGui(QWidget *parent = 0);
	~QtTestGui();

protected:
	//virtual void changeEvent(QEvent *e) override;
	virtual void hideEvent(QHideEvent *e) override;
	virtual void resizeEvent(QResizeEvent *e) override;

protected slots:
	void TrayActivated(QSystemTrayIcon::ActivationReason reason);

private:
	Ui::QtTestGuiClass ui;
	QSystemTrayIcon trayIcon;

	bool stopD3d;
	std::mutex d3dMtx;
	std::thread d3dThread;

	D3D_FEATURE_LEVEL d3dLevel;
	Microsoft::WRL::ComPtr<ID3D11Device2> d3dDev;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext2> d3dCtx;
	Microsoft::WRL::ComPtr<IDXGISwapChain2> d3dSwapChain;
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> d3dRtView;

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mipMapSrv;

	Microsoft::WRL::ComPtr<ID3D11Buffer> positions;
	Microsoft::WRL::ComPtr<ID3D11Buffer> cbuffer;
	Microsoft::WRL::ComPtr<ID3D11InputLayout> layout;
	Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler;
	Microsoft::WRL::ComPtr<ID3D11VertexShader> vshader;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> pshader;

	DirectX::XMFLOAT4X4 projection;

	void RenderD3d();
	void TestTextureTypes();
	void CreateGeometry();
	void CreateShaders();
	void CreateSampler();

	void UpdateProjection();
};

#endif // QTTESTGUI_H
