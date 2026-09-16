#pragma once

class Component
{
	friend class GameObject;	// Start の呼び出し管理を GameObject に任せる

protected:
	class GameObject* m_GameObject = nullptr;

private:
	bool m_Started = false;		// Start を呼んだかどうか
	bool m_Enabled = true;		// false の間は Update / Draw が呼ばれない

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

	void SetEnabled(bool enabled) { m_Enabled = enabled; }
	bool IsEnabled() const { return m_Enabled; }

	virtual void Init() {};		// AddComponent した瞬間に呼ばれる（Unity の Awake）
	virtual void Start() {};	// 最初の Update の直前に一度だけ呼ばれる
	virtual void Uninit() {};
	virtual void Update() {};
	virtual void Draw() {};

	// 他のコライダーと当たったときに呼ばれる（Unity の OnCollisionEnter / OnTriggerEnter に近い。ただし毎フレーム呼ばれる）
	virtual void OnCollision(GameObject* other) {};

	// Inspector に表示する項目（各コンポーネントで上書きする）
	virtual void OnInspectorGUI() {};
};
