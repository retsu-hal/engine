#pragma once
#include <string>
#include <vector>
#include "Vector3.h"

//=============================================================
// EditorGUI を分けたファイル同士で使う小さな関数（中身は EditorGUI.cpp）
//=============================================================
namespace EditorInternal
{
	// 今シーンビューに映しているカメラの行列（止めている間はエディタカメラ、Play 中はゲームカメラ）
	bool GetActiveViewProjection(XMMATRIX& view, XMMATRIX& projection);

	// 回転行列 → GameObject の回転（XMMatrixRotationRollPitchYaw と同じ並び。x=pitch y=yaw z=roll）
	Vector3 RotationFromQuaternion(FXMVECTOR quaternion);

	// マウス位置からシーンビューの奥へ伸ばした光線（選択・ドロップ位置で使う）
	void ScreenRay(const XMMATRIX& viewProjection, Vector3& origin, Vector3& direction);

	// asset\scene にある .json の一覧（"asset\scene\GameScene.json" の形）
	std::vector<std::string> GetSceneFiles();
}
