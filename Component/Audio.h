#pragma once
#include <string>

#include <xaudio2.h>
#include "Component.h"


class Audio : public Component
{
private:
	static IXAudio2*				m_Xaudio;
	static IXAudio2MasteringVoice*	m_MasteringVoice;

	IXAudio2SourceVoice*	m_SourceVoice{};
	BYTE*					m_SoundData{};

	int						m_Length{};
	int						m_PlayLength{};

	std::string				m_FileName;	// 保存用


public:
	static void InitMaster();
	static void UninitMaster();

	using Component::Component;

	void Uninit() override;

	void Load(const char *FileName);
	void Play(bool Loop = false);

	void OnInspectorGUI() override;
	void Serialize(nlohmann::json& data) const override;
	void Deserialize(const nlohmann::json& data) override;


};

