#pragma once

class Component
{
	friend class GameObject;	// Start の呼び出し管理を GameObject に任せる

protected:
	class GameObject* m_GameObject = nullptr;

private:
	bool m_Started = false;		// Start を呼んだかどうか

	// 最初の Update の直前に一度だけ Start を呼ぶ
	void TryStart()
	{
		if (m_Started) return;
		m_Started = true;
		Start();
	}

public:
	Component() = delete;
	Component(GameObject* Object) { m_GameObject = Object; }
	virtual ~Component() {}

	GameObject* GetGameObject() const { return m_GameObject; }

	virtual void Init() {};		
	virtual void Start() {};	
	virtual void Uninit() {};
	virtual void Update() {};
	virtual void Draw() {};

	// Inspector に表示する項目（各コンポーネントで上書きする）
	virtual void OnInspectorGUI() {};
};