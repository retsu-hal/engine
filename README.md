# GM31 Engine

C++ / DirectX11 の自作ゲームエンジン（授業フレームワーク GM31_DX がベース）。

## フォルダ構成

```
GM31Engine.sln
├ Engine/        Engine.dll … 描画・GameObject・コンポーネント・エディタ（ImGui）
│   ├ Frame/ Component/ Object/ ThirdParty/ shader/
│   └ EngineAPI.h   … ENGINE_API（DLL から出すクラスに付ける印）
├ Editor/        Editor.exe（Game 構成では Player.exe）… Engine を起動するだけ
├ Sample/        サンプルのゲームプロジェクト（作業フォルダ）
│   ├ Script/ Scene/          … GameScripts.dll になるゲームのコード
│   ├ asset/                  … モデル・テクスチャ・シーン・project.json
│   └ GameScripts.vcxproj
└ bin/<構成>/    ビルド結果（Editor.exe・Engine.dll・GameScripts.dll）
```

## 使い方

1. `GM31Engine.sln` を開き、構成を **Debug / x64**、スタートアップを **Editor** にする
2. F5 で起動（作業フォルダは `Sample`）
3. ゲームのコードは `GameScripts` プロジェクトに書く。`REGISTER_COMPONENT(クラス名)` で Add Component に出る
4. 最初のシーンは `asset/project.json` の `startScene`（`.json` のパス、または `REGISTER_SCENE` したクラス名）

別のフォルダのプロジェクトを開く: `Editor.exe -project "C:\...\MyGame"`
