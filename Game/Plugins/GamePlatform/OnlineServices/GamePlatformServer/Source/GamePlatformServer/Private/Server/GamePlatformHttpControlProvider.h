#pragma once

#include "Server/GamePlatformServerLifecycleSubsystem.h"

class FJsonObject;

/** HTTP传输适配器只在服务器模块内可见；不公开凭据、请求对象或JSON实现细节。 */
class FGamePlatformHttpControlProvider final
    : public IGamePlatformServerControlProvider
{
public:
    virtual void RegisterInstance(
        const FGamePlatformServerInstanceInfo& Instance,
        FGamePlatformServerControlCompletion Completion) override;
    virtual void SendHeartbeat(
        const FGamePlatformServerInstanceInfo& Instance,
        int32 CurrentPlayers,
        const FString& Status,
        FGamePlatformServerControlCompletion Completion) override;
    virtual void PublishReady(
        const FGamePlatformServerInstanceInfo& Instance,
        FGamePlatformServerControlCompletion Completion) override;
    virtual void BeginDrain(
        const FGamePlatformServerInstanceInfo& Instance,
        FGamePlatformServerControlCompletion Completion) override;

private:
    static void SendAcceptedPost(
        const FGamePlatformServerInstanceInfo& Instance,
        const FString& RelativePath,
        const TSharedRef<FJsonObject>& Body,
        FGamePlatformServerControlCompletion Completion);
};
