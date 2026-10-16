// GamePlatformVFX通用NDC（Niagara数据通道）命中写入；不用旧版SearchParameters/Legacy API。
// 游戏线程使用真实UE5.8通道上下文；写入仅供GPU可见，不执行Gameplay命中、伤害或复制。
#include "DataChannels/GamePlatformVFXDataChannelBridge.h"
#include "Engine/World.h"
#include "NiagaraDataChannel.h"
#include "NiagaraDataChannelAccessContext.h"
#include "NiagaraDataChannel_Global.h"
#include "NiagaraDataChannelAccessor.h"
#include "NiagaraDataChannelAsset.h"
#include "NiagaraDataChannelFunctionLibrary.h"

namespace
{
constexpr int32 MaxSharedBurstBatchSize = 128;
}

/** 单条请求走同一安全路径；批量调用需由上层在确认去重、同通道及生命周期后发起。 */
bool FGamePlatformVFXDataChannelBridge::WriteInstantBurst(
    UWorld& World,
    const UNiagaraDataChannelAsset& ChannelAsset,
    const FGamePlatformVFXRequest& Request)
{
    return WriteInstantBursts(World, ChannelAsset, TConstArrayView<FGamePlatformVFXRequest>(&Request, 1));
}

bool FGamePlatformVFXDataChannelBridge::WriteInstantBursts(
    UWorld& World,
    const UNiagaraDataChannelAsset& ChannelAsset,
    TConstArrayView<FGamePlatformVFXRequest> Requests)
{
    check(IsInGameThread());
    // 只有经过上层预算/幂等校验的事件才能写入；批次上限防止极端帧产生大分配。
    if (World.bIsTearingDown || World.GetNetMode() == NM_DedicatedServer ||
        Requests.IsEmpty() || Requests.Num() > MaxSharedBurstBatchSize)
    {
        return false;
    }

    for (const FGamePlatformVFXRequest& Request : Requests)
    {
        if (Request.PredictionState == EGamePlatformVFXPredictionState::Cancelled ||
            Request.SpawnContext.ImpactLocation.ContainsNaN())
        {
            return false; // 全批拒绝，不能在失败后产生部分视觉。
        }
    }

    UNiagaraDataChannel* Channel = ChannelAsset.Get();
    if (!IsValid(Channel) || !Channel->IsValid()) return false;

    // 共享批次仅允许Global；Islands/MapBase必须逐命中按真实位置路由，未确认空间分组前不可跨岛合并。
    if (Requests.Num() > 1 && !Channel->IsA<UNiagaraDataChannel_Global>()) return false;

    // UE5.8瞬时上下文禁止跨帧保存。对Islands必须写入真实位置，否则事件会错误进入原点岛屿。
    FNDCAccessContextInst& Context = Channel->GetTransientAccessContext();
    if (FNDCAccessContext* ContextParameters = Context.Get<FNDCAccessContext>())
    {
        ContextParameters->Location = Requests[0].SpawnContext.ImpactLocation;
        ContextParameters->bOverrideLocation = true;
    }
    else if (!Channel->IsA<UNiagaraDataChannel_Global>())
    {
        // UE5.8当前引擎内部分区通道若仍声明Legacy上下文，不能安全按位置路由；拒绝并回退独立System。
        return false;
    }
    // Global无需位置路由，可接受引擎默认的Legacy上下文；写入接口仍采用_WithContext非旧版API。

    UNiagaraDataChannelWriter* Writer = UNiagaraDataChannelLibrary::CreateDataChannelWriter_WithContext(
        &World, Channel, Context, Requests.Num(), false, false, true, TEXT("GamePlatformVFX.InstantBurst"));
    if (!IsValid(Writer)) return false;

    // 六字段必须与真实NDC资产一致；请求身份保留在WorldSubsystem，GPU只接收中立视觉参数。
    for (int32 Index = 0; Index < Requests.Num(); ++Index)
    {
        const FGamePlatformVFXRequest& Request = Requests[Index];
        const FVector Normal = Request.SpawnContext.ImpactNormal.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
        const FVector VisualScale = Request.SpawnContext.Scale;
        const double Scale = FMath::Clamp(FMath::Max3(VisualScale.X, VisualScale.Y, VisualScale.Z), 0.05, 8.0);
        const FLinearColor* RequestedColor = Request.Parameters.ColorParameters.Find(TEXT("User.PrimaryColor"));
        const int32* RequestedStyle = Request.Parameters.IntegerParameters.Find(TEXT("User.StyleId"));

        Writer->WritePosition(TEXT("Position"), Index, Request.SpawnContext.ImpactLocation);
        Writer->WriteVector(TEXT("Normal"), Index, Normal);
        Writer->WriteLinearColor(TEXT("Color"), Index, RequestedColor ? *RequestedColor : FLinearColor::White);
        Writer->WriteFloat(TEXT("Scale"), Index, Scale);
        Writer->WriteInt(TEXT("StyleId"), Index, RequestedStyle ? *RequestedStyle : 0);
        Writer->WriteInt(TEXT("Seed"), Index, static_cast<int32>(GetTypeHash(Request.RequestId)));
    }
    return true; // 仅表示Niagara Writer已受理，Reader渲染成功须由GPU测试证明。
}
