// 平台客户端私有JSON边界；只转换Shared合同，不拥有队伍状态或HTTP执行器。全部调用限游戏线程。
#pragma once
#include "Types/GamePlatformParty.h"

namespace GamePlatformPartyCodec
{
    /** 完整校验嵌套快照/收件箱后一次赋值；游标不允许隐式类型转换。 */
    bool DecodeObservation(const FString& Body, FGamePlatformPartyResult& Out);
    /** 完整解析后原子赋值；失败保留调用者旧快照，不接受精度受损、超限或身份矛盾的响应。 */
    bool DecodeSnapshot(const FString& Body, FGamePlatformPartySnapshot& Out);
    /** 解析本人邀请数组；失败保留旧输出，最多20项，重复身份失败关闭。 */
    bool DecodeInvitations(const FString& Body, TArray<FGamePlatformPartyRequest>& Out);
    /** 校验命令并生成中立正文；失败清空输出，不注入客户端自报主体。 */
    bool EncodeCommand(const FGamePlatformPartyCommand& Command, FString& Out);
    /** 命令回执必须回显同一逻辑键和受影响队伍；失败不发布快照。 */
    bool DecodeCommandResult(const FString& Body, const FGamePlatformPartyCommand& Command, FGamePlatformPartySnapshot& Out);
}
