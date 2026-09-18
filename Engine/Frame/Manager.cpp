#include "main.h"
#include "Manager.h"
#include "Renderer.h"
#include "Camera.h"
#include "GameObject.h"
#include "Registry.h"
#include "Audio.h"
#include "ModelRenderer.h"
#include "PrimitiveRenderer.h"
#include "CameraComponent.h"
#include "SceneGrid.h"
#include "Console.h"
#include "ProjectSettings.h"
#include "SceneSerializer.h"
#include "ShaderManager.h"
#include "TextureManager.h"
#include "Collider.h"
#include "Gizmo.h"
#include "EditorGUI.h"
#include "EditorCamera.h"
#include "ImGuizmo.h"
#include "SceneSerializer.h"


// UTF-8 → ワイド文字（ウィンドウのタイトル用）
static std::wstring Utf8ToWideTitle(const std::string& text)
{
	int size = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
	std::wstring result(size > 0 ? size - 1 : 0, L'\0');
	if (size > 1) MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, &result[0], size);
	return result;
}

//staticメンバー変数はcppで定義する必要がある
std::list<GameObject* > Manager::m_GameObjects;
Scene* Manager::m_Scene=nullptr;
Scene* Manager::m_NextScene=nullptr;
std::function<Scene*()> Manager::m_SceneFactory;
std::function<Scene*()> Manager::m_NextSceneFactory;
float Manager::m_ChangeSceneTime = 5.0f;
bool  Manager::m_DrawingSceneView = false;

float Manager::m_DeltaTime = 1.0f / 60.0f;

void Manager::Init()
{
	Input::Init();
	Renderer::Init();
	Audio::InitMaster();
	ProjectSettings::Load();

#ifdef ENGINE_EDITOR
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
			fontPath = "asset\\font\\NotoSansJP-Regular.ttf";
		}
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
#else
	// ゲーム用 exe：タイトルを設定する
	SetWindowTextW(GetWindow(), Utf8ToWideTitle(ProjectSettings::Title).c_str());
#endif

	LoadStartScene();
}

//=============================================================
// 最初のシーン（ProjectSettings の StartScene）
//   "asset\scene\GameScene.json" のような .json → そのファイルを読む
//   "TitleScene" のようなクラス名           → REGISTER_SCENE したシーンを作る
//   どちらもなければ、カメラだけの空のシーン
//=============================================================
void Manager::LoadStartScene()
{
	const std::string& start = ProjectSettings::StartScene;
	bool isJson = start.size() > 5 && _stricmp(start.c_str() + start.size() - 5, ".json") == 0;

	if (isJson && GetFileAttributesA(start.c_str()) != INVALID_FILE_ATTRIBUTES)
	{
#ifdef ENGINE_EDITOR
		EditorGUI::OpenScene(start);	// 保存先もこのファイルになる
#else
		LoadSceneFile(start);
#endif
		return;
	}

	if (!isJson && ChangeSceneByName(start)) return;

	Debug::LogWarning("最初のシーンが見つかりません: %s（空のシーンで始めます）", start.c_str());
	LoadSceneText(EMPTY_SCENE_JSON);
}

bool Manager::ChangeSceneByName(const std::string& sceneClassName)
{
	if (m_NextScene != nullptr) return false;
	if (SceneRegistry::GetAll().count(sceneClassName) == 0) return false;

	m_NextSceneFactory = [sceneClassName]() -> Scene* { return SceneRegistry::Create(sceneClassName); };
	m_NextScene = m_NextSceneFactory();
	m_ChangeSceneTime = 0.0f;
	return true;
}


void Manager::Uninit()
{
#ifdef ENGINE_EDITOR
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
#endif
	
	
	for (GameObject* obj : m_GameObjects) obj->Uninit();
	for (GameObject* obj : m_GameObjects) delete obj;
	m_GameObjects.clear();
	
	if(m_Scene!=nullptr)
	{
		m_Scene->Uninit();
		delete m_Scene;
		m_Scene = nullptr;
	}

	// 切り替え待ちのシーンも消す
	if (m_NextScene != nullptr)
	{
		delete m_NextScene;
		m_NextScene = nullptr;
	}

	// シーンを作る関数は GameScripts.dll の中のラムダを持っていることがある。
	// DLL を外した後に片付けようとすると、なくなったコードを触って落ちるので今のうちに空にする
	m_SceneFactory = nullptr;
	m_NextSceneFactory = nullptr;

	ModelRenderer::UnloadAll();
	PrimitiveRenderer::UnloadAll();
	SceneGrid::Uninit();	
	ShaderManager::Unload();
	TextureManager::Unload();

	Gizmo::Uninit();
	Renderer::Uninit();
	Input::Uninit();
	Audio::UninitMaster();
}

