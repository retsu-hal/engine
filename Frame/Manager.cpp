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
#include "EditorGUI.h"
#include "EditorCamera.h"


//staticメンバー変数はcppで定義する必要がある
std::list<GameObject* > Manager::m_GameObjects;
Scene* Manager::m_Scene=nullptr;
Scene* Manager::m_NextScene=nullptr;
std::function<Scene*()> Manager::m_SceneFactory;
std::function<Scene*()> Manager::m_NextSceneFactory;
float Manager::m_ChangeSceneTime = 5.0f;

float Manager::m_DeltaTime = 1.0f / 60.0f;

void Manager::Init()
{
	Input::Init();
	Renderer::Init();
	Audio::InitMaster();

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	ImGui::StyleColorsDark();

	// 日本語フォント（assetに置いたフォントを優先し、なければWindows標準のメイリオを使う）
	{
		ImGuiIO& io = ImGui::GetIO();
		const char* fontPath = "asset\\font\\NotoSansJP-VariableFont_wght.ttf";
		if (GetFileAttributesA(fontPath) == INVALID_FILE_ATTRIBUTES)
		{
			fontPath = "C:\\Windows\\Fonts\\meiryo.ttc";
		}

		if (GetFileAttributesA(fontPath) != INVALID_FILE_ATTRIBUTES)
		{
			io.Fonts->AddFontFromFileTTF(fontPath, 18.0f);
		}
	}
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

	ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

	float  dt =GetDeltaTime();
	Input::Update();


	if (Input::GetKeyTrigger(VK_F1)) Gizmo::SetEnable(!Gizmo::IsEnable());


	// Play 中（または Step 要求があるとき）だけゲームを進める
	if (EditorGUI::ConsumeGameUpdate())
	{
		if (m_Scene != nullptr)	m_Scene->Update();

		for (GameObject* gameObject : m_GameObjects) gameObject->Update();

		//当たり判定（各オブジェクトのUpdateで移動し終わってから、まとめて押し出す）
		Collider::Check();
	}

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
			//先に全員の Uninit を済ませてから delete する（Uninit 中に他のオブジェクトを触っても安全なように）
			for (GameObject* gameObject : m_GameObjects) gameObject->Uninit();
			for (GameObject* gameObject : m_GameObjects) delete gameObject;
			m_GameObjects.clear();

			Profiler::Clear();
			m_Scene = m_NextScene;
			m_SceneFactory = m_NextSceneFactory;
			{
				ScopedSceneTimer sceneTimer;	// シーン全体の実ロード時間(実経過)
				m_Scene->Init();
			}
			Profiler::Dump();
			m_NextScene = nullptr;

			// 止まっている状態で読み込んだときも、1フレームだけ更新してカメラやアニメーションを初期状態にする
			if (EditorGUI::GetPlayState() != PlayState::Play) EditorGUI::RequestStep(1);
		}
	}
	// エディタカメラ（初回はゲームカメラの位置から始める）
	if (!EditorCamera::IsInitialized())
	{
		if (CAMERA* camera = GetGameObject<CAMERA>())
			EditorCamera::InitFrom(camera->GetPosition(), camera->GetTarget());
	}
	if (EditorGUI::UseEditorCamera()) EditorCamera::Update(EditorGUI::IsSceneViewHovered());

	EditorGUI::Draw();
};

void Manager::Draw()
{
	// ゲーム画面はシーン用テクスチャに描き、最後に ImGui のシーンビューで表示する
	Renderer::BeginScene();

	bool useEditorCamera = EditorGUI::UseEditorCamera() && EditorCamera::IsInitialized();


	

	//Z値計算
	CAMERA* camera = GetGameObject<CAMERA>();

	if (camera)
	{
		Vector3 forward = useEditorCamera ? EditorCamera::GetForward() : camera->GetForward();
		Vector3 position = useEditorCamera ? EditorCamera::GetPosition() : camera->GetPosition();

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
				if (gameObject->GetLayer() == layer)
				{
					gameObject->Draw();

					// ゲームカメラが行列を設定した直後に、エディタカメラの行列で上書きする
					if (useEditorCamera && dynamic_cast<CAMERA*>(gameObject)) EditorCamera::Apply();
				}
			}
		}
	}
	
	//デバッグ表示（ImGui::Render より前に線をためる）
	Collider::DrawGizmo();

	// ここからは画面（バックバッファ）に ImGui を描く
	Renderer::BeginBackBuffer();
	Gizmo::Draw();

	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());


	Renderer::End();
}

void Manager::RemoveGameObject(GameObject* gameobject)
{
	m_GameObjects.remove(gameobject);
	delete gameobject;
}

GameObject* Manager::FindGameObjectByID(unsigned int id)
{
	if (id == 0) return nullptr;
	for (GameObject* object : m_GameObjects)
	{
		if (object->GetID() == id && !object->IsDestroyed()) return object;
	}
	return nullptr;
}

GameObject* Manager::CreateGameObject(const std::string& name)
{
	GameObject* gameObject = AddGameObject<GameObject>();
	gameObject->SetName(name);
	return gameObject;
}

void Manager::ReloadScene()
{
	if (m_SceneFactory && m_NextScene == nullptr)
	{
		m_NextScene = m_SceneFactory();
		m_NextSceneFactory = m_SceneFactory;
		m_ChangeSceneTime = 0.0f;
	}
}
