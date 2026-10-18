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
 * 独立为UI、法术、新手村音乐和环境声建立内容包事务。
 * 某类定义预载失败不得阻止其他领域的播放；音乐与按钮互相不依赖。
 */
void UDivineBeastsPresentationClientSubsystem::EnsureStarterAudioForWorld(UWorld* World)
{
    check(IsInGameThread());
    ULocalPlayer* Player = GetLocalPlayer();
    if (bClosing || !World || World->bIsTearingDown || !World->IsGameWorld() ||
        World->GetNetMode() == NM_DedicatedServer || !Player ||
        Player->GetWorld() != World || !PlatformPresentation)
        return;

    // 一台客户端的世界音乐由第一个LocalPlayer独占，防止分屏双倍播放。
    UGameInstance* Instance = Player->GetGameInstance();
    if (!Instance || Instance->GetFirstGamePlayer() != Player) return;

    const FString MapName = World->GetMapName();
    // 首期只根据正式地图身份启用自动背景音乐，避免开发审核地图误触发。
    const bool bVillage = MapName.Contains(TEXT("L_Village_Start"), ESearchCase::IgnoreCase);
    const bool bFrontEnd = MapName.Contains(TEXT("L_DBA_FrontEnd"), ESearchCase::IgnoreCase) ||
        MapName.Contains(TEXT("L_DBA_CharacterStudio"), ESearchCase::IgnoreCase);
    if (StarterAudioWorld.Get() != World)
    {
        StopStarterAudioForWorld();
        StarterAudioWorld = World;
    }

    auto EnsureGroup = [this, World](FName Group, int32 ExpectedCount,
        FDivineBeastsPresentationContentPackHandle& Slot)
    {
        if (Slot.IsValid())
        {
            FString Error;
            const auto State = GetContentPackState(Slot, Error);
            if (State == EDivineBeastsPresentationContentPackState::Loading ||
                State == EDivineBeastsPresentationContentPackState::Active) return;
            Slot = {};
        }

        FDivineBeastsPresentationContentPackFragment Pack;
        Pack.ContentPackId = FName(*(FString(TEXT("DBA.Audio.Starter.")) + Group.ToString()));
        Pack.Revision = 1;
        Pack.LifecycleScope = EGamePlatformPresentationContextScope::World;
        Pack.bRequiredPreload = true;
        // 在校验GameplayTag之前筛选分类；其它音效的缺陷不会影响本类。
        Pack.CatalogFragment = FDivineBeastsPresentationProjectCatalog::BuildStarterAudioFragment(Group);
        Pack.CatalogFragment.FragmentId = Pack.ContentPackId;
        Pack.CatalogFragment.Scope = EGamePlatformPresentationCatalogScope::ContentPack;
        Pack.CatalogFragment.OwnerScopeId = Pack.ContentPackId;
        Pack.CatalogFragment.LifecycleScope = Pack.LifecycleScope;
        // 内容包层级与各条目Scope必须完全一致，否则正式PreflightCatalogFragment拒绝发布。
        for (FGamePlatformPresentationCatalogEntry& Entry : Pack.CatalogFragment.Entries)
            Entry.Scope = EGamePlatformPresentationCatalogScope::ContentPack;
        if (Pack.CatalogFragment.Entries.Num() != ExpectedCount)
        {
            UE_LOG(LogTemp, Warning, TEXT("SSLM音频分类 %s 不完整，拒绝半包激活。"), *Group.ToString());
            return;
        }
        FString Error;
        // Pending句柄先发布给Slot，防止同步完成回调比ActivateContentPack返回更早。
        const auto Handle = ActivateContentPack(Pack, Error, &Slot);
        if (!Handle.IsValid())
        {
            Slot = {};
            UE_LOG(LogTemp, Warning, TEXT("SSLM音频分类 %s 预载失败：%s"), *Group.ToString(), *Error);
        }
        else if (!bClosing && StarterAudioWorld.Get() == World) Slot = Handle;
        else if (Handle.IsValid()) DeactivateContentPack(Handle);
    };

    EnsureGroup(TEXT("UI"), 3, StarterAudioPack);
    EnsureGroup(TEXT("Spell"), 3, StarterSpellPack);
    if (bVillage)
    {
        EnsureGroup(TEXT("Music.Village"), 1, StarterMusicPack);
        EnsureGroup(TEXT("Ambience.Forest"), 1, StarterAmbiencePack);
    }
    else if (bFrontEnd)
        EnsureGroup(TEXT("Music.Login"), 1, StarterMusicPack);

    // 同步完成数据预载时，也保证音乐导演能够在所属句柄登记后启动一次。
    SubmitStarterAudioForWorld();
}

/** 先提交循环声音取消，再撤销自身拥有的内容包注册和租约。
 * 世界正在销毁时不再向旧世界发声，由平台SFX World清理实例。
 */