void Manager::Update()
{
#ifdef ENGINE_EDITOR
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
	ImGuizmo::BeginFrame();

	// 画面全体をドッキング領域にする（初回は EditorGUI が Unity 風の配置を作る）
	EditorGUI::SetDockSpaceID(ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport()));
#endif

	float  dt =GetDeltaTime();
	Input::Update();


#ifdef ENGINE_EDITOR
	if (Input::GetKeyTrigger(VK_F1)) Gizmo::SetEnable(!Gizmo::IsEnable());
#endif


	// Play 中（または Step 要求があるとき）だけゲームを進める。ゲーム用 exe では常に進める
#ifdef ENGINE_EDITOR
	if (EditorGUI::ConsumeGameUpdate())
#endif
	{
		if (m_Scene != nullptr)	m_Scene->Update();

		for (GameObject* gameObject : m_GameObjects)
		{
			if (gameObject->IsActiveInHierarchy()) gameObject->Update();	// 非アクティブは止める
		}

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
#ifdef ENGINE_EDITOR
			if (EditorGUI::GetPlayState() != PlayState::Play) EditorGUI::RequestStep(1);
#endif
		}
	}
#ifdef ENGINE_EDITOR
	// エディタカメラ（初回はゲームカメラの位置から始める）
	if (!EditorCamera::IsInitialized())
	{
		if (CAMERA* camera = GetGameObject<CAMERA>())
			EditorCamera::InitFrom(camera->GetPosition(), camera->GetTarget());
	}
	EditorCamera::Update(EditorGUI::IsSceneViewHovered());	// Scene ビューはいつでもエディタカメラ

	EditorGUI::Draw();
#endif
};

void Manager::Draw()
{
#ifdef ENGINE_EDITOR
	// Scene ビュー（エディタカメラ）と Game ビュー（ゲームのカメラ）を別々のテクスチャに描く
	// 表示されていないタブは描かない（重くならないように）
	if (EditorGUI::IsSceneViewVisible()) DrawWorld(true);
	if (EditorGUI::IsGameViewVisible())  DrawWorld(false);
	m_DrawingSceneView = false;

	// ここからは画面（バックバッファ）に ImGui を描く
	Renderer::BeginBackBuffer();
	Gizmo::Draw();

	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
#else
	// ゲーム用 exe：画面に直接ゲームを描く
	DrawWorld(false, true);
#endif

	Renderer::End();
}

