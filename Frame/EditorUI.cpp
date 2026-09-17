#include "main.h"
#include "EditorUI.h"
#include <cctype>

//=============================================================
// アイコンごとの記号の大きさ（ボタンの高さに対する割合）
//=============================================================
static float IconRadiusScale(EditorUI::Icon icon)
{
	switch (icon)
	{
	case EditorUI::Icon::Play:
	case EditorUI::Icon::Pause:
	case EditorUI::Icon::Stop: return 0.28f;
	case EditorUI::Icon::Grid: return 0.30f;
	default:                   return 0.32f;
	}
}

namespace EditorUI
{

//=============================================================
// 記号のボタン（フォントに記号がなくても表示できるよう、図形で描く）
//=============================================================
bool IconButton(const char* id, Icon icon, bool active, const char* tooltip, float widthScale)
{
	float h = ImGui::GetFrameHeight();

	if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
	bool pressed = ImGui::Button(id, ImVec2(h * widthScale, h));
	if (active) ImGui::PopStyleColor();

	if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tooltip);

	ImVec2 min = ImGui::GetItemRectMin();
	ImVec2 max = ImGui::GetItemRectMax();
	ImVec2 c((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f);
	float r = h * IconRadiusScale(icon);	// 記号の大きさ
	ImU32 color = ImGui::GetColorU32(ImGuiCol_Text);
	ImDrawList* dl = ImGui::GetWindowDrawList();
	const float t = 1.5f;	// 線の太さ

	switch (icon)
	{
	case Icon::Play:	// ▶
		dl->AddTriangleFilled(ImVec2(c.x - r * 0.8f, c.y - r), ImVec2(c.x - r * 0.8f, c.y + r), ImVec2(c.x + r, c.y), color);
		break;

	case Icon::Pause:	// ❚❚
		dl->AddRectFilled(ImVec2(c.x - r * 0.8f, c.y - r), ImVec2(c.x - r * 0.25f, c.y + r), color);
		dl->AddRectFilled(ImVec2(c.x + r * 0.25f, c.y - r), ImVec2(c.x + r * 0.8f, c.y + r), color);
		break;

	case Icon::Stop:	// ■
		dl->AddRectFilled(ImVec2(c.x - r * 0.85f, c.y - r * 0.85f), ImVec2(c.x + r * 0.85f, c.y + r * 0.85f), color);
		break;

	case Icon::Move:	// 十字の矢印
	{
		dl->AddLine(ImVec2(c.x - r, c.y), ImVec2(c.x + r, c.y), color, t);
		dl->AddLine(ImVec2(c.x, c.y - r), ImVec2(c.x, c.y + r), color, t);
		float a = r * 0.35f;
		dl->AddTriangleFilled(ImVec2(c.x + r + 1, c.y), ImVec2(c.x + r - a, c.y - a), ImVec2(c.x + r - a, c.y + a), color);
		dl->AddTriangleFilled(ImVec2(c.x - r - 1, c.y), ImVec2(c.x - r + a, c.y + a), ImVec2(c.x - r + a, c.y - a), color);
		dl->AddTriangleFilled(ImVec2(c.x, c.y - r - 1), ImVec2(c.x + a, c.y - r + a), ImVec2(c.x - a, c.y - r + a), color);
		dl->AddTriangleFilled(ImVec2(c.x, c.y + r + 1), ImVec2(c.x - a, c.y + r - a), ImVec2(c.x + a, c.y + r - a), color);
		break;
	}

	case Icon::Rotate:	// 回る矢印
	{
		dl->PathArcTo(c, r * 0.85f, XM_PI * 0.15f, XM_PI * 1.75f, 20);
		dl->PathStroke(color, 0, t);
		ImVec2 tip(c.x + cosf(XM_PI * 1.75f) * r * 0.85f, c.y + sinf(XM_PI * 1.75f) * r * 0.85f);
		float a = r * 0.4f;
		dl->AddTriangleFilled(ImVec2(tip.x + a, tip.y), ImVec2(tip.x - a * 0.3f, tip.y - a), ImVec2(tip.x - a * 0.3f, tip.y + a * 0.6f), color);
		break;
	}

	case Icon::Scale:	// 小さい四角から大きい四角へ伸びる
	{
		dl->AddRect(ImVec2(c.x - r, c.y - r), ImVec2(c.x + r, c.y + r), color, 0.0f, 0, t);
		dl->AddRectFilled(ImVec2(c.x - r, c.y + r * 0.1f), ImVec2(c.x - r * 0.1f, c.y + r), color);
		dl->AddLine(ImVec2(c.x - r * 0.2f, c.y + r * 0.2f), ImVec2(c.x + r * 0.7f, c.y - r * 0.7f), color, t);
		float a = r * 0.35f;
		dl->AddTriangleFilled(ImVec2(c.x + r * 0.85f, c.y - r * 0.85f), ImVec2(c.x + r * 0.85f - a * 1.4f, c.y - r * 0.85f), ImVec2(c.x + r * 0.85f, c.y - r * 0.85f + a * 1.4f), color);
		break;
	}

	case Icon::Local:	// 立方体（自分の向き）
	{
		float s = r * 0.7f, o = r * 0.4f;
		dl->AddRect(ImVec2(c.x - s - o * 0.5f, c.y - s + o * 0.5f), ImVec2(c.x + s - o * 0.5f, c.y + s + o * 0.5f), color, 0.0f, 0, t);
		dl->AddLine(ImVec2(c.x - s - o * 0.5f, c.y - s + o * 0.5f), ImVec2(c.x - s + o * 0.5f, c.y - s - o * 0.5f), color, t);
		dl->AddLine(ImVec2(c.x + s - o * 0.5f, c.y - s + o * 0.5f), ImVec2(c.x + s + o * 0.5f, c.y - s - o * 0.5f), color, t);
		dl->AddLine(ImVec2(c.x + s - o * 0.5f, c.y + s + o * 0.5f), ImVec2(c.x + s + o * 0.5f, c.y + s - o * 0.5f), color, t);
		dl->AddLine(ImVec2(c.x - s + o * 0.5f, c.y - s - o * 0.5f), ImVec2(c.x + s + o * 0.5f, c.y - s - o * 0.5f), color, t);
		dl->AddLine(ImVec2(c.x + s + o * 0.5f, c.y - s - o * 0.5f), ImVec2(c.x + s + o * 0.5f, c.y + s - o * 0.5f), color, t);
		break;
	}

	case Icon::World:	// 地球（円と経線・緯線）
		dl->AddCircle(c, r, color, 20, t);
		dl->AddLine(ImVec2(c.x - r, c.y), ImVec2(c.x + r, c.y), color, t);
		dl->AddEllipse(c, ImVec2(r * 0.45f, r), color, 0.0f, 20, t);
		break;

	case Icon::Grid:	// 井桁
		for (int i = -1; i <= 1; i += 2)
		{
			dl->AddLine(ImVec2(c.x + r * 0.4f * i, c.y - r), ImVec2(c.x + r * 0.4f * i, c.y + r), color, t);
			dl->AddLine(ImVec2(c.x - r, c.y + r * 0.4f * i), ImVec2(c.x + r, c.y + r * 0.4f * i), color, t);
		}
		break;
	}
	return pressed;
}

//=============================================================
// 画面の縦横比を保ったまま、ウィンドウに収まる大きさを求める
//=============================================================
ImVec2 FitAspect(const ImVec2& avail, float aspect)
{
	ImVec2 size(avail.x, avail.x / aspect);
	if (size.y > avail.y) size = ImVec2(avail.y * aspect, avail.y);
	if (size.x < 1.0f || size.y < 1.0f) size = ImVec2(1.0f, 1.0f);
	return size;
}

