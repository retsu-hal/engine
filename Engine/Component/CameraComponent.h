#pragma once
#include "EngineAPI.h"
#include "Component.h"

// オブジェクトの位置と向きから画面を映すカメラ
// Priority が一番大きい有効なカメラがゲーム画面に使われる（なければ従来の CAMERA）
class ENGINE_API CameraComponent : public Component
{
private:
	float m_Fov = 60.0f;		// 縦の視野角（度）
	float m_Near = 0.1f;
	float m_Far = 1000.0f;
	int   m_Priority = 0;

public:
	using Component::Component;

	static CameraComponent* GetMain();	// 今使うカメラ（なければ nullptr）

	XMMATRIX GetViewMatrix() const;
	XMMATRIX GetProjectionMatrix() const;
	void Apply() const;					// Renderer に行列を設定する

	void Draw() override;				// 止めている間は視野の枠を線で表示する
	void OnInspectorGUI() override;
	void Serialize(nlohmann::json& data) const override;
	void Deserialize(const nlohmann::json& data) override;
};
