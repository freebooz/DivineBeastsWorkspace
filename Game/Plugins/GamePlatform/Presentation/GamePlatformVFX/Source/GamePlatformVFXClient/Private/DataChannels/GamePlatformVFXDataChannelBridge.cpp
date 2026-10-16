// GamePlatformVFX通用NDC（Niagara数据通道）命中写入；不用旧版SearchParameters/Legacy API。
// 游戏线程使用真实UE5.8通道上下文；写入仅供GPU可见，不执行Gameplay命中、伤害或复制。
#include "DataChannels/GamePlatformVFXDataChannelBridge.h"
#include "Engine/World.h"
#include "NiagaraDataChannel.h"
#include "NiagaraDataChannelAccessor.h"
#include "NiagaraDataChannelAsset.h"
#include "NiagaraDataChannelFunctionLibrary.h"

bool FGamePlatformVFXDataChannelBridge::WriteInstantBurst(
    UWorld& World,
    const UNiagaraDataChannelAsset& ChannelAsset,
    const FGamePlatformVFXRequest& Request)
{
    check(IsInGameThread());
    if (World.bIsTearingDown || World.GetNetMode() == NM_DedicatedServer ||
        Request.PredictionState == EGamePlatformVFXPredictionState::Cancelled)
    {
        return false;
    }

    UNiagaraDataChannel* Channel = ChannelAsset.Get();
    if (!IsValid(Channel) || !Channel->IsValid()) return false;

    // GetTransientAccessContext只在游戏线程有效，且不得保存跨帧引用；Global通道可直接使用默认上下文。
    // 若配置为Islands/自定义分区，必须在资产制作阶段提供有效上下文，失败则回退独立Niagara。
    FNDCAccessContextInst& Context = Channel->GetTransientAccessContext();
    UNiagaraDataChannelWriter* Writer = UNiagaraDataChannelLibrary::CreateDataChannelWriter_WithContext(
        &World, Channel, Context, 1, false, false, true, TEXT("GamePlatformVFX.InstantBurst"));
    if (!IsValid(Writer)) return false;

    // 字段名称为平台中立合同；NDC原生资产必须具有Position/Normal/Color/Scale/StyleId/Seed六项。
    const FVector Position = Request.SpawnContext.ImpactLocation;
<<<<<<< HEAD
    const FVector Normal = Request.SpawnContext.ImpactNormal.GetSafeNormal(SMALL_NUMBER, FVector::UpVector);
=======
    // UE5.8首参为平方长度容差，第二参才是零向量回退；缺失命中法线时给GPU有效朝上法线。
    const FVector Normal = Request.SpawnContext.ImpactNormal.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
>>>>>>> 2099daa15f2914ed7e018b3b28408a637afe408e
    const FVector VisualScale = Request.SpawnContext.Scale;
    const double Scale = FMath::Clamp(FMath::Max3(VisualScale.X, VisualScale.Y, VisualScale.Z), 0.05, 8.0);
    const FLinearColor* RequestedColor = Request.Parameters.ColorParameters.Find(TEXT("User.PrimaryColor"));
    const int32* RequestedStyle = Request.Parameters.IntegerParameters.Find(TEXT("User.StyleId"));

    Writer->WritePosition(TEXT("Position"), 0, Position);
    Writer->WriteVector(TEXT("Normal"), 0, Normal);
    Writer->WriteLinearColor(TEXT("Color"), 0, RequestedColor ? *RequestedColor : FLinearColor::White);
    Writer->WriteFloat(TEXT("Scale"), 0, Scale);
    Writer->WriteInt(TEXT("StyleId"), 0, RequestedStyle ? *RequestedStyle : 0);
    Writer->WriteInt(TEXT("Seed"), 0, static_cast<int32>(GetTypeHash(Request.RequestId)));
    return true;
}
