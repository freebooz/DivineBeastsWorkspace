// 本文件属于平台客户端VFX复合排程；仅持有时序值，定时器/子实例由World服务登记并清理。
// 中文参数、必需子拒绝/取消与资源生命周期见本插件Docs/AuditRemediation-2026-10-09.md（2026-10-09本轮范围）。
#pragma once

#include "Definitions/GamePlatformVFXCompositeDefinition.h"
#include "TimerManager.h"
#include "Types/GamePlatformVFXHandle.h"
#include "Types/GamePlatformVFXRequest.h"

class UWorld;

/** Composite Definition 的轻量时序编排器。 */
class FGamePlatformVFXCompositeRunner
{
public:
    /** true只表示必需子事务执行/排队成功；false要求失败整树，不能静默跳过。 */
    using FPlayChild = TFunction<bool(
        FName,
        const FGamePlatformVFXRequest&,
        const FGamePlatformVFXHandle&)>;

    /** 由WorldSubsystem登记延迟步骤Timer，使父Handle取消时可以立即清除。 */
    using FRegisterTimer = TFunction<bool(
        const FGamePlatformVFXHandle&,
        const FTimerHandle&)>;

    /** 全步骤先预检再执行/排程；false时不能把未开始的父实例标Playing。 */
    static bool Run(
        UWorld& World,
        const UGamePlatformVFXCompositeDefinition& Definition,
        const FGamePlatformVFXRequest& Request,
        const FGamePlatformVFXHandle& ParentHandle,
        FPlayChild PlayChild,
        FRegisterTimer RegisterTimer);
};
