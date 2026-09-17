#pragma once



struct VERTEX_SKIN
{
	XMFLOAT3 Position;
	XMFLOAT3 Normal;
	XMFLOAT4 Diffuse;
	XMFLOAT2 TexCoord;
	UINT           BoneIndex[4];
	XMFLOAT4 BoneWeight;
};

struct VERTEX_3D
{
	XMFLOAT3 Position;
	XMFLOAT3 Normal;
	XMFLOAT4 Diffuse;
	XMFLOAT2 TexCoord;
};



struct MATERIAL
{
	XMFLOAT4	Ambient;
	XMFLOAT4	Diffuse;
	XMFLOAT4	Specular;
	XMFLOAT4	Emission;
	float		Shininess;
	BOOL		TextureEnable;
	float		Dummy[2];
};



struct LIGHT
{
	BOOL		Enable;
	BOOL		Dummy[3];
	XMFLOAT4	Direction;
	XMFLOAT4	Diffuse;
	XMFLOAT4	Ambient;
};



class Renderer
{
private:

	static D3D_FEATURE_LEVEL       m_FeatureLevel;

	static ID3D11Device*           m_Device;
	static ID3D11DeviceContext*    m_DeviceContext;
	static IDXGISwapChain*         m_SwapChain;
	static ID3D11RenderTargetView* m_RenderTargetView;
	static ID3D11DepthStencilView* m_DepthStencilView;

	static ID3D11Buffer*			m_WorldBuffer;
	static ID3D11Buffer*			m_ViewBuffer;
	static ID3D11Buffer*			m_ProjectionBuffer;
	static ID3D11Buffer*			m_MaterialBuffer;
	static ID3D11Buffer*			m_LightBuffer;


	static ID3D11DepthStencilState* m_DepthStateEnable;
	static ID3D11DepthStencilState* m_DepthStateDisable;

	static ID3D11BlendState*		m_BlendState;
	static ID3D11BlendState*		m_BlendStateAdd;
	static ID3D11BlendState*		m_BlendStateATC;

	// Scene ビューと Game ビュー用（いったんテクスチャに描き、ImGui のウィンドウに表示する）
	static ID3D11RenderTargetView*   m_ViewRTV[2];
	static ID3D11ShaderResourceView* m_ViewSRV[2];
	static ID3D11DepthStencilView*   m_ViewDSV[2];



public:
	static void Init();
	static void Uninit();
	static void Begin();
	enum { VIEW_SCENE = 0, VIEW_GAME = 1 };
	static void BeginScene(int view = VIEW_SCENE);	// Scene / Game 用のテクスチャに描き始める
	static void BeginBackBuffer();					// 画面（バックバッファ）に切り替える。ImGui はこちらに描く
	static void Resize(UINT width, UINT height);	// ウィンドウの大きさが変わったとき（WM_SIZE）に呼ぶ
	static ID3D11ShaderResourceView* GetViewTexture(int view) { return m_ViewSRV[view]; }
	static ID3D11ShaderResourceView* GetSceneTexture() { return m_ViewSRV[VIEW_SCENE]; }
	static void End();

	static void SetDepthEnable(bool Enable);
	static void SetAddEnable(bool Enable);
	static void SetATCEnable(bool Enable);
	static void SetWorldViewProjection2D();
	static void SetWorldMatrix(XMMATRIX WorldMatrix);
	static void SetViewMatrix(XMMATRIX ViewMatrix);
	static void SetProjectionMatrix(XMMATRIX ProjectionMatrix);
	static void SetMaterial(MATERIAL Material);
	static void SetLight(LIGHT Light);

	static ID3D11Device* GetDevice( void ){ return m_Device; }
	static ID3D11DeviceContext* GetDeviceContext( void ){ return m_DeviceContext; }



	static void CreateVertexShader(ID3D11VertexShader** VertexShader, ID3D11InputLayout** VertexLayout, const char* FileName);
	static void CreatePixelShader(ID3D11PixelShader** PixelShader, const char* FileName);


};
