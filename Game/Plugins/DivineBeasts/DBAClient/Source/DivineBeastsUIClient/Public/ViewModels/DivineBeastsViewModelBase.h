#pragma once

#include "ViewModels/GamePlatformViewModelBase.h"
#include "DivineBeastsViewModelBase.generated.h"

/**
 * UDivineBeastsViewModelBase（神兽联盟视图模型基类）。
 *
 * 职责：
 * - 作为所有神兽联盟 UI ViewModel 的统一项目层父类。
 * - 继承平台 Revision（修订号）、PageGeneration（页面代次）和事件驱动刷新能力。
 * - 为后续账号、角色、世界、HUD、匹配、竞技和系统设置 ViewModel 提供稳定扩展点。
 *
 * 约束：
 * - 不创建 Widget，不操作 Viewport 或 CommonUI Stack。
 * - 不持有 Gameplay 权威状态，只保存 UI 安全的只读投影。
 * - 不使用 Tick 轮询业务对象。
 */
UCLASS(BlueprintType, Blueprintable)
class DIVINEBEASTSUICLIENT_API UDivineBeastsViewModelBase
    : public UGamePlatformViewModelBase
{
    GENERATED_BODY()
};