	// "BoxCollider" → "Box Collider"（Unity の ObjectNames.NicifyVariableName と同じ考え方）
	std::string NicifyName(const std::string& name)
	{
		std::string result;
		for (size_t i = 0; i < name.size(); i++)
		{
			char c = name[i];
			if (i > 0 && isupper((unsigned char)c))
			{
				char prev = name[i - 1];
				bool nextLower = (i + 1 < name.size()) && islower((unsigned char)name[i + 1]);
				// 小文字→大文字、または "UIText" の "T" のように略語の終わり
				if (islower((unsigned char)prev) || isdigit((unsigned char)prev) || (isupper((unsigned char)prev) && nextLower))
					result += ' ';
			}
			if (c == '_') { result += ' '; continue; }
			result += c;
		}
		return result;
	}

	// 種類ごとのアイコン（1文字）と色
	void GetComponentIcon(const std::string& typeName, const char*& icon, ImVec4& color)
	{
		auto has = [&](const char* word) { return typeName.find(word) != std::string::npos; };
		if (has("Collider"))       { icon = "C"; color = ImVec4(0.35f, 0.85f, 0.40f, 1.0f); }
		else if (has("Rigidbody")) { icon = "R"; color = ImVec4(0.30f, 0.75f, 0.95f, 1.0f); }
		else if (has("Camera"))    { icon = "Ca"; color = ImVec4(0.70f, 0.55f, 0.95f, 1.0f); }
		else if (has("Audio"))     { icon = "A"; color = ImVec4(0.95f, 0.65f, 0.25f, 1.0f); }
		else if (has("Animation")) { icon = "An"; color = ImVec4(0.95f, 0.45f, 0.60f, 1.0f); }
		else if (has("Renderer") || has("Model")) { icon = "M"; color = ImVec4(0.40f, 0.60f, 1.00f, 1.0f); }
		else if (has("Light"))     { icon = "L"; color = ImVec4(1.00f, 0.90f, 0.35f, 1.0f); }
	}

