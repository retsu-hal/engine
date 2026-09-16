#include "main.h"
#include "Manager.h"
#include "Renderer.h"
#include "Camera.h"
#include "GameObject.h"
#include "GameScene.h"
#include "TitleScene.h"
#include "Audio.h"
#include "ModelRenderer.h"
#include "ShaderManager.h"
#include "TextureManager.h"
#include "Collider.h"
#include "Gizmo.h"


//staticメンバー変数はcppで定義する必要がある
std::list<GameObject* > Manager::m_GameObjects;
Scene* Manager::m_Scene=nullptr;
Scene* Manager::m_NextScene=nullptr;
float Manager::m_ChangeSceneTime = 5.0f;

float Manager::m_DeltaTime = 1.0f / 60.0f;

void Manager::Init()
{
	Input::Init();
	Renderer::Init();
	Audio::InitMaster();

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();
	ImGui_ImplWin32_Init(GetWindow());
	ImGui_ImplDX11_Init(Renderer::GetDevice(), Renderer::GetDeviceContext());

	ChangeScene<TitleScene>();
}


void Manager::Uninit()
{
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	
	for (GameObject* obj : m_GameObjects) obj->Uninit();
	for (GameObject* obj : m_GameObjects) delete obj;
	m_GameObjects.clear();
	
	if(m_Scene!=nullptr)
	{
		m_Scene->Uninit();
		delete m_Scene;
	}

	ModelRenderer::UnloadAll();	
	ShaderManager::Unload();
	TextureManager::Unload();

	Gizmo::Uninit();
	Renderer::Uninit();
	Input::Uninit();
	Audio::UninitMaster();
}

void Manager::Update()
{
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	float  dt =GetDeltaTime();
	Input::Update();

#if _DEBUG
	//F1でコライダー表示のON/OFF
	if (Input::GetKeyTrigger(VK_F1)) Gizmo::SetEnable(!Gizmo::IsEnable());
#endif


	if (m_Scene != nullptr)	m_Scene->Update();

	for (GameObject* gameObject : m_GameObjects) gameObject->Update();

	//当たり判定（各オブジェクトのUpdateで移動し終わってから、まとめて押し出す）
	Collider::Check();

	//ゲームオブジェクトの削除　【ラムダ式】
	m_GameObjects.remove_if([](GameObject* object)
		{
			return object->Destroy();
		});

	//シーンの切り替え
	if (m_NextScene != nullptr)
	{
		m_ChangeSceneTime -= dt;

		if(m_ChangeSceneTime <= 0.0f)
		{
			//現在のシーンを破棄
			if (m_Scene != nullptr)
			{
				m_Scene->Uninit();
				delete m_Scene;
			}

			//ゲームオブジェクトの削除
			for (GameObject* gameObject : m_GameObjects)
			{
				gameObject->Uninit();
				delete gameObject;
			}
			m_GameObjects.clear();

			Profiler::Clear();
			m_Scene = m_NextScene;
			{
				ScopedSceneTimer sceneTimer;	// シーン全体の実ロード時間(実経過)
				m_Scene->Init();
			}
			Profiler::Dump();
			m_NextScene = nullptr;
		}
	}
};

void Manager::Draw()
{
	Renderer::Begin();


	

	//Z値計算
	CAMERA* camera = GetGameObject<CAMERA>();

	if (camera)
	{
		Vector3 forward = camera->GetForward();
		Vector3 position = camera->GetPosition();

		//Z値計算
		for (GameObject* gameObject : m_GameObjects)
		{
			if (gameObject != nullptr)
			{
				gameObject->CalcCameraZ(position, forward);
			}
		}

		//Z値でソート
		m_GameObjects.sort([](GameObject* a, GameObject* b)
			{
				return a->GetCameraZ() > b->GetCameraZ();
			});
	}


	//レイヤー順で描画
	for (int layer = 0; layer < 4; layer++)
	{
		for (GameObject* gameObject : m_GameObjects)
		{
			if (gameObject != nullptr)
			{
				if(gameObject->GetLayer()==layer)
				gameObject->Draw();
			}
		}
	}
	
	//デバッグ表示（ImGui::Render より前に線をためる）
	Collider::DrawGizmo();
	Gizmo::Draw();

	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());


	Renderer::End();
}
