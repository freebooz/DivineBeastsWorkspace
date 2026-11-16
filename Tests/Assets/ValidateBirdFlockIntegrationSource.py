#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SSLM鸟群正式接入合同：审查当前统一EnvironmentFlock（环境群落）实现。

迁移后鸟/蝶/鱼共用 StartEnvironmentFlock（启动环境群落）与
SubmitPendingEnvironmentFlocks（提交待完成群落）；旧SubmitPendingBirdFlocks接口
已退休，不允许为满足历史脚本恢复第二套状态机。仅静态验证源码和资源边界，
不替代UE5.8编译、实际Niagara GPU播放、Cook和人工验证。
"""
from pathlib import Path

root = Path(__file__).resolve().parents[2]
base = root / "Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsPresentationClient"
header = (base / "Public/DivineBeastsPresentationClientSubsystem.h").read_text(encoding="utf-8-sig")
bird = (base / "Private/Environment/DivineBeastsBirdFlockPresentation.cpp").read_text(encoding="utf-8-sig")
environment = (base / "Private/Environment/DivineBeastsEnvironmentFlockPresentation.cpp").read_text(encoding="utf-8-sig")
dispatcher = (base / "Private/DivineBeastsPresentationClientSubsystem.cpp").read_text(encoding="utf-8-sig")

for api in (
    "StartAmbientBirdFlock(", "StartAmbientBirdFlockForOwner(",
    "StartAmbientBirdFlockForOwnerAdvanced(",
    "StopAmbientBirdFlockForOwner(", "StopAmbientBirdFlock(",
    "StopAllAmbientBirdFlocks(", "GetRequestedBirdCount(",
):
    assert api in header, "当前正式鸟群API缺失：" + api

# 项目层必须通过同一环境群落事务、统一 Data 内容包与中立 Presentation/VFX 服务。
for contract in (
    "StartEnvironmentFlock(BirdFlocks", "StopEnvironmentFlockForOwner(BirdFlocks",
    "StopEnvironmentFlock(BirdFlocks", "StopAllEnvironmentFlocks(BirdFlocks",
    "World->GetNetMode() == NM_DedicatedServer",
    "bEnableVerifiedGPUFlight",
):
    assert contract in bird, "当前鸟群入口未满足统一服务/端侧/高级飞行门禁：" + contract

for contract in (
    "EnsureEnvironmentFlockPack(", "SubmitPendingEnvironmentFlocks(",
    "EDivineBeastsPresentationContentPackState::Active",
    "PlatformPresentation", "SubmitProjectRequest(Request)",
    "Request.PredictionState = EGamePlatformPresentationPredictionState::Confirmed",
    "Cancel.PredictionState = EGamePlatformPresentationPredictionState::Cancelled",
    "AddCompletionHandler(", "RemoveCompletionHandler(",
    "EnvironmentCompletionToken", "Group.Requests.RemoveAndCopyValue(",
    "Current->MutationToken != MutationToken",
    "Group.Requests.Num() >= 16",
):
    assert contract in environment, "环境群落统一事务的资源/取消/防重入合同缺失：" + contract

assert "SubmitPendingEnvironmentFlocks(BirdFlocks)" in dispatcher, \
    "Data内容包成为Active后，鸟群没有统一的异步提交路径"
assert "SubmitPendingEnvironmentFlocks(AmbientFauna)" in dispatcher, \
    "鸟/蝶/鱼群统一接入不应只保留旧鸟专用提交"
assert "PlatformPresentation->Submit(Request)" in dispatcher, \
    "正式表现必须经过平台分发，不得直接生成Niagara"

for text in (bird, environment):
    assert "SpawnSystemAtLocation" not in text and "NewObject<UNiagara" not in text, \
        "项目层不得绕过平台VFX实例/预算池直接生成Niagara"

assert "SubmitPendingBirdFlocks(" not in header, "已退休鸟群第二状态机不得重新公开"
print("BIRD_INTEGRATION_SOURCE_CONTRACT_OK: unified_environment_flocks, owner_tokens, async_pack, cancellation")
