#pragma once
#include "EngineAPI.h"
#include "Component.h"

struct ShaderSet;

// 四角・球・カプセルをプログラムで作って描くコンポーネント（モデルファイル不要）
// 大きさは Unity と同じ：Cube 1×1×1、Sphere 直径1、Capsule 高さ2・半径0.5（どれも中心が原点）
class ENGINE_API PrimitiveRenderer : public Component
{
public:
	enum class Shape { Cube, Sphere, Capsule, Count };

private:
	struct Mesh
	{
		ID3D11Buffer* VertexBuffer = nullptr;
		ID3D11Buffer* IndexBuffer = nullptr;
		UINT          IndexCount = 0;
	};
	static Mesh m_Meshes[(int)Shape::Count];	// 形ごとに1つだけ作って使い回す

	static void CreateMesh(Shape shape);

	Shape            m_Shape = Shape::Cube;
	XMFLOAT4         m_Color = { 1.0f, 1.0f, 1.0f, 1.0f };
	const ShaderSet* m_Shader = nullptr;

public:
	using Component::Component;

	static void UnloadAll();

	void Init() override;
	void Draw() override;

	void SetShape(Shape shape) { m_Shape = shape; }
	void SetColor(const XMFLOAT4& color) { m_Color = color; }

	void OnInspectorGUI() override;
	void Serialize(nlohmann::json& data) const override;
	void Deserialize(const nlohmann::json& data) override;
};
