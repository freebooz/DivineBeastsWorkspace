// SSLM第二批音频库运行时接线：所有音频必须通过Project Catalog→Presentation→平台SFX执行器。
// 不持有SoundBase/AudioComponent，不执行任何服务器战斗判定；世界/账号退出先取消循环再撤租约。
#include "DivineBeastsPresentationClientSubsystem.h"
#include "Catalog/DivineBeastsPresentationProjectCatalog.h"
#include "GamePlatformPresentationClientSubsystem.h"
#include "Identity/DivineBeastsProjectCatalog.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Components/SkeletalMeshComponent.h"

namespace
{
/** 根据受控语义前缀解析资源类；拒绝外部传入任意资产路径或不受支持的领域名。 */
FName CategoryForAudioClip(FName ClipId)
{
    const FString Clip = ClipId.ToString();
    FString Family;
    FString Variant;
    if (!Clip.Split(TEXT("."), &Family, &Variant) || Variant.IsEmpty()) return NAME_None;
    if (Family == TEXT("Footstep")) return TEXT("Footsteps");
    if (Family == TEXT("Creature")) return TEXT("Creatures");
    if (Family == TEXT("Spell")) return TEXT("Spells");
    if (Family == TEXT("Ambience")) return TEXT("Ambience");
    if (Family == TEXT("UI")) return TEXT("UI");
    if (Family == TEXT("World")) return TEXT("World");
    if (Family == TEXT("Water")) return TEXT("Water");
    if (Family == TEXT("Combat")) return TEXT("Combat");
    return NAME_None;
}

bool IsVillageSoundWorld(const UWorld* World)
{
    return World && World->GetMapName().Contains(TEXT("Village"), ESearchCase::IgnoreCase);
}
}

void UDivineBeastsPresentationClientSubsystem::EnsureAudioLibraryForWorld(UWorld* World)
{
    check(IsInGameThread());
    auto* Player = GetLocalPlayer();
    if (bClosing || !World || World->bIsTearingDown || !World->IsGameWorld() ||
        World->GetNetMode() == NM_DedicatedServer || !Player || Player->GetWorld() != World ||
        !PlatformPresentation || !Player->GetGameInstance() ||
        Player->GetGameInstance()->GetFirstGamePlayer() != Player) return;
    if (AudioLibraryWorld.Get() != World)
    {
        StopAudioLibraryForWorld();
        AudioLibraryWorld = World;
    }
    // 前端只预热界面音/法术审查；Village按区域和玩法加载共用材质音效，不自动播放无锚点循环声。
    const FName Groups[] = { TEXT("UI"), TEXT("Spells"), TEXT("Footsteps"), TEXT("Combat"),
        TEXT("Creatures"), TEXT("World"), TEXT("Water"), TEXT("Ambience") };
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Groups); ++Index)
    {
        if (Index >= 2 && !IsVillageSoundWorld(World)) break;
        FString Error;
        const auto Handle = ActivateAudioLibraryCategory(Groups[Index], Error);
        if (!Handle.IsValid() && !Error.IsEmpty())
            UE_LOG(LogTemp, Verbose, TEXT("SSLM可选音频分类 %s 未就绪：%s"),
                *Groups[Index].ToString(), *Error);
    }
}

FDivineBeastsPresentationContentPackHandle
UDivineBeastsPresentationClientSubsystem::ActivateAudioLibraryCategory(FName Category, FString& OutError)
{
    check(IsInGameThread());
    OutError.Reset();
    UWorld* World = GetLocalPlayer() ? GetLocalPlayer()->GetWorld() : nullptr;
    if (bClosing || !World || World->bIsTearingDown || !World->IsGameWorld() ||
        World->GetNetMode() == NM_DedicatedServer || World != AudioLibraryWorld.Get() ||
        !PlatformPresentation)
    {
        OutError = TEXT("音频世界不可用，不能登记资源库。");
        return {};
    }
    if (const auto* Existing = AudioLibraryCategories.Find(Category))
    {
        FString StateError;
        const auto State = GetContentPackState(*Existing, StateError);
        if (State == EDivineBeastsPresentationContentPackState::Loading ||
            State == EDivineBeastsPresentationContentPackState::Active)
            return *Existing;
        AudioLibraryCategories.Remove(Category);
    }

    auto Catalog = FDivineBeastsPresentationProjectCatalog::BuildAudioLibraryFragment(Category);
    if (Catalog.Entries.IsEmpty())
    {
        OutError = TEXT("音频分类不存在或必要GameplayTag未登记；拒绝发布空目录。");
        return {};
    }

    FDivineBeastsPresentationContentPackFragment Pack;
    Pack.ContentPackId = FName(*(FString(TEXT("DBA.Audio.Library.")) + Category.ToString()));
    Pack.Revision = 1;
    Pack.LifecycleScope = EGamePlatformPresentationContextScope::World;
    Pack.bRequiredPreload = true;
    Pack.CatalogFragment = MoveTemp(Catalog);
    Pack.CatalogFragment.Scope = EGamePlatformPresentationCatalogScope::ContentPack;
    Pack.CatalogFragment.OwnerScopeId = Pack.ContentPackId;
    Pack.CatalogFragment.LifecycleScope = Pack.LifecycleScope;
    for (auto& Entry : Pack.CatalogFragment.Entries)
        Entry.Scope = EGamePlatformPresentationCatalogScope::ContentPack;
    const auto Handle = ActivateContentPack(Pack, OutError);
    if (Handle.IsValid() && !bClosing && AudioLibraryWorld.Get() == World)
        AudioLibraryCategories.Add(Category, Handle);
    else if (Handle.IsValid())
        DeactivateContentPack(Handle);
    return (AudioLibraryWorld.Get() == World && !bClosing) ? Handle : FDivineBeastsPresentationContentPackHandle{};
}

