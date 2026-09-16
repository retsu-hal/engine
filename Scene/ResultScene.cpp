#include "main.h"
#include "Manager.h"
#include "Input.h"
#include "Renderer.h"
#include "Polygon2D.h"
#include "ResultScene.h"
#include "TitleScene.h"


void ResultScene::Init()
{
	Manager::AddGameObject<Polygon2D>()->Init(0.0f, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, L"asset\\texture\\DemoResult.png");
}

void ResultScene::Uninit()
{

}

void ResultScene::Update()
{
	if (Input::GetKeyTrigger(VK_RETURN) || Input::GetMouseTrigger(Input::MOUSE_LEFT))
	{
		Manager::ChangeScene<TitleScene>();
	}
}

void ResultScene::Draw()
{

}

