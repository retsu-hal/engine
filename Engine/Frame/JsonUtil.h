#pragma once
#include "nlohmann/json.hpp"
#include "Vector3.h"
#include <string>

using json = nlohmann::json;

//=============================================================
// JSON の読み書きを短く書くための関数
//=============================================================
inline json ToJson(const Vector3& v) { return json::array({ v.x, v.y, v.z }); }
inline json ToJson(const XMFLOAT4& v) { return json::array({ v.x, v.y, v.z, v.w }); }

// キーがあるときだけ読む（ないときは今の値のまま＝初期値が使われる）
template<typename T>
inline void JsonRead(const json& data, const char* key, T& value)
{
	auto it = data.find(key);
	if (it != data.end() && !it->is_null()) value = it->get<T>();
}

inline void JsonRead(const json& data, const char* key, Vector3& value)
{
	auto it = data.find(key);
	if (it != data.end() && it->is_array() && it->size() == 3)
		value = Vector3((*it)[0].get<float>(), (*it)[1].get<float>(), (*it)[2].get<float>());
}

inline void JsonRead(const json& data, const char* key, XMFLOAT4& value)
{
	auto it = data.find(key);
	if (it != data.end() && it->is_array() && it->size() == 4)
		value = XMFLOAT4((*it)[0].get<float>(), (*it)[1].get<float>(), (*it)[2].get<float>(), (*it)[3].get<float>());
}

// wchar_t のパス ⇔ UTF-8 の文字列
inline std::string WideToUtf8(const std::wstring& text)
{
	if (text.empty()) return std::string();
	int size = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), (int)text.size(), nullptr, 0, nullptr, nullptr);
	std::string result(size, '\0');
	WideCharToMultiByte(CP_UTF8, 0, text.c_str(), (int)text.size(), &result[0], size, nullptr, nullptr);
	return result;
}

inline std::wstring Utf8ToWide(const std::string& text)
{
	if (text.empty()) return std::wstring();
	int size = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), (int)text.size(), nullptr, 0);
	std::wstring result(size, L'\0');
	MultiByteToWideChar(CP_UTF8, 0, text.c_str(), (int)text.size(), &result[0], size);
	return result;
}
