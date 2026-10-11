// 项目层头像消费已复制英雄身份及选中角色摘要；不保存业务权威、不编造等级、无需Tick和HTTP。
#include "Components/DivineBeastsPlayerPortraitWidget.h"
#include "Catalog/DivineBeastsHeroCatalog.h"
#include "Components/DivineBeastsCharacterComponent.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "DivineBeastsApplicationFlowSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/StreamableManager.h"
#include "GameFramework/Pawn.h"
#include "Loading/GamePlatformAssetLoader.h"
#include "Localization/DivineBeastsUILocalization.h"

FSoftObjectPath UDivineBeastsPlayerPortraitWidget::ResolvePortraitPath(FName HeroDefinitionId)
{
    if (!FDivineBeastsHeroCatalog::IsCoreHeroId(HeroDefinitionId)) { return FSoftObjectPath(); }
    FString Prefix, Suffix;
    HeroDefinitionId.ToString().Split(TEXT("."), &Prefix, &Suffix, ESearchCase::CaseSensitive, ESearchDir::FromEnd);
    return FSoftObjectPath(FString::Printf(TEXT("/DBAHeroPack_%s/UI/Portraits/T_DBA_%s_Portrait.T_DBA_%s_Portrait"),
        *Suffix, *Suffix, *Suffix));
}

void UDivineBeastsPlayerPortraitWidget::NativeConstruct()
{
    Super::NativeConstruct();
    if (UGameInstance* Instance = GetGameInstance())
    {
        if (auto* Flow = Instance->GetSubsystem<UDivineBeastsApplicationFlowSubsystem>())
        {
            BoundFlow = Flow;
            FlowHandle = Flow->OnViewStateChanged().AddUObject(this, &ThisClass::HandleFlowChanged);
        }
    }
    BindToPawn(GetOwningPlayerPawn());
}

void UDivineBeastsPlayerPortraitWidget::NativeDestruct() { ClearAllBindings(); Super::NativeDestruct(); }
void UDivineBeastsPlayerPortraitWidget::BeginDestroy()
{
    // 蓝图重编译会直接GC旧实例，未必经过NativeDestruct；此时只能原生清理，不能ProcessEvent或触碰Widget树。
    bDestroyingNativeResources = true;
    ClearAllBindings(false);
    Super::BeginDestroy();
}

void UDivineBeastsPlayerPortraitWidget::ClearAllBindings(bool bPublishEmptySnapshot)
{
    if (auto* Flow = BoundFlow.Get()) { Flow->OnViewStateChanged().Remove(FlowHandle); }
    BoundFlow.Reset();
    FlowHandle.Reset();
    ClearPawnBinding(bPublishEmptySnapshot);
}

void UDivineBeastsPlayerPortraitWidget::ClearPawnBinding(bool bPublishEmptySnapshot)
{
    ++LoadGeneration;
    if (auto* Identity = BoundIdentity.Get())
    {
        Identity->OnReadinessChanged().Remove(ReadinessHandle);
        Identity->OnIdentityChanged().Remove(IdentityHandle);
    }
    if (APawn* Pawn = BoundPawn.Get()) { Pawn->OnDestroyed.RemoveDynamic(this, &ThisClass::HandlePawnDestroyed); }
    BoundIdentity.Reset();
    BoundPawn.Reset();
    ReadinessHandle.Reset();
    IdentityHandle.Reset();
    FGamePlatformAssetLoader::Cancel(PortraitLoadHandle);
    PortraitLoadHandle.Reset();
    RequestedPortraitPath.Reset();
    // 不可达对象的反射事件会触发引擎断言；必须在资源已释放之后退出，而不是把整个清理路径跳过。
    if (!bPublishEmptySnapshot || bDestroyingNativeResources || !IsValid(this) || IsUnreachable() ||
        HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed)) { return; }
    const uint64 ExpectedClearGeneration = LoadGeneration;
    ApplyPortraitState(FGamePlatformUIPortraitState());
    // 空快照通知也允许同步重入；新绑定已取得所有权时，旧清理不得再次隐藏它。
    if (ExpectedClearGeneration != LoadGeneration || bDestroyingNativeResources || !IsValid(this) || IsUnreachable()) { return; }
    RenderPortrait();
    SetVisibility(ESlateVisibility::Collapsed);
}

void UDivineBeastsPlayerPortraitWidget::BindToPawn(APawn* Pawn)
{
    if (bDestroyingNativeResources || !IsValid(this) || IsUnreachable()) { return; }
    if (BoundPawn.Get() == Pawn && IsValid(Pawn)) { RefreshPortrait(); return; }
    const uint64 ExpectedClearGeneration = LoadGeneration + 1;
    ClearPawnBinding();
    // 清空旧显示期间可能由蓝图重新绑定；继续旧调用会覆盖新身份和订阅，故必须放弃旧请求。
    if (ExpectedClearGeneration != LoadGeneration || bDestroyingNativeResources || !IsValid(this) || IsUnreachable()) { return; }
    if (!IsValid(Pawn) || !Pawn->IsLocallyControlled()) { return; }
    UDivineBeastsCharacterComponent* Identity = Pawn->FindComponentByClass<UDivineBeastsCharacterComponent>();
    if (!IsValid(Identity)) { return; }
    BoundPawn = Pawn;
    BoundIdentity = Identity;
    ReadinessHandle = Identity->OnReadinessChanged().AddUObject(this, &ThisClass::HandleReadinessChanged);
    IdentityHandle = Identity->OnIdentityChanged().AddUObject(this, &ThisClass::HandleIdentityChanged);
    Pawn->OnDestroyed.AddUniqueDynamic(this, &ThisClass::HandlePawnDestroyed);
    RefreshPortrait();
}

