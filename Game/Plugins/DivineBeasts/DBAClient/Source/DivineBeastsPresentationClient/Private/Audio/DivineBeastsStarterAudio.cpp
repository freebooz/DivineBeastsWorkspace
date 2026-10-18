// SSLM第三层首批音频接线：只负责世界/场景音频语义，不重新实现平台SFX播放器。
// 客户端游戏线程运行；目录由GamePlatformPresentation持有，资源由GamePlatformData租约保留。
// 音效失败不得改变登录、施法、伤害及天气等权威结果。
#include "DivineBeastsPresentationClientSubsystem.h"
#include "Catalog/DivineBeastsPresentationProjectCatalog.h"
#include "GamePlatformPresentationClientSubsystem.h"
#include "Identity/DivineBeastsProjectCatalog.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

/** 创建客户端World级音频目录并提交真实Data预载请求。
 * 缺声音资产/标签时不登记半成品；避免每帧轮询或创建重复播放组件。
 */
void UDivineBeastsPresentationClientSubsystem::EnsureStarterAudioForWorld(UWorld* World)
{
    check(IsInGameThread());
    ULocalPlayer* Player = GetLocalPlayer();
    if (bClosing || !World || World->bIsTearingDown || !World->IsGameWorld() ||
        World->GetNetMode() == NM_DedicatedServer || !Player ||
        Player->GetWorld() != World || !PlatformPresentation)
        return;

    // 首版2D音乐/环境只允许主本地玩家提交一次，避免分屏重复叠音。
    UGameInstance* Instance = Player->GetGameInstance();
    if (!Instance || Instance->GetFirstGamePlayer() != Player)
        return;

    if (StarterAudioWorld.Get() == World && StarterAudioPack.IsValid())
    {
        FString ExistingError;
        const auto State = GetContentPackState(StarterAudioPack, ExistingError);
        if (State == EDivineBeastsPresentationContentPackState::Loading ||
            State == EDivineBeastsPresentationContentPackState::Active)
            return;
    }
    StopStarterAudioForWorld();

    FDivineBeastsPresentationContentPackFragment Pack;
    Pack.ContentPackId = TEXT("DBA.Audio.Starter");
    Pack.Revision = 1;
    Pack.LifecycleScope = EGamePlatformPresentationContextScope::World;
    Pack.bRequiredPreload = true;
    Pack.CatalogFragment = FDivineBeastsPresentationProjectCatalog::BuildStarterAudioFragment();
    if (Pack.CatalogFragment.Entries.Num() != 9)
    {
        UE_LOG(LogTemp, Warning, TEXT("SSLM首批音频标签缺失，不挂载部分目录。"));
        return;
    }
    Pack.CatalogFragment.Scope = EGamePlatformPresentationCatalogScope::ContentPack;
    Pack.CatalogFragment.OwnerScopeId = Pack.ContentPackId;
    Pack.CatalogFragment.LifecycleScope = Pack.LifecycleScope;
    for (auto& Entry : Pack.CatalogFragment.Entries)
        Entry.Scope = EGamePlatformPresentationCatalogScope::ContentPack;

    FString Error;
    const auto Handle = ActivateContentPack(Pack, Error, &StarterAudioPack);
    if (!Handle.IsValid())
    {
        StarterAudioPack = {};
        UE_LOG(LogTemp, Warning, TEXT("SSLM音频内容包预载未受理：%s"), *Error);
        return;
    }
    StarterAudioWorld = World;
    StarterAudioPack = Handle;
}

/** 先提交循环声音取消，再撤销自身拥有的内容包注册和租约。
 * 世界正在销毁时不再向旧世界发声，由平台SFX World清理实例。
 */
void UDivineBeastsPresentationClientSubsystem::StopStarterAudioForWorld()
{
    check(IsInGameThread());
    const UWorld* OldWorld = StarterAudioWorld.Get();
    if (PlatformPresentation && OldWorld && !OldWorld->bIsTearingDown)
    {
        auto Cancel = [this](const FGuid& RequestId, const FGameplayTag& SemanticTag)
        {
            if (!RequestId.IsValid() || !SemanticTag.IsValid()) return;
            FGamePlatformPresentationRequest Request;
            Request.RequestId = RequestId;
            Request.SemanticTag = SemanticTag;
            Request.Context.ProjectId = FDivineBeastsProjectCatalog::GetProjectId();
            Request.PredictionState = EGamePlatformPresentationPredictionState::Cancelled;
            SubmitProjectRequest(Request);
        };
        Cancel(StarterMusicRequestId, StarterMusicTag);
        Cancel(StarterAmbienceRequestId, FGameplayTag::RequestGameplayTag(
            TEXT("Presentation.SSLM.Audio.Ambience.Forest"), false));
    }
    StarterMusicRequestId.Invalidate();
    StarterAmbienceRequestId.Invalidate();
    StarterMusicTag = {};
    const auto Pack = StarterAudioPack;
    StarterAudioPack = {};
    StarterAudioWorld.Reset();
    if (Pack.IsValid()) DeactivateContentPack(Pack);
}