EDivineBeastsPresentationContentPackState
UDivineBeastsPresentationClientSubsystem::GetAudioLibraryCategoryState(FName Category) const
{
    if (const auto* Handle = AudioLibraryCategories.Find(Category))
    {
        FString Error;
        return GetContentPackState(*Handle, Error);
    }
    return EDivineBeastsPresentationContentPackState::Invalid;
}

bool UDivineBeastsPresentationClientSubsystem::ResolveAudioLibraryClip(
    FName ClipId, FGameplayTag& OutSemantic) const
{
    OutSemantic = FGameplayTag();
    const FName Category = CategoryForAudioClip(ClipId);
    if (Category.IsNone() || !AudioLibraryCategories.Contains(Category) ||
        GetAudioLibraryCategoryState(Category) != EDivineBeastsPresentationContentPackState::Active)
        return false;
    const auto Catalog = FDivineBeastsPresentationProjectCatalog::BuildAudioLibraryFragment(Category);
    const FName DesiredTag(*(FString(TEXT("Presentation.SSLM.AudioLibrary.")) + ClipId.ToString()));
    for (const auto& Entry : Catalog.Entries)
        if (Entry.SemanticTag.GetTagName() == DesiredTag && Entry.ProviderChannel == TEXT("SFX"))
        {
            OutSemantic = Entry.SemanticTag;
            return OutSemantic.IsValid();
        }
    return false;
}

bool UDivineBeastsPresentationClientSubsystem::PlayAudioLibraryClip(FName ClipId, FVector Location)
{
    return PlayAudioLibraryFactClip(ClipId, Location, FGuid::NewGuid());
}

bool UDivineBeastsPresentationClientSubsystem::PlayAudioLibraryFootstepClip(
    FName ClipId, FVector Location, USkeletalMeshComponent* MeshComp, FName FootSocketName)
{
    check(IsInGameThread());
    UWorld* World = AudioLibraryWorld.Get();
    if (bClosing || !World || World->bIsTearingDown || !IsValid(MeshComp) ||
        MeshComp->GetWorld() != World || Location.ContainsNaN() ||
        !ClipId.ToString().StartsWith(TEXT("Footstep."), ESearchCase::CaseSensitive))
        return false;

    // UAnimNotify实例可被多角色共享，因此去重必须归LocalPlayerSubsystem维护。
    // 每只脚独立限频，不影响左右脚交替；只记录已提交成功的播放请求。
    constexpr double MinSameFootIntervalSeconds = 0.085;
    const double Now = static_cast<double>(World->GetTimeSeconds());
    const TWeakObjectPtr<USkeletalMeshComponent> MeshKey(MeshComp);
    if (const TMap<FName, double>* Times = RecentFootsteps.Find(MeshKey))
        if (const double* Last = Times->Find(FootSocketName))
            if (Now >= *Last && Now - *Last < MinSameFootIntervalSeconds)
                return false;

    if (!PlayAudioLibraryClip(ClipId, Location)) return false;
    RecentFootsteps.FindOrAdd(MeshKey).Add(FootSocketName, Now);
    // 异常大量瞬态Mesh时仍有硬上限；正常10人竞技场不会触发整表淘汰。
    if (RecentFootsteps.Num() > 256)
    {
        for (auto It = RecentFootsteps.CreateIterator(); It; ++It)
            if (!It.Key().IsValid()) It.RemoveCurrent();
        if (RecentFootsteps.Num() > 256) RecentFootsteps.Reset();
    }
    return true;
}

