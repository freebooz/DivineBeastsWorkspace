// 项目客户端创建介绍VM：从真实HeroDefinition和独立CreationProfile复制只读文本。
// 调用方为创建页；所有资源由GamePlatformData中央租约持有，无HTTP/GAS授权/业务Tick。
// 页面与请求双代次拒绝迟到结果；失活/切英雄/销毁仅释放本用途，外部事件允许同步重入。
#include "ViewModels/Characters/DivineBeastsCharacterCreateViewModel.h"
#include "Definitions/DivineBeastsCharacterCreationProfile.h"
#include "Definitions/DivineBeastsHeroDefinition.h"
#include "Catalog/DivineBeastsHeroCatalog.h"
#include "Settings/DivineBeastsUISettings.h"
#include "Interfaces/IGamePlatformDataService.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"

/** 私有单用途租约上下文；只保存签发句柄和弱实例，不暴露Data私有子系统或引擎加载句柄。 */
struct FDivineBeastsHeroOverviewLeaseState
{
    TWeakObjectPtr<UGameInstance> Instance;
    FGamePlatformDataLease Lease;
};

UDivineBeastsCharacterCreateViewModel::UDivineBeastsCharacterCreateViewModel()
    : OverviewLeaseState(MakeShared<FDivineBeastsHeroOverviewLeaseState>()) {}
UDivineBeastsCharacterCreateViewModel::~UDivineBeastsCharacterCreateViewModel() = default;

void UDivineBeastsCharacterCreateViewModel::OnPageBegan()
{
    Super::OnPageBegan();
    ResetHeroOverview(false);
}

void UDivineBeastsCharacterCreateViewModel::OnPageEnded()
{
    ClearHeroOverview();
    Super::OnPageEnded();
}

void UDivineBeastsCharacterCreateViewModel::BeginDestroy()
{
    ResetHeroOverview(false);
    HeroOverviewChanged.Clear();
    Super::BeginDestroy();
}

void UDivineBeastsCharacterCreateViewModel::BroadcastHeroOverview()
{
    // 复制事件值防观察者同步切英雄后，后续观察者读取被原地改写的容器。
    const FDivineBeastsHeroOverview Snapshot = HeroOverview;
    HeroOverviewChanged.Broadcast(Snapshot);
}

void UDivineBeastsCharacterCreateViewModel::ReleaseOverviewLease()
{
    // 先撤本地所有权再调用中央释放；取消通知可能重入，旧栈不能释放后继请求。
    const FGamePlatformDataLease Previous = OverviewLeaseState->Lease;
    const TWeakObjectPtr<UGameInstance> PreviousInstance = OverviewLeaseState->Instance;
    OverviewLeaseState->Lease = {};
    OverviewLeaseState->Instance.Reset();
    if (Previous.IsValid() && PreviousInstance.IsValid())
        if (auto* Data = IGamePlatformDataService::Get(*PreviousInstance.Get())) Data->ReleaseResources(Previous);
}

void UDivineBeastsCharacterCreateViewModel::ResetHeroOverview(bool bNotify)
{
    check(IsInGameThread());
    ++OverviewRequestGeneration;
    bOverviewCompletionPublished = false;
    HeroOverview = {};
    HeroOverview.PageGeneration = GetPageGeneration();
    HeroOverview.RequestGeneration = OverviewRequestGeneration;
    ReleaseOverviewLease();
    if (bNotify) BroadcastHeroOverview();
}

void UDivineBeastsCharacterCreateViewModel::ClearHeroOverview()
{
    ResetHeroOverview(true);
}

bool UDivineBeastsCharacterCreateViewModel::IsOverviewRequestCurrent(
    int32 ExpectedPageGeneration, int64 ExpectedRequestGeneration, FName ExpectedHero) const
{
    return IsPageActive() && GetPageGeneration() == ExpectedPageGeneration &&
        OverviewRequestGeneration == ExpectedRequestGeneration && HeroOverview.HeroDefinitionId == ExpectedHero;
}

void UDivineBeastsCharacterCreateViewModel::PublishOverviewFailure(
    int32 ExpectedPageGeneration, int64 ExpectedRequestGeneration, FName ExpectedHero,
    FName ErrorCode, const FText& Message)
{
    if (!IsOverviewRequestCurrent(ExpectedPageGeneration, ExpectedRequestGeneration, ExpectedHero) || bOverviewCompletionPublished) return;
    bOverviewCompletionPublished = true;
    ReleaseOverviewLease();
    HeroOverview.Introduction = FText::GetEmpty();
    HeroOverview.Abilities.Reset();
    HeroOverview.SkillIntroductionText = FText::GetEmpty();
    HeroOverview.bDescriptionsAreDraft = false;
    HeroOverview.PageState = EDivineBeastsUIPageState::Error;
    HeroOverview.ErrorCode = ErrorCode;
    HeroOverview.StatusText = Message;
    BroadcastHeroOverview();
}