	// CollapsingHeader の直後に呼ぶ：見出しの上にアイコン・有効チェック・名前を重ねて描き、右端に「…」ボタンを置く位置へ移動する
	void ComponentHeaderLabel(const char* label, const char* icon, const ImVec4& iconColor, bool* enabled)
	{
		ImVec2 min = ImGui::GetItemRectMin();
		ImVec2 max = ImGui::GetItemRectMax();
		float height = max.y - min.y;
		ImDrawList* draw = ImGui::GetWindowDrawList();

		// アイコン（色付きの角丸四角＋文字）
		float x = min.x + ImGui::GetTreeNodeToLabelSpacing();
		float size = height - 6.0f;
		ImVec2 iconMin(x, min.y + 3.0f);
		ImVec2 iconMax(x + size, min.y + 3.0f + size);
		draw->AddRectFilled(iconMin, iconMax, ImGui::GetColorU32(iconColor), 3.0f);
		ImVec2 textSize = ImGui::CalcTextSize(icon);
		draw->AddText(ImVec2(iconMin.x + (size - textSize.x) * 0.5f, iconMin.y + (size - textSize.y) * 0.5f), IM_COL32(20, 20, 20, 255), icon);
		x += size + 6.0f;

		// 有効チェック（見出しの上に重ねて置く）
		if (enabled)
		{
			ImGui::SameLine(x - ImGui::GetWindowPos().x + ImGui::GetScrollX());
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2.0f, 2.0f));
			ImGui::Checkbox("##Enabled", enabled);
			ImGui::PopStyleVar();
			x = ImGui::GetItemRectMax().x + 6.0f;
		}

		// 名前（無効なら薄く）
		ImU32 textColor = ImGui::GetColorU32((enabled && !*enabled) ? ImGuiCol_TextDisabled : ImGuiCol_Text);
		draw->AddText(ImVec2(x, min.y + (height - ImGui::GetFontSize()) * 0.5f), textColor, label);

		// 右端の「…」ボタンの位置
		float buttonWidth = ImGui::CalcTextSize("...").x + ImGui::GetStyle().FramePadding.x * 2.0f;
		ImGui::SameLine(max.x - ImGui::GetWindowPos().x + ImGui::GetScrollX() - buttonWidth - 4.0f);
	}

	// Unity 風の Vector3 の行：「Position   [X ___] [Y ___] [Z ___]」
	// X/Y/Z の色付きラベルを左右にドラッグしても値が変わる。右クリックで 0（Scale は 1）に戻す
	bool Vector3Field(const char* label, Vector3& value, float speed, float labelWidth)
	{
		bool changed = false;
		ImGui::PushID(label);

		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted(label);
		ImGui::SameLine(labelWidth);

		float spacing = ImGui::GetStyle().ItemSpacing.x;
		float letterWidth = ImGui::GetFrameHeight();
		float fieldWidth = (ImGui::GetContentRegionAvail().x - spacing * 2.0f) / 3.0f - letterWidth;
		if (fieldWidth < 20.0f) fieldWidth = 20.0f;

		const char* letters[] = { "X", "Y", "Z" };
		const ImVec4 colors[] = {
			ImVec4(0.80f, 0.25f, 0.25f, 1.0f),
			ImVec4(0.30f, 0.65f, 0.25f, 1.0f),
			ImVec4(0.25f, 0.45f, 0.85f, 1.0f) };
		float* values[] = { &value.x, &value.y, &value.z };
		float resetValue = (strcmp(label, "Scale") == 0) ? 1.0f : 0.0f;

		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, ImGui::GetStyle().ItemSpacing.y));
		for (int i = 0; i < 3; i++)
		{
			ImGui::PushID(i);
			if (i > 0) ImGui::SameLine(0.0f, spacing);

			ImGui::PushStyleColor(ImGuiCol_Button, colors[i]);
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(colors[i].x * 1.2f, colors[i].y * 1.2f, colors[i].z * 1.2f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, colors[i]);
			ImGui::Button(letters[i], ImVec2(letterWidth, 0.0f));
			ImGui::PopStyleColor(3);

			if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
			if (ImGui::IsItemActive() && ImGui::GetIO().MouseDelta.x != 0.0f)
			{
				*values[i] += ImGui::GetIO().MouseDelta.x * speed;
				changed = true;
			}
			if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
			{
				*values[i] = resetValue;
				changed = true;
			}

			ImGui::SameLine();
			ImGui::SetNextItemWidth(fieldWidth);
			if (ImGui::DragFloat("##v", values[i], speed, 0.0f, 0.0f, "%.3f")) changed = true;
			ImGui::PopID();
		}
		ImGui::PopStyleVar();

		ImGui::PopID();
		return changed;
	}

}