bool UDivineBeastsPresentationClientSubsystem::PlayAudioLibraryFactClip(
    FName ClipId, FVector Location, const FGuid& OccurrenceId)
{
    check(IsInGameThread());
    UWorld* World = AudioLibraryWorld.Get();
    if (bClosing || !OccurrenceId.IsValid() || !World || World->bIsTearingDown ||
        !GetLocalPlayer() || GetLocalPlayer()->GetWorld() != World ||
        !PlatformPresentation || Location.ContainsNaN()) return false;
    FGameplayTag Tag;
    if (!ResolveAudioLibraryClip(ClipId, Tag)) return false;
    FGamePlatformPresentationRequest Request;
    Request.RequestId = OccurrenceId;
    Request.SemanticTag = Tag;
    Request.ContextId = ClipId;
    Request.Context.ProjectId = FDivineBeastsProjectCatalog::GetProjectId();
    Request.SourceLocation = Location;
    Request.ImpactLocation = Location;
    Request.WorldGeneration = PlatformPresentation->GetWorldGeneration();
    return SubmitProjectRequest(Request) == EGamePlatformPresentationSubmitResult::Submitted;
}

bool UDivineBeastsPresentationClientSubsystem::StartAudioLibraryLoop(
    FName AnchorId, FName ClipId, FVector Location)
{
    check(IsInGameThread());
    UWorld* World = AudioLibraryWorld.Get();
    if (AnchorId.IsNone() || !World || World->bIsTearingDown || Location.ContainsNaN() ||
        bClosing || !PlatformPresentation || !GetLocalPlayer() || GetLocalPlayer()->GetWorld() != World)
        return false;
    // 只能循环清单中真正声明Looping的两条水流；禁止把任意鸟鸣/脚步/技能变成无限循环。
    if (ClipId != TEXT("Ambience.WaterStreamA") && ClipId != TEXT("Ambience.WaterStreamB"))
        return false;
    FGameplayTag Tag;
    if (!ResolveAudioLibraryClip(ClipId, Tag)) return false;
    if (const auto* Old = AudioLibraryLoops.Find(AnchorId))
        if (Old->ClipId == ClipId && Old->Location.Equals(Location, 1.0))
            return true;
    StopAudioLibraryLoop(AnchorId);
    const FGuid RequestId = FGuid::NewGuid();
    FGamePlatformPresentationRequest Request;
    Request.RequestId = RequestId;
    Request.SemanticTag = Tag;
    Request.ContextId = AnchorId;
    Request.Context.ProjectId = FDivineBeastsProjectCatalog::GetProjectId();
    Request.SourceLocation = Location;
    Request.ImpactLocation = Location;
    Request.Lifetime = EGamePlatformPresentationLifetime::Persistent;
    Request.Priority = EGamePlatformPresentationPriority::Low;
    Request.WorldGeneration = PlatformPresentation->GetWorldGeneration();
    const auto Result = SubmitProjectRequest(Request);
    if (Result != EGamePlatformPresentationSubmitResult::Submitted) return false;
    if (bClosing || World != AudioLibraryWorld.Get() || World->bIsTearingDown)
    {
        Request.PredictionState = EGamePlatformPresentationPredictionState::Cancelled;
        SubmitProjectRequest(Request);
        return false;
    }
    FAudioLibraryLoop Owned;
    Owned.RequestId = RequestId;
    Owned.ClipId = ClipId;
    Owned.Location = Location;
    Owned.SemanticTag = Tag;
    AudioLibraryLoops.Add(AnchorId, MoveTemp(Owned));
    return true;
}

bool UDivineBeastsPresentationClientSubsystem::StopAudioLibraryLoop(FName AnchorId)
{
    check(IsInGameThread());
    FAudioLibraryLoop Old;
    if (!AudioLibraryLoops.RemoveAndCopyValue(AnchorId, Old)) return false;
    if (Old.RequestId.IsValid() && Old.SemanticTag.IsValid() && PlatformPresentation)
    {
        UWorld* World = AudioLibraryWorld.Get();
        if (World && !World->bIsTearingDown)
        {
            FGamePlatformPresentationRequest Cancel;
            Cancel.RequestId = Old.RequestId;
            Cancel.SemanticTag = Old.SemanticTag;
            Cancel.Context.ProjectId = FDivineBeastsProjectCatalog::GetProjectId();
            Cancel.PredictionState = EGamePlatformPresentationPredictionState::Cancelled;
            Cancel.WorldGeneration = PlatformPresentation->GetWorldGeneration();
            SubmitProjectRequest(Cancel);
        }
    }
    return true;
}

void UDivineBeastsPresentationClientSubsystem::StopAudioLibraryForWorld()
{
    check(IsInGameThread());
    TArray<FName> LoopKeys;
    AudioLibraryLoops.GetKeys(LoopKeys);
    for (FName Key : LoopKeys) StopAudioLibraryLoop(Key);
    AudioLibraryLoops.Reset();
    RecentFootsteps.Reset(); // 切图、角色退出和账号切换不保留旧世界脚步门闩。
    auto Old = MoveTemp(AudioLibraryCategories);
    AudioLibraryCategories.Reset();
    AudioLibraryWorld.Reset();
    for (const auto& Pair : Old)
        if (Pair.Value.IsValid()) DeactivateContentPack(Pair.Value);
}
