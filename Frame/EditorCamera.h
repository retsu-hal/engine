#pragma once
#include "main.h"
#include "Vector3.h"

// 編集中にシーンを見回すためのカメラ（ゲームの CAMERA とは別物）
// 操作: シーンビュー上で右ドラッグ=視点回転、右ドラッグ中に WASD/QE=移動、ホイール=前後移動
class EditorCamera
{
private:
	static Vector3 m_Position;
	static float   m_Yaw;		// 左右の向き（ラジアン）
	static float   m_Pitch;		// 上下の向き（ラジアン）
	static float   m_MoveSpeed;
	static bool    m_Initialized;

public:
	// 最初にエディタカメラを使うとき、ゲームカメラと同じ場所から始める
	static void InitFrom(const Vector3& position, const Vector3& target);
	static bool IsInitialized() { return m_Initialized; }

	static void Update(bool sceneViewHovered);

	static Vector3  GetPosition() { return m_Position; }
	static Vector3  GetForward();
	static XMMATRIX GetViewMatrix();
	static XMMATRIX GetProjectionMatrix();

	// Renderer に行列を設定する
	static void Apply();

	static void OnInspectorGUI();
};