bool UDivineBeastsCharacterCreateViewModel::SelectHeroOverview(FName HeroDefinitionId)
{
    check(IsInGameThread());
    if (!IsPageActive() || !FDivineBeastsHeroCatalog::IsCoreHeroId(HeroDefinitionId)) return false;
    if (HeroOverview.HeroDefinitionId == HeroDefinitionId && HeroOverview.PageGeneration == GetPageGeneration() &&
        (HeroOverview.PageState == EDivineBeastsUIPageState::Loading || HeroOverview.PageState == EDivineBeastsUIPageState::Ready)) return true;

    ResetHeroOverview(false);
    const int32 ExpectedPageGeneration = GetPageGeneration();
    const int64 RequestGeneration = OverviewRequestGeneration;
    HeroOverview.HeroDefinitionId = HeroDefinitionId;
    HeroOverview.PageState = EDivineBeastsUIPageState::Loading;
    HeroOverview.StatusText = NSLOCTEXT("DivineBeastsCharacterCreation", "IntroductionLoading", "正在加载角色与技能介绍…");
    BroadcastHeroOverview();
    // Loading观察者可能同步失活或选择另一英雄；后继意图优先，旧调用不再申请资源。
    if (!IsOverviewRequestCurrent(ExpectedPageGeneration, RequestGeneration, HeroDefinitionId)) return false;

    // 生产VM归LocalPlayer子系统，LocalPlayer本身归Engine而非GI；不得从Outer链假定存在GI。
    const ULocalPlayer* LocalPlayer = GetTypedOuter<ULocalPlayer>();
    UGameInstance* Instance = LocalPlayer ? LocalPlayer->GetGameInstance() : GetTypedOuter<UGameInstance>();
    auto* Data = Instance ? IGamePlatformDataService::Get(*Instance) : nullptr;
    if (!Data)
    {
        PublishOverviewFailure(ExpectedPageGeneration, RequestGeneration, HeroDefinitionId, TEXT("HeroOverviewDataUnavailable"),
            NSLOCTEXT("DivineBeastsCharacterCreation", "DataUnavailable", "介绍数据服务尚未就绪，请稍后重试。"));
        return false;
    }
    FSoftObjectPath ProfilePath;
    FString Error;
    if (!GetDefault<UDivineBeastsUISettings>()->TryGetCharacterCreationProfilePath(HeroDefinitionId, ProfilePath, Error))
    {
        PublishOverviewFailure(ExpectedPageGeneration, RequestGeneration, HeroDefinitionId, TEXT("CreationProfileMappingInvalid"), FText::FromString(Error));
        return false;
    }
    const FSoftObjectPath HeroPath = FDivineBeastsHeroCatalog::GetDefinitionAssetPath(HeroDefinitionId);
    if (!HeroPath.IsValid())
    {
        PublishOverviewFailure(ExpectedPageGeneration, RequestGeneration, HeroDefinitionId, TEXT("HeroDefinitionMissing"),
            NSLOCTEXT("DivineBeastsCharacterCreation", "HeroMissing", "角色介绍定义尚未注册，不能显示未经配置的内容。"));
        return false;
    }
    // 两份作者数据共用一个普通资源租约，全部加载成功后才原子发布；不读取开发UIProfile或AbilitySet。
    OverviewLeaseState->Instance = Instance;
    const TWeakObjectPtr<UDivineBeastsCharacterCreateViewModel> WeakThis(this);
    FGamePlatformResult Accepted;
    // 显式GI是Data支持的合法实例调用者，避免为资源作用域依赖UObject.GetWorld转发。
    // 完成另以弱VM/双代次门禁，VM的EndPage/BeginDestroy必须主动释放本用途，不能依赖GI寿命兜底。
    const FGamePlatformDataLease Lease = Data->AcquireResources({HeroPath, ProfilePath}, EGamePlatformDataLifetime::Instance, Instance,
        [WeakThis, ExpectedPageGeneration, RequestGeneration, HeroDefinitionId, HeroPath, ProfilePath](const auto& FinishedLease, const auto& Result)
        {
            if (auto* Self = WeakThis.Get()) Self->HandleOverviewResourcesLoaded(ExpectedPageGeneration, RequestGeneration,
                HeroDefinitionId, HeroPath, ProfilePath, FinishedLease, Result);
        }, Accepted);
    OverviewLeaseState->Lease = Lease;
    if (!Accepted.IsSuccess() || !Lease.IsValid())
    {
        // Data同步拒绝仍可能延后通知；完成门闩阻止失败状态重复广播。
        PublishOverviewFailure(ExpectedPageGeneration, RequestGeneration, HeroDefinitionId, TEXT("HeroOverviewResourcesRejected"),
            NSLOCTEXT("DivineBeastsCharacterCreation", "ResourcesRejected", "介绍资源请求未受理，请检查资源映射和实例作用域。"));
        return false;
    }
    return true;
}

