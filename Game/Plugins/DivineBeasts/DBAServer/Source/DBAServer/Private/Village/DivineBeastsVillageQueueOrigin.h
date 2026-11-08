#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IHttpRequest.h"
#include "Server/GamePlatformServerAdmissionSubsystem.h"

class ACharacter;

namespace DivineBeasts::VillageQueueOrigin
{
    /** 只有已落地的权威Pawn可提供胶囊中心、朝向及当前地图包。 */
    bool Capture(const ACharacter& Pawn, const FString& CharacterId, FGamePlatformVerifiedReturnSpawn& OutSpawn);
    /** 以现有内网凭据和已验证连接身份创建一次报告；调用方持有和取消请求。 */
    FHttpRequestPtr MakeReport(const FGamePlatformServerVerifiedAdmission& Admission, const FString& QueueTicketId,
        const FGamePlatformVerifiedReturnSpawn& Spawn);
    /** 对当前已完成角色配置的胶囊复查精确位置、真实地面和遮挡；失败保持普通出生。 */
    bool TryRestore(ACharacter& Pawn, const FGamePlatformVerifiedReturnSpawn& Spawn);
}
