#pragma once

#include "CoreMinimal.h"

/** 当前认证玩家的公共资料值快照；仅 DisplayName 可通过本插件申请更新。 */
struct FGamePlatformOnlineProfile
{
    /** 服务端从认证主体解析的玩家身份，必须与本地认证上下文一致。 */
    FString PlayerId;
    /** 必须与实例配置一致的游戏身份。 */
    FString GameId;
    /** 服务端返回的显示名；不是客户端权威字段。 */
    FString DisplayName;
    /** 正数资料结构版本；0 表示未取得有效资料。 */
    int32 DataVersion = 0;
    /** 非负持久化修订号；-1 表示未取得，缓存只接受同主体不降低修订号的响应。 */
    int64 Revision = -1;
    /** 只读教学完成事实，本插件不提供修改入口。 */
    bool bTutorialCompleted = false;
    /** 可选世界逻辑身份；空值不允许客户端凭空选择服务器或执行切服。 */
    FString DefaultWorldId;
    /** 后端返回的只读角色身份，不是客户端授权发放入口。 */
    TArray<FString> OwnedCharacterIds;
};