/** 先取消当前世界背景音乐及环境，再撤销本地玩家所拥有的四个分类目录。 */
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

    // 先失效本地所有权，后注销目录；同步通知不得重复关闭新世界句柄。
    const FDivineBeastsPresentationContentPackHandle Packs[] = {
        StarterAudioPack, StarterSpellPack, StarterMusicPack, StarterAmbiencePack
    };
    StarterAudioPack = {};
    StarterSpellPack = {};
    StarterMusicPack = {};
    StarterAmbiencePack = {};
    StarterAudioWorld.Reset();
    for (const auto& Pack : Packs)
        if (Pack.IsValid()) DeactivateContentPack(Pack);
}

/** 内容包全部完成且目录成功注册后自动播放世界级音乐和森林声音。 */
/** BGM与环境各自预载成功后独立启动；不因按钮或法术定义失败而静音。 */
void UDivineBeastsPresentationClientSubsystem::SubmitStarterAudioForWorld()
{
    check(IsInGameThread());
    UWorld* World = StarterAudioWorld.Get();
    if (bClosing || !World || World->bIsTearingDown || !PlatformPresentation ||
        !GetLocalPlayer() || GetLocalPlayer()->GetWorld() != World) return;

    const FString MapName = World->GetMapName();
    const bool bVillage = MapName.Contains(TEXT("L_Village_Start"), ESearchCase::IgnoreCase);
    const bool bFrontEnd = MapName.Contains(TEXT("L_DBA_FrontEnd"), ESearchCase::IgnoreCase) ||
        MapName.Contains(TEXT("L_DBA_CharacterStudio"), ESearchCase::IgnoreCase);
    if (!bVillage && !bFrontEnd) return;

    auto SubmitLoop = [this, World](const TCHAR* TagName, FGuid& OutId, FGameplayTag* OutTag)
    {
        if (OutId.IsValid() || bClosing || StarterAudioWorld.Get() != World) return;
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
            // Submitted只代表平台SFX已经接受播放请求，不能代替音频设备实际发声的检查。
            UE_LOG(LogTemp, Log, TEXT("SSLM场景音频请求已受理：%s；地图：%s"),
                TagName, *World->GetMapName());
        }
    };

    FString Error;
    if (StarterMusicPack.IsValid() &&
        GetContentPackState(StarterMusicPack, Error) == EDivineBeastsPresentationContentPackState::Active)
        SubmitLoop(bVillage ? TEXT("Presentation.SSLM.Audio.Music.Village") :
            TEXT("Presentation.SSLM.Audio.Music.Login"), StarterMusicRequestId, &StarterMusicTag);
    Error.Reset();
    if (bVillage && StarterAmbiencePack.IsValid() &&
        GetContentPackState(StarterAmbiencePack, Error) == EDivineBeastsPresentationContentPackState::Active)
        SubmitLoop(TEXT("Presentation.SSLM.Audio.Ambience.Forest"), StarterAmbienceRequestId, nullptr);
}

/** UI与施法声音统一调用，不允许发送任意路径或绕开平台目录/并发策略。 */
bool UDivineBeastsPresentationClientSubsystem::SubmitStarterOneShot(
    const TCHAR* Semantic, const FName ContextId, const FVector& Location)
{
    check(IsInGameThread());
    UWorld* World = StarterAudioWorld.Get();
    // 只有对应音频分类的真实Definition预载完成，才能提交按钮/技能表现请求。
    const FString TagName(Semantic);
    const auto& Pack = TagName.StartsWith(TEXT("Presentation.SSLM.Audio.UI."))
        ? StarterAudioPack : StarterSpellPack;
    if (bClosing || !World || World->bIsTearingDown || !PlatformPresentation ||
        Location.ContainsNaN() || !Pack.IsValid())
        return false;
    FString Error;
    if (GetContentPackState(Pack, Error) != EDivineBeastsPresentationContentPackState::Active)
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
bool UDivineBeastsPresentationClientSubsystem::PlayStarterSpellSound(
    const FName SpellName, const bool bImpact, const FVector& Location)
{
    // 奥术飞弹复用已入库的奥术脉冲，飞行与命中分别提交，阶段去重归弹体事实适配。
    if (SpellName == TEXT("ArcaneMissiles"))
        return PlayAudioLibraryClip(TEXT("Spell.ArcaneCast"), Location);
    const TCHAR* Semantic =
        SpellName == TEXT("Frostbolt") ? (bImpact ?
            TEXT("Presentation.SSLM.Audio.Spell.Frost.Impact") :
            TEXT("Presentation.SSLM.Audio.Spell.Frost.Cast")) :
        (SpellName == TEXT("Fireball") || SpellName == TEXT("DirectedMissile")) && bImpact ?
            TEXT("Presentation.SSLM.Audio.Spell.Fire.Impact") : nullptr;
    return Semantic && SubmitStarterOneShot(Semantic, SpellName, Location);
}