void Manager::DrawWorld(bool sceneView, bool toBackBuffer)
{
	m_DrawingSceneView = sceneView;
	if (toBackBuffer) Renderer::BeginGameOnBackBuffer();
	else Renderer::BeginScene(sceneView ? Renderer::VIEW_SCENE : Renderer::VIEW_GAME);

	bool useEditorCamera = sceneView && EditorCamera::IsInitialized();
	CameraComponent* mainCamera = sceneView ? nullptr : CameraComponent::GetMain();

	// 従来の CAMERA が設定した行列を、エディタカメラかカメラコンポーネントで上書きする
	auto applyOverride = [&]()
	{
		if (useEditorCamera) EditorCamera::Apply();
		else if (mainCamera) mainCamera->Apply();
	};
	applyOverride();

	//Z値計算
	CAMERA* camera = GetGameObject<CAMERA>();

	if (camera || useEditorCamera || mainCamera)
	{
		Vector3 forward, position;
		if (useEditorCamera)
		{
			forward = EditorCamera::GetForward();
			position = EditorCamera::GetPosition();
		}
		else if (mainCamera)
		{
			XMMATRIX world = mainCamera->GetGameObject()->GetWorldMatrix();
			XMStoreFloat3((XMFLOAT3*)&forward, XMVector3Normalize(world.r[2]));
			position = mainCamera->GetGameObject()->GetWorldPosition();
		}
		else
		{
			forward = camera->GetForward();
			position = camera->GetPosition();
		}

		for (GameObject* gameObject : m_GameObjects)
		{
			if (gameObject != nullptr) gameObject->CalcCameraZ(position, forward);
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
		// 2D（レイヤー3）の前に、Scene ビューだけ床のグリッドを描く
		if (layer == 3 && useEditorCamera)
		{
			EditorCamera::Apply();
			SceneGrid::Draw(EditorCamera::GetPosition());
		}

		for (GameObject* gameObject : m_GameObjects)
		{
			if (gameObject != nullptr && gameObject->GetLayer() == layer && gameObject->IsActiveInHierarchy())
			{
				gameObject->Draw();

				// ゲームカメラが行列を設定した直後に、エディタカメラ／カメラコンポーネントの行列で上書きする
				if (dynamic_cast<CAMERA*>(gameObject)) applyOverride();
			}
		}
	}

	// コライダーの線は Scene ビューにだけ出す（ImGui::Render より前に線をためる）
	if (sceneView) Collider::DrawGizmo();
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

std::string Manager::MakeUniqueName(const std::string& name)
{
	// 「Tree(3)」なら元の名前「Tree」を取り出す
	std::string base = name;
	if (!base.empty() && base.back() == ')')
	{
		size_t open = base.find_last_of('(');
		if (open != std::string::npos && open + 1 < base.size() - 1)
		{
			std::string number = base.substr(open + 1, base.size() - open - 2);
			bool digits = !number.empty();
			for (char c : number) if (c < '0' || c > '9') digits = false;
			if (digits) base = base.substr(0, open);
		}
	}

	auto used = [](const std::string& candidate)
	{
		for (GameObject* object : m_GameObjects)
			if (!object->IsDestroyed() && object->GetName() == candidate) return true;
		return false;
	};

	if (!used(name)) return name;
	for (int i = 1; ; i++)
	{
		std::string candidate = base + "(" + std::to_string(i) + ")";
		if (!used(candidate)) return candidate;
	}
}

GameObject* Manager::CreateGameObject(const std::string& name)
{
	std::string uniqueName = MakeUniqueName(name);	// 追加する前に調べる（自分自身と比べないように）
	GameObject* gameObject = AddGameObject<GameObject>();
	gameObject->SetName(uniqueName);
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

void Manager::LoadSceneFile(const std::string& path)
{
	if (m_NextScene != nullptr) return;
	m_NextSceneFactory = [path]() -> Scene* { return new FileScene(path, false); };
	m_NextScene = m_NextSceneFactory();
	m_ChangeSceneTime = 0.0f;
}

void Manager::LoadSceneText(const std::string& text)
{
	if (m_NextScene != nullptr) return;
	m_NextSceneFactory = [text]() -> Scene* { return new FileScene(text, true); };
	m_NextScene = m_NextSceneFactory();
	m_ChangeSceneTime = 0.0f;
}

GameObject* Manager::AddGameObjectInstance(GameObject* gameObject, const std::string& name)
{
	if (gameObject == nullptr) return nullptr;
	gameObject->SetName(MakeUniqueName(name));
	gameObject->Init();
	m_GameObjects.push_back(gameObject);
	return gameObject;
}

//=============================================================
// タグで探す（破棄予定・非アクティブのものは除く）
//=============================================================
GameObject* Manager::FindWithTag(const std::string& tag)
{
	for (GameObject* object : m_GameObjects)
	{
		if (!object->IsDestroyed() && object->IsActiveInHierarchy() && object->CompareTag(tag)) return object;
	}
	return nullptr;
}

std::vector<GameObject*> Manager::FindGameObjectsWithTag(const std::string& tag)
{
	std::vector<GameObject*> result;
	for (GameObject* object : m_GameObjects)
	{
		if (!object->IsDestroyed() && object->IsActiveInHierarchy() && object->CompareTag(tag)) result.push_back(object);
	}
	return result;
}
