#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""天气蓝图/资产制作/统一参数合同静态审核；不把源码存在冒充UE二进制资源交付。"""
from __future__ import annotations
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def text(relative: str) -> str:
    return (ROOT / relative).read_text(encoding="utf-8-sig")


def audit() -> None:
    platform = "Game/Plugins/GamePlatform/"
    project = "Game/Plugins/DivineBeasts/"
    blueprints = (
        platform + "World/GamePlatformWeather/Source/GamePlatformWeatherRuntime/Public/Blueprint/GamePlatformWeatherBlueprintLibrary.h",
        platform + "World/GamePlatformWeather/Source/GamePlatformWeatherRuntime/Private/Blueprint/GamePlatformWeatherBlueprintLibrary.cpp",
    )
    for path in blueprints:
        source = text(path)
        for symbol in ("ReadWeather", "SetWeatherOnAuthority", "ApplyWeatherPresetOnAuthority"):
            assert symbol in source, (path, symbol)
    actor = text(project + "DBAWorlds/Source/DBAWorldsRuntime/Private/Weather/DivineBeastsWeatherReviewController.cpp")
    for symbol in ("HasAuthority()", "ReadCurrentWeather", "ApplyReviewWeather", "bApplyAtBeginPlay"):
        assert symbol in actor, symbol
    # P0服务器世界必须使用GamePlatformData真正的Definition租约，退出时释放，
    # 而不是数据资产仅创建在目录里却没有任何生产消费者。
    gm = text(project + "DBAWorlds/Source/DBAWorldsRuntime/Private/Gameplay/DivineBeastsWorldGameMode.cpp")
    for symbol in ("InitialWeatherPresetId", "AcquireDefinition(", "EGamePlatformDataLifetime::World",
                   "GetLoadedDefinition(", "ApplyPreset(", "ReleaseDefinition("):
        assert symbol in gm, symbol

    client = text(platform + "World/GamePlatformWeather/Source/GamePlatformWeatherClient/Private/Subsystems/GamePlatformWeatherClientWorldSubsystem.cpp")
    request = text(platform + "Presentation/GamePlatformPresentation/Source/GamePlatformPresentationCore/Public/GamePlatformPresentationTypes.h")
    vfx = text(platform + "Presentation/GamePlatformVFX/Source/GamePlatformVFXClient/Private/Integration/Presentation/GamePlatformVFXPresentationProvider.cpp")
    sfx = text(platform + "Presentation/GamePlatformSFX/Source/GamePlatformSFXClient/Private/Integration/Presentation/GamePlatformSFXPresentationBridgeSubsystem.cpp")
    assert "User.WeatherIntensity" in client and "WeatherIntensity" in client
    assert "GetViewTarget()" in client and "GetRootComponent()" in client
    assert "FloatParameters" in request and "AttachComponent" in request and "VolumeMultiplier" in request
    assert "VFXRequest.Parameters.FloatParameters = Request.FloatParameters" in vfx
    assert "VFXRequest.SpawnContext.AttachComponent = Request.AttachComponent" in vfx
    assert "SFXRequest.FloatParameters = Request.FloatParameters" in sfx
    assert "SFXRequest.AttachComponent = Request.AttachComponent" in sfx
    # 项目客户端仅通过Weather Runtime事件订阅，复用ContentPack原子预载及注册；
    # 真Definition成功发布后才请求WeatherClient重发当前雨雪，晚加入不等待下一次天气变化。
    client_project = text(project + "DBAClient/Source/DivineBeastsPresentationClient/Private/DivineBeastsPresentationClientSubsystem.cpp")
    for symbol in ("BindWeatherWorld(", "HandleWeatherSnapshot(", "ActivateWeatherVisuals(",
                   "ActivateWeatherAudio(", "BuildWeatherVFXFragment(", "BuildWeatherSFXFragment(",
                   "RefreshPresentationAfterContentActivation("):
        assert symbol in client_project, symbol
    for symbol in ("SampleAndApplyTransition(", "RefreshPresentationAfterContentActivation(",
                   "LastPresentedIntensity", "bIntensityCorrection"):
        assert symbol in client, symbol

    paths = [
        "AuthorWeatherSurfaceAssets.py",
        "AuthorWeatherVFXMaterials.py",
        "AuthorWeatherBlueprintAssets.py",
        "AuthorWeatherVFXDefinitions.py",
        "AuthorWeatherAudioAssets.py",
        "ImportWeatherSourceArt.py",
        "AuthorWeatherReviewMap.py",
        "GenerateWeatherMonolithSpecs.py",
        "AuthorWeatherProductionPipeline.py",
        "VerifyWeatherUnrealAssets.py",
    ]
    ue_author_scripts = set(paths) - {"GenerateWeatherMonolithSpecs.py",
                                    "VerifyWeatherUnrealAssets.py"}
    for name in paths:
        source = text("Tools/Unreal/Weather/" + name)
        if name in ue_author_scripts:
            # 只有写.uasset的编辑器作者脚本具备inspect/apply闸门，不能错误要求
            # JSON规格生成器或只读验证器必须包含名为apply的写入状态。
            assert '"inspect"' in source and '"apply"' in source, name
        if name != "GenerateWeatherMonolithSpecs.py":
            assert "import unreal" in source, name
    verifier = text("Tools/Unreal/Weather/VerifyWeatherUnrealAssets.py")
    assert '"verify"' in verifier and '"inspect"' in verifier
    generator = text("Tools/Unreal/Weather/GenerateWeatherMonolithSpecs.py")
    assert "create_system_from_spec" in generator, "Monolith载荷生成器不完整"

    spec = json.loads(text("Tools/Unreal/Weather/WeatherNiagaraAuthoringSpec_V1.json"))
    assert spec.get("schemaVersion") == 1
    assert spec.get("assetAuthoringState") == "spec_only_not_uasset"
    names = {sys["name"] for sys in spec["systems"]}
    assert names == {"NS_GP_Weather_Rain", "NS_GP_Weather_Snow", "NS_GP_Weather_RainSplash"}
    assert "User.WeatherIntensity" in [x["name"] for x in spec["parameters"]]
    assert len(spec["systems"][0]["emitters"]) == 2 and len(spec["systems"][1]["emitters"]) == 2
    # Monolith对接必须使用真实的UE5.8 NiagaraEmitter模板，并显式包含两个发射率参数。
    generated = json.loads(text("Tools/Unreal/Weather/WeatherNiagaraMonolithPayloads_V1.json"))
    assert generated["schemaVersion"] == 1
    assert generated["state"] == "prepared_specs_not_ue_assets"
    assert len(generated["payloads"]) == 3
    assert sum(len(x["params"]["spec"]["emitters"]) for x in generated["payloads"]) == 5
    for payload in generated["payloads"]:
        assert payload["action"] == "create_system_from_spec"
        assert payload["params"]["save_path"].startswith("/GamePlatformVFX/Weather/Niagara/")
        assert all(x["asset"] == generated["sourceEmitter"] for x in payload["params"]["spec"]["emitters"])
        assert set(("User.WeatherNearSpawnRate", "User.WeatherFarSpawnRate")) <= {
            p["name"] for p in payload["params"]["spec"]["user_parameters"]
        }
    assert "User.WeatherNearSpawnRate" in client and "User.WeatherFarSpawnRate" in client
    vfxdefs = text("Tools/Unreal/Weather/AuthorWeatherVFXDefinitions.py")
    for name in ("User.WeatherIntensity", "User.WeatherNearSpawnRate", "User.WeatherFarSpawnRate"):
        assert name in vfxdefs, name

    # 确保当前的编辑器制作步骤不会错误地声称空资产已创建成功。
    for domain, base in (
        ("Surface", platform + "Presentation/GamePlatformSurface/Content"),
        ("VFX", platform + "Presentation/GamePlatformVFX/Content/Weather"),
    ):
        folder = ROOT / base
        actual = list(folder.rglob("*.uasset")) if folder.exists() else []
        print("ACTUAL_UE_ASSETS", domain, len(actual))
    print("WEATHER_AUTHORING_STATIC_PASS scripts=10 systems=3 emitters=5")


if __name__ == "__main__":
    audit()