void UDivineBeastsCharacterCreateViewModel::HandleOverviewResourcesLoaded(
    int32 ExpectedPageGeneration, int64 ExpectedRequestGeneration, FName ExpectedHero,
    const FSoftObjectPath& HeroPath, const FSoftObjectPath& ProfilePath,
    const FGamePlatformDataLease& Lease, const FGamePlatformResult& Result)
{
    check(IsInGameThread());
    if (!IsOverviewRequestCurrent(ExpectedPageGeneration, ExpectedRequestGeneration, ExpectedHero) || bOverviewCompletionPublished) return;
    if (!Result.IsSuccess())
    {
        PublishOverviewFailure(ExpectedPageGeneration, ExpectedRequestGeneration, ExpectedHero, TEXT("HeroOverviewResourceLoadFailed"),
            NSLOCTEXT("DivineBeastsCharacterCreation", "LoadFailed", "角色或技能介绍资源加载失败，请稍后重试。"));
        return;
    }
    auto* Data = OverviewLeaseState->Instance.IsValid() ? IGamePlatformDataService::Get(*OverviewLeaseState->Instance.Get()) : nullptr;
    if (!Data || Lease.ScopeId != OverviewLeaseState->Lease.ScopeId || Lease.LeaseId != OverviewLeaseState->Lease.LeaseId ||
        Lease.Generation != OverviewLeaseState->Lease.Generation || Data->GetLeaseState(Lease) != EGamePlatformDataRequestState::Succeeded)
    {
        PublishOverviewFailure(ExpectedPageGeneration, ExpectedRequestGeneration, ExpectedHero, TEXT("HeroOverviewLeaseInvalid"),
            NSLOCTEXT("DivineBeastsCharacterCreation", "LeaseInvalid", "介绍资源用途已结束，请重新选择英雄。"));
        return;
    }
    const auto* Hero = Cast<UDivineBeastsHeroDefinition>(HeroPath.ResolveObject());
    const auto* Profile = Cast<UDivineBeastsCharacterCreationProfile>(ProfilePath.ResolveObject());
    FString ValidationError;
    if (!Hero || Hero->DefinitionId != ExpectedHero || Hero->Introduction.ToString().TrimStartAndEnd().IsEmpty() ||
        !Profile || Profile->HeroDefinitionId != ExpectedHero ||
        Profile->GetPrimaryAssetId() != FPrimaryAssetId(TEXT("DivineBeastsCharacterCreationProfile"), ExpectedHero) ||
        !Profile->ValidateCreationProfile(ValidationError))
    {
        const FText ErrorText = ValidationError.IsEmpty()
            ? NSLOCTEXT("DivineBeastsCharacterCreation", "ContentIncomplete", "角色或技能介绍尚未完整配置，不能显示其他英雄或缺失正文。")
            : FText::FromString(ValidationError);
        PublishOverviewFailure(ExpectedPageGeneration, ExpectedRequestGeneration, ExpectedHero, TEXT("HeroOverviewContentInvalid"), ErrorText);
        return;
    }
    HeroOverview.Introduction = Hero->Introduction;
    HeroOverview.bDescriptionsAreDraft = Profile->bDescriptionsAreDraft;
    TArray<FText> Paragraphs;
    for (const FDivineBeastsAbilityUIEntry& Entry : Profile->Entries)
    {
        FDivineBeastsHeroOverviewAbility Ability;
        Ability.AbilityId = Entry.AbilityId;
        Ability.DisplayName = Entry.DisplayName;
        Ability.Description = Entry.Description;
        HeroOverview.Abilities.Add(MoveTemp(Ability));
        Paragraphs.Add(FText::Format(NSLOCTEXT("DivineBeastsCharacterCreation", "SkillParagraph", "{0}\n{1}"), Entry.DisplayName, Entry.Description));
    }
    HeroOverview.SkillIntroductionText = FText::Join(FText::FromString(TEXT("\n\n")), Paragraphs);
    HeroOverview.StatusText = Profile->bDescriptionsAreDraft
        ? NSLOCTEXT("DivineBeastsCharacterCreation", "DraftNotice", "开发候选，未作为正式技能批准") : FText::GetEmpty();
    HeroOverview.PageState = EDivineBeastsUIPageState::Ready;
    HeroOverview.ErrorCode = NAME_None;
    bOverviewCompletionPublished = true;
    // 发布后的观察者可立即切英雄/关闭页；函数在广播后不再访问作者对象或改写新请求。
    BroadcastHeroOverview();
}
