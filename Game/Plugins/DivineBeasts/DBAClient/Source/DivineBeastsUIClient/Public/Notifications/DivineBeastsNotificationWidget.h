#pragma once

#include "Notifications/GamePlatformNotificationWidget.h"
#include "DivineBeastsNotificationWidget.generated.h"

/**
 * UDivineBeastsNotificationWidget（神兽联盟通知基类）。
 *
 * 用于奖励、任务、成就、解锁和系统公告等项目通知。
 * 通知只负责表现，不改变奖励、任务或权益等权威业务结果。
 */
UCLASS(Abstract, Blueprintable)
class DIVINEBEASTSUICLIENT_API UDivineBeastsNotificationWidget
    : public UGamePlatformNotificationWidget
{
    GENERATED_BODY()
};
