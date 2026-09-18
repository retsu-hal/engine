#include "GameObject.h"

// Engine.dll の中で1つだけ数える（GameScripts.dll で作ったオブジェクトとも ID が重ならない）
unsigned int GameObject::NewID()
{
	static unsigned int next = 1;
	return next++;
}