void UDivineBeastsPlayerPortraitWidget::HandleReadinessChanged(bool) { RefreshPortrait(); }
void UDivineBeastsPlayerPortraitWidget::HandleIdentityChanged() { RefreshPortrait(); }
void UDivineBeastsPlayerPortraitWidget::HandleFlowChanged(const FDivineBeastsFlowViewState&) { RefreshPortrait(); }
void UDivineBeastsPlayerPortraitWidget::HandlePawnDestroyed(AActor*) { ClearPawnBinding(); }

void UDivineBeastsPlayerPortraitWidget::RefreshPortrait()
{
    if (bDestroyingNativeResources || !IsValid(this) || IsUnreachable()) { return; }
    auto* Identity = BoundIdentity.Get();
    if (!IsValid(Identity) || BoundPawn.Get() != GetOwningPlayerPawn()) { return; }
    const FName HeroId = Identity->GetHeroDefinitionId();
    const FSoftObjectPath Path = ResolvePortraitPath(HeroId);
    FGamePlatformUIPortraitState Snapshot;
    Snapshot.DisplayId = HeroId;
    Snapshot.DisplayName = FDivineBeastsUILocalization::HeroNameToText(HeroId);
    // 流程中的角色名只有在持久身份及英雄均一致时才显示，旅行期间不得错配上一角色的名称。
    if (auto* Flow = BoundFlow.Get())
    {
        const auto State = Flow->GetViewState();
        if (State.bHasSelectedCharacter && State.SelectedCharacter.CharacterId == Identity->GetCharacterId() &&
            State.SelectedCharacter.HeroDefinitionId == HeroId && !State.SelectedCharacter.CharacterName.IsEmpty())
        { Snapshot.DisplayName = FText::FromString(State.SelectedCharacter.CharacterName); }
    }
    Snapshot.Level = INDEX_NONE; // 当前持久摘要与组件均没有等级字段，禁止以固定1冒充权威等级。
    Snapshot.StatusId = Identity->IsCharacterReady() ? FName(TEXT("Ready")) : FName(TEXT("Loading"));
    Snapshot.PortraitTexture = TSoftObjectPtr<UTexture2D>(Path);
    const uint64 ExpectedIdentityGeneration = LoadGeneration;
    ApplyPortraitState(Snapshot);
    // 同步蓝图通知可能解绑或切换角色；外层旧投影不得再申请原头像或覆盖新名称。
    if (bDestroyingNativeResources || !IsValid(this) || IsUnreachable() ||
        ExpectedIdentityGeneration != LoadGeneration || BoundIdentity.Get() != Identity ||
        !IsValid(Identity) || Identity->GetHeroDefinitionId() != HeroId || BoundPawn.Get() != GetOwningPlayerPawn()) { return; }
    if (Path != RequestedPortraitPath)
    {
        ++LoadGeneration;
        FGamePlatformAssetLoader::Cancel(PortraitLoadHandle);
        PortraitLoadHandle.Reset();
        RequestedPortraitPath = Path;
        if (Path.IsValid())
        {
            const uint64 ExpectedGeneration = LoadGeneration;
            TWeakObjectPtr<UDivineBeastsPlayerPortraitWidget> WeakThis(this);
            PortraitLoadHandle = FGamePlatformAssetLoader::RequestAsyncLoad({Path},
                FStreamableDelegate::CreateLambda([WeakThis, ExpectedGeneration]()
                {
                    if (auto* Widget = WeakThis.Get(); Widget && Widget->LoadGeneration == ExpectedGeneration &&
                        Widget->BoundIdentity.IsValid() && Widget->BoundPawn.Get() == Widget->GetOwningPlayerPawn())
                    { Widget->RenderPortrait(); }
                }));
        }
    }
    RenderPortrait();
    SetVisibility(HeroId.IsNone() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
}

void UDivineBeastsPlayerPortraitWidget::RenderPortrait()
{
    if (bDestroyingNativeResources || !IsValid(this) || IsUnreachable()) { return; }
    const auto State = GetPortraitState();
    if (auto* Image = Cast<UImage>(GetWidgetFromName(TEXT("PortraitImage"))))
    {
        UTexture2D* Texture = State.PortraitTexture.Get();
        Image->SetBrushFromTexture(Texture);
        Image->SetVisibility(IsValid(Texture) ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Hidden);
    }
    // 图腾为英雄身份的装饰投影，复用真实肖像；不保留原稿白马，也不新增印记玩法或权威状态。
    if (auto* Image = Cast<UImage>(GetWidgetFromName(TEXT("HeroTotemImage"))))
    {
        UTexture2D* Texture = State.PortraitTexture.Get();
        Image->SetBrushFromTexture(Texture);
        Image->SetVisibility(IsValid(Texture) ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
    }
    if (auto* Name = Cast<UTextBlock>(GetWidgetFromName(TEXT("PlayerName")))) { Name->SetText(State.DisplayName); }
    if (auto* Level = Cast<UTextBlock>(GetWidgetFromName(TEXT("LevelText"))))
    {
        Level->SetText(State.Level >= 0 ? FText::AsNumber(State.Level) : FText::GetEmpty());
        Level->SetVisibility(State.Level >= 0 ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
    }
    // 无可信等级时数字留空、可选控件显示未知标记；原稿60不能成为默认值，也不把就绪当成1级。
    if (auto* Unavailable = Cast<UTextBlock>(GetWidgetFromName(TEXT("LevelUnavailableText"))))
    { Unavailable->SetVisibility(State.Level < 0 && !State.DisplayId.IsNone() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed); }
}