/** 内容包全部完成且目录成功注册后自动播放世界级音乐和森林声音。 */
void UDivineBeastsPresentationClientSubsystem::SubmitStarterAudioForWorld()
{
    check(IsInGameThread());
    UWorld* World = StarterAudioWorld.Get();
    if (bClosing || !World || World->bIsTearingDown || !PlatformPresentation ||
        !GetLocalPlayer() || GetLocalPlayer()->GetWorld() != World)
        return;
    FString Error;
    if (!StarterAudioPack.IsValid() ||
        GetContentPackState(StarterAudioPack, Error) != EDivineBeastsPresentationContentPackState::Active)
        return;

    // 场景识别仅用于首批原型配乐，正式音乐导演应基于ExperienceId/WorldDefinition。
    const FString MapName = World->GetMapName();
    const bool bVillage = MapName.Contains(TEXT("Village"), ESearchCase::IgnoreCase);
    const bool bFrontEnd = MapName.Contains(TEXT("FrontEnd"), ESearchCase::IgnoreCase) ||
        MapName.Contains(TEXT("CharacterStudio"), ESearchCase::IgnoreCase);
    if (!bVillage && !bFrontEnd)
        return;

    auto SubmitLoop = [this, World](const TCHAR* TagName, FGuid& OutId, FGameplayTag* OutTag)
    {
        if (OutId.IsValid() || bClosing || StarterAudioWorld.Get() != World)
            return;
        FGamePlatformPresentationRequest Request;
        Request.RequestId = FGuid::NewGuid();
        Request.SemanticTag = FGameplayTag::RequestGameplayTag(FName(TagName), false);
        if (!Request.SemanticTag.IsValid()) return;
        Request.ContextId = TEXT("SSLM.Audio.Scene");
        Request.Context.ProjectId = FDivineBeastsProjectCatalog::GetProjectId();
        Request.WorldGeneration = PlatformPresentation->GetWorldGeneration();
        Request.Lifetime = EGamePlatformPresentationLifetime::Persistent;
        Request.Priority = EGamePlatformPresentationPriority::Low;
        const auto Result = SubmitProjectRequest(Request);
        if (Result == EGamePlatformPresentationSubmitResult::Submitted &&
            !bClosing && StarterAudioWorld.Get() == World)
        {
            OutId = Request.RequestId;
            if (OutTag) *OutTag = Request.SemanticTag;
        }
    };
    SubmitLoop(bVillage ? TEXT("Presentation.SSLM.Audio.Music.Village") :
        TEXT("Presentation.SSLM.Audio.Music.Login"), StarterMusicRequestId, &StarterMusicTag);
    if (bVillage)
        SubmitLoop(TEXT("Presentation.SSLM.Audio.Ambience.Forest"), StarterAmbienceRequestId, nullptr);
}

/** UI与施法声音统一调用，不允许发送任意路径或绕开平台目录/并发策略。 */
bool UDivineBeastsPresentationClientSubsystem::SubmitStarterOneShot(
    const TCHAR* Semantic, const FName ContextId, const FVector& Location)
{
    check(IsInGameThread());
    UWorld* World = StarterAudioWorld.Get();
    if (bClosing || !World || World->bIsTearingDown || !PlatformPresentation ||
        Location.ContainsNaN() || !StarterAudioPack.IsValid())
        return false;
    FString Error;
    if (GetContentPackState(StarterAudioPack, Error) != EDivineBeastsPresentationContentPackState::Active)
        return false;

    FGamePlatformPresentationRequest Request;
    Request.RequestId = FGuid::NewGuid();
    Request.SemanticTag = FGameplayTag::RequestGameplayTag(FName(Semantic), false);
    if (!Request.SemanticTag.IsValid()) return false;
    Request.Context.ProjectId = FDivineBeastsProjectCatalog::GetProjectId();
    Request.ContextId = ContextId;
    Request.SourceLocation = Location;
    Request.ImpactLocation = Location;
    Request.WorldGeneration = PlatformPresentation->GetWorldGeneration();
    return SubmitProjectRequest(Request) == EGamePlatformPresentationSubmitResult::Submitted;
}

/** 项目UI可通过蓝图/C++调用；首期提供Click/Confirm/Cancel三个固定动作。 */
bool UDivineBeastsPresentationClientSubsystem::PlayStarterUISound(FName ActionId)
{
    const TCHAR* Semantic =
        ActionId == TEXT("Click") ? TEXT("Presentation.SSLM.Audio.UI.Click") :
        ActionId == TEXT("Confirm") ? TEXT("Presentation.SSLM.Audio.UI.Confirm") :
        ActionId == TEXT("Cancel") ? TEXT("Presentation.SSLM.Audio.UI.Cancel") : nullptr;
    return Semantic && SubmitStarterOneShot(Semantic, ActionId, FVector::ZeroVector);
}

/** 根据已授权项目魔法视觉事实增补音效，不自己创建法术或者判定命中。 */
void UDivineBeastsPresentationClientSubsystem::PlayStarterSpellSound(
    const FName SpellName, const bool bImpact, const FVector& Location)
{
    const TCHAR* Semantic =
        SpellName == TEXT("Frostbolt") ? (bImpact ?
            TEXT("Presentation.SSLM.Audio.Spell.Frost.Impact") :
            TEXT("Presentation.SSLM.Audio.Spell.Frost.Cast")) :
        SpellName == TEXT("Fireball") && bImpact ?
            TEXT("Presentation.SSLM.Audio.Spell.Fire.Impact") : nullptr;
    if (Semantic) SubmitStarterOneShot(Semantic, SpellName, Location);
}
