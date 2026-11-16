// SSLM内容预览只拥有本次样板和租约；不注册玩法、不授予技能、不修改原资产。
#include "Scenarios/DivineBeastsDemoContentAssetScenario.h"
#include "Demo/DivineBeastsDemoWorldSubsystem.h"
#include "UI/DivineBeastsDemoMenuItem.h"
#include "Interfaces/IGamePlatformDataService.h"
#include "Interfaces/IPluginManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/AssetData.h"
#include "Modules/ModuleManager.h"
#include "Blueprint/UserWidget.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/ListView.h"
#include "Components/Viewport.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/SkeletalMeshActor.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/Package.h"
#if WITH_EDITOR
#include "Editor.h"
#include "Subsystems/AssetEditorSubsystem.h"
#endif

namespace DivineBeasts::Demo
{
FGamePlatformResult ValidateContentAssetSelection(FName OwnerPlugin, const FSoftObjectPath& Asset)
{
    const FString Package = Asset.GetLongPackageName();
    if (OwnerPlugin.IsNone() || !Asset.IsValid() || !Asset.GetSubPathString().IsEmpty() ||
        !FPackageName::IsValidLongPackageName(Package) || !Package.StartsWith(TEXT("/") + OwnerPlugin.ToString() + TEXT("/")) ||
        Asset.GetAssetName() != FPackageName::GetShortName(Package))
        return FGamePlatformResult::Failure(TEXT("Demo.ContentAssetOwner"), TEXT("只允许当前内容包内真实顶层资源；不接受跨包、子对象、类别名或磁盘路径。"));
    return FGamePlatformResult::Success();
}

/** 弱World/Widget不延长页面生命；中央Data只保留本次选中的一个资产和固定球样板资源。 */
struct FContentAssetState : TSharedFromThis<FContentAssetState>
{
    FDivineBeastsDemoScenarioContext Context;
    TWeakObjectPtr<UGameInstance> Instance;
    FName OwnerPlugin;
    FGamePlatformDataLease Lease;
    TWeakObjectPtr<AActor> PreviewActor;
    FDivineBeastsDemoOperationCompletion Pending;
    uint64 Generation = 0;
    bool bPrepared = false;
    bool bWaiting = false;
    bool bCleaning = false;
#if WITH_EDITOR
    TWeakObjectPtr<UObject> OwnedEditorAsset;
    IAssetEditorInstance* OwnedEditor = nullptr; // 仅作身份比较，不解引用越过Editor生命周期。
    TWeakObjectPtr<UAssetEditorSubsystem> EditorEvents;
    FDelegateHandle EditorClosedHandle;
    void ForgetEditor()
    {
        if (auto* Editors = EditorEvents.Get()) if (EditorClosedHandle.IsValid()) Editors->OnAssetClosedInEditor().Remove(EditorClosedHandle);
        EditorClosedHandle.Reset(); EditorEvents.Reset(); OwnedEditorAsset.Reset(); OwnedEditor = nullptr;
    }
    void TrackEditor(UAssetEditorSubsystem& Editors, UObject& Asset, IAssetEditorInstance* Editor)
    {
        ForgetEditor(); OwnedEditorAsset = &Asset; OwnedEditor = Editor; EditorEvents = &Editors;
        TWeakPtr<FContentAssetState> Weak = AsShared();
        EditorClosedHandle = Editors.OnAssetClosedInEditor().AddLambda([Weak](UObject* ClosedAsset, IAssetEditorInstance* ClosedEditor)
        {
            if (auto Self = Weak.Pin()) if (Self->OwnedEditor == ClosedEditor && Self->OwnedEditorAsset.Get() == ClosedAsset)
            { Self->OwnedEditor = nullptr; Self->OwnedEditorAsset.Reset(); }
        });
    }
    FGamePlatformResult CloseOwnedEditor()
    {
        if (!OwnedEditor) { ForgetEditor(); return FGamePlatformResult::Success(); }
        auto* Editors = EditorEvents.Get();
        if (!Editors) return FGamePlatformResult::Failure(TEXT("Demo.ContentEditorUnavailable"), TEXT("原生预览服务失效，未确认本次窗口清理。"));
        UObject* Asset = OwnedEditorAsset.Get();
        if (!Asset || !Editors->GetAllOpenEditors().Contains(OwnedEditor) || !Editors->FindEditorsForAsset(Asset).Contains(OwnedEditor))
        { ForgetEditor(); return FGamePlatformResult::Success(); }
        if (Asset->GetPackage()->IsDirty())
            return FGamePlatformResult::Failure(TEXT("Demo.ContentEditorDirty"), TEXT("预览资源出现未保存编辑；保留窗口，请先人工处理再停止。"));
        for (UObject* Other : Editors->GetAllEditedAssets())
            if (Other && Other != Asset && !Other->IsIn(Asset) && Editors->FindEditorsForAsset(Other).Contains(OwnedEditor))
                return FGamePlatformResult::Failure(TEXT("Demo.ContentEditorShared"), TEXT("本次窗口已载入其他资产；请人工关闭该预览，再重试停止，不关闭其他工作。"));
        IAssetEditorInstance* Closing = OwnedEditor;
        const bool bClosed = Closing->CloseWindow(EAssetEditorCloseReason::AssetEditorHostClosed);
        if (!bClosed || (Editors->GetAllOpenEditors().Contains(Closing) && Editors->FindEditorsForAsset(Asset).Contains(Closing)))
            return FGamePlatformResult::Failure(TEXT("Demo.ContentEditorCleanup"), TEXT("本次确切预览窗口拒绝关闭，保留重试身份。"));
        ForgetEditor(); return FGamePlatformResult::Success();
    }
#endif
    IGamePlatformDataService* Data() const { return Instance.IsValid() ? IGamePlatformDataService::Get(*Instance) : nullptr; }
    UUserWidget* Widget() const { return Cast<UUserWidget>(Context.SampleWidget.Get()); }
    void Status(const FString& Text)
    {
        if (auto* View = Widget()) if (auto* Label = Cast<UTextBlock>(View->GetWidgetFromName(TEXT("AssetPreviewStatusText"))))
            Label->SetText(FText::FromString(Text));
        if (bPrepared && Context.PublishObservation) Context.PublishObservation(Context.Token, Text);
    }
    void Finish(const FGamePlatformResult& Result)
    {
        if (!bWaiting) return;
        bWaiting = false;
        auto Completion = MoveTemp(Pending);
        Status(Result.Message);
        if (Completion) Completion(Context.Token, Result);
    }
    FGamePlatformResult Clear()
    {
        if (bCleaning) return FGamePlatformResult::Failure(TEXT("Demo.ContentCleanupReentry"), TEXT("本次资源仍在清理。"));
        TGuardValue<bool> Guard(bCleaning, true);
        if (auto* View = Widget()) if (auto* Image = Cast<UImage>(View->GetWidgetFromName(TEXT("AssetPreviewImage"))))
            Image->SetBrushFromTexture(nullptr);
        if (auto* View = Widget()) if (auto* Viewport = Cast<UViewport>(View->GetWidgetFromName(TEXT("AssetPreviewViewport"))))
            Viewport->SetVisibility(ESlateVisibility::Collapsed);
        if (PreviewActor.IsValid())
        {
            if (!PreviewActor->Destroy()) return FGamePlatformResult::Failure(TEXT("Demo.ContentActorCleanup"), TEXT("自己的预览Actor未销毁，保留诊断。"));
            PreviewActor.Reset();
        }
#if WITH_EDITOR
        const auto Closed = CloseOwnedEditor();
        if (!Closed.IsSuccess()) return Closed;
#endif
        if (Lease.IsValid())
        {
            auto* Service = Data();
            if (!Service) return FGamePlatformResult::Failure(TEXT("Demo.ContentDataUnavailable"), TEXT("Data服务失效，无法确认本次租约清理。"));
            const auto Released = Service->ReleaseResources(Lease);
            if (!Released.IsSuccess()) return Released;
            Lease = {};
        }
        return FGamePlatformResult::Success();
    }
    FGamePlatformResult Show(UObject& Asset, FName Mode, int32 Size, float Distance, float Yaw)
    {
        const TStrongObjectPtr<UObject> KeepAsset(&Asset);
        const uint64 ExpectedGeneration = Generation;
        const TWeakObjectPtr<UObject> ExpectedWidget = Context.SampleWidget;
        const auto IsCurrent = [this, ExpectedGeneration, ExpectedWidget]() { return bPrepared && bWaiting && Generation == ExpectedGeneration && Context.SampleWidget == ExpectedWidget && ExpectedWidget.IsValid() && Context.World.IsValid() && !Context.World->bIsTearingDown; };
        const FString Details = FString::Printf(TEXT("%s\n类型：%s\n所有者：%s\n"), *Asset.GetPathName(), *Asset.GetClass()->GetPathName(), *OwnerPlugin.ToString());
        Status(Details);
        if (!IsCurrent()) return FGamePlatformResult::Cancelled(TEXT("呈现资源信息时运行已失效，停止后不恢复旧显示。"));
        if (Mode == TEXT("Inspect"))
        {
            auto Result = FGamePlatformResult::Success(); Result.Message = Details + TEXT("已真实加载并显示身份；此操作不等于视觉、声音或世界消费验收。"); return Result;
        }
        if (Mode == TEXT("NativeEditor"))
        {
#if WITH_EDITOR
            if (Asset.IsA<UWorld>()) return FGamePlatformResult::Unsupported(TEXT("Demo.ContentWorldHost"), TEXT("世界资源须在登记的隔离世界中进入；不从内容页直接替换正在编辑的地图。"));
            auto* Editors = GEditor ? GEditor->GetEditorSubsystem<UAssetEditorSubsystem>() : nullptr;
            FText Error;
            if (!Editors || !Editors->CanOpenEditorForAsset(&Asset, EAssetTypeActivationOpenedMethod::View, &Error))
                return FGamePlatformResult::Failure(TEXT("Demo.ContentNativePreviewUnavailable"), TEXT("原生只读预览不可用：") + Error.ToString());
            auto* Existing = Editors->FindEditorForAsset(&Asset, false);
            if (Existing && Existing->GetOpenMethod() != EAssetOpenMethod::View)
                return FGamePlatformResult::Unsupported(TEXT("Demo.ContentExistingEditWindow"), TEXT("该资源已有可编辑窗口；保持其原状态，请在人工处理后重试只读预览。"));
            const auto ExistingEditors = Editors->GetAllOpenEditors();
            if (!Editors->OpenEditorForAsset(&Asset, EToolkitMode::Standalone, {}, true, EAssetTypeActivationOpenedMethod::View))
                return FGamePlatformResult::Failure(TEXT("Demo.ContentNativePreviewFailed"), TEXT("原生资产预览未打开。"));
            auto* OpenedEditor = Editors->FindEditorForAsset(&Asset, false);
            if (!Existing && OpenedEditor && !ExistingEditors.Contains(OpenedEditor)) TrackEditor(*Editors, Asset, OpenedEditor);
            if (!IsCurrent())
            {
                const auto Closed = CloseOwnedEditor();
                return Closed.IsSuccess() ? FGamePlatformResult::Cancelled(TEXT("打开原生预览时案例已停止，迟到窗口已精确清理。")) : Closed;
            }
            if (!OpenedEditor || OpenedEditor->GetOpenMethod() != EAssetOpenMethod::View)
            {
                const auto Closed = CloseOwnedEditor();
                return Closed.IsSuccess() ? FGamePlatformResult::Unsupported(TEXT("Demo.ContentViewMethod"), TEXT("原生编辑器没有提供只读View窗口，未当作只读预览通过。")) : Closed;
            }
            auto Result = FGamePlatformResult::Success(); Result.Message = Details + TEXT("已打开原生只读资产预览。请在该预览中人工观察、播放和记录；既有窗口不归本案例关闭。"); return Result;
#else
            return FGamePlatformResult::Unsupported(TEXT("Demo.ContentRequiresEditor"), TEXT("NativeEditor需要Editor宿主；开发客户端只提供明确支持的Auto预览。"));
#endif
        }
        if (auto* Texture = Cast<UTexture2D>(&Asset))
        {
            const TStrongObjectPtr<UUserWidget> KeepTextureView(Widget());
            auto* View = KeepTextureView.Get();
            auto* Image = View ? Cast<UImage>(View->GetWidgetFromName(TEXT("AssetPreviewImage"))) : nullptr;
            if (!Image) return FGamePlatformResult::Failure(TEXT("Demo.ContentPreviewWidget"), TEXT("缺少Monolith制作的AssetPreviewImage样板。"));
            Image->SetVisibility(ESlateVisibility::Visible);
            if (!IsCurrent()) return FGamePlatformResult::Cancelled(TEXT("显示图像时案例已停止，不恢复旧纹理。"));
            Image->SetBrushFromTexture(Texture, false);
            if (!IsCurrent()) return FGamePlatformResult::Cancelled(TEXT("设置图像时案例已停止，不继续写旧样板。"));
            Image->SetDesiredSizeOverride(FVector2D(Size, Size));
            if (!IsCurrent()) return FGamePlatformResult::Cancelled(TEXT("图像尺寸通知期间案例已停止。"));
            auto Result = FGamePlatformResult::Success(); Result.Message = Details + FString::Printf(TEXT("实际图像按%d×%d显示，保持原纹理RGB；请人工核对透明度与细节。"), Size, Size); return Result;
        }
        UStaticMesh* Static = Cast<UStaticMesh>(&Asset);
        auto* Material = Cast<UMaterialInterface>(&Asset);
        auto* Skeletal = Cast<USkeletalMesh>(&Asset);
        if (Material) Static = Cast<UStaticMesh>(FSoftObjectPath(TEXT("/Engine/BasicShapes/Sphere.Sphere")).ResolveObject());
        if (!Static && !Skeletal) return FGamePlatformResult::Unsupported(TEXT("Demo.ContentPreviewType"), Details + TEXT("此类型未接入Auto显示；Editor可显式选择NativeEditor。声音/VFX正式播放仍使用既有平台服务。"));
        const TStrongObjectPtr<UUserWidget> KeepView(Widget());
        auto* View = KeepView.Get();
        auto* Viewport = View ? Cast<UViewport>(View->GetWidgetFromName(TEXT("AssetPreviewViewport"))) : nullptr;
        if (Viewport) Viewport->TakeWidget(); // 只构造Monolith资产已有原生控件的Slate资源，不生成视觉树。
        if (!IsCurrent()) return FGamePlatformResult::Cancelled(TEXT("嵌入视口准备期间运行已失效。"));
        UWorld* World = Viewport ? Viewport->GetViewportWorld() : nullptr;
        // 使用引擎原生UViewport可视样板；其EditorPreview世界不启动游戏业务子系统。
        // 只由引擎更新视觉场景，不在Widget/VM中Tick或查询业务状态，不创建第二套资源管理器。
        if (!Viewport || !Viewport->bIsEditorPreview || !World || World->WorldType != EWorldType::EditorPreview)
            return FGamePlatformResult::Unsupported(TEXT("Demo.ContentPreviewWorld"), TEXT("三维样板需要已构造且配置为EditorPreview的原生嵌入视口。"));
        const FVector Position = FVector::ZeroVector;
        const FRotator Facing(0.f, Yaw, 0.f);
        FActorSpawnParameters Spawn; Spawn.ObjectFlags |= RF_Transient; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        AActor* Actor = nullptr;
        if (Static)
        {
            auto* MeshActor = World->SpawnActor<AStaticMeshActor>(Position, Facing, Spawn);
            if (MeshActor) { auto* Component = MeshActor->GetStaticMeshComponent(); Component->SetMobility(EComponentMobility::Movable); Component->SetStaticMesh(Static); if (Material) Component->SetMaterial(0, Material); Actor = MeshActor; }
        }
        else
        {
            auto* MeshActor = World->SpawnActor<ASkeletalMeshActor>(Position, Facing, Spawn);
            if (MeshActor) { MeshActor->GetSkeletalMeshComponent()->SetSkeletalMesh(Skeletal); Actor = MeshActor; }
        }
        if (!Actor) return FGamePlatformResult::Failure(TEXT("Demo.ContentPreviewSpawn"), TEXT("真实预览Actor创建失败。"));
        if (!IsCurrent()) { Actor->Destroy(); return FGamePlatformResult::Cancelled(TEXT("创建预览时运行失效，立即撤销迟到样板。")); }
        Actor->SetActorEnableCollision(false); Actor->SetReplicates(false); PreviewActor = Actor;
        FVector Center, Extent; Actor->GetActorBounds(false, Center, Extent, true);
        const FVector Camera = Center + FVector(-Distance, 0.f, Extent.Z * 0.15f);
        Viewport->SetViewLocation(Camera); Viewport->SetViewRotation((Center - Camera).Rotation());
        Viewport->SetEnableAdvancedFeatures(true); Viewport->SetLightIntensity(1.f); Viewport->SetSkyIntensity(1.f);
        if (auto* Image = Cast<UImage>(View->GetWidgetFromName(TEXT("AssetPreviewImage")))) Image->SetVisibility(ESlateVisibility::Collapsed);
        if (!IsCurrent()) return FGamePlatformResult::Cancelled(TEXT("切换预览显示时案例已停止，不恢复旧视口。"));
        Viewport->SetVisibility(ESlateVisibility::Visible);
        if (!IsCurrent()) return FGamePlatformResult::Cancelled(TEXT("视口显示通知期间案例已停止。"));
        auto Result = FGamePlatformResult::Success(); Result.Message = Details + TEXT("已在原生嵌入视口显示真实模型；停止/切资源只销毁自己的样板，保持原资产不变。"); return Result;
    }
};

class FContentAssetScenario final : public IDivineBeastsDemoScenarioAdapter
{
public:
    FContentAssetScenario() : State(MakeShared<FContentAssetState>()) {}
    ~FContentAssetScenario() override
    {
        State->bPrepared = false; ++State->Generation; State->bWaiting = false; State->Pending = {}; State->Clear();
#if WITH_EDITOR
        // World退出时即使用户保留脏预览窗口，也必须解除本案例的事件订阅。
        State->ForgetEditor();
#endif
    }
    void Prepare(const FDivineBeastsDemoScenarioContext& Context, FDivineBeastsDemoOperationCompletion Completion) override
    {
        State->Context = Context; State->Instance = Context.World.IsValid() ? Context.World->GetGameInstance() : nullptr;
        FString Owner, Suffix; Context.Scenario.FeatureId.ToString().Split(TEXT("."), &Owner, &Suffix); State->OwnerPlugin = FName(*Owner);
        const auto Plugin = IPluginManager::Get().FindPlugin(Owner);
        const auto& Parameters = Context.Scenario.Parameters;
        const TArray<FName> Expected{TEXT("Asset"), TEXT("PreviewMode"), TEXT("PixelSize"), TEXT("DistanceCm"), TEXT("YawDegrees")};
        bool bParametersMatch = Parameters.Num() == Expected.Num();
        for (int32 Index = 0; bParametersMatch && Index < Expected.Num(); ++Index) bParametersMatch = Parameters[Index].ParameterId == Expected[Index];
        State->bPrepared = Context.Token.IsValid() && Context.World.IsValid() && !Context.World->bIsTearingDown && State->Data() &&
            Context.Scenario.AdapterId == TEXT("ContentAsset") && Plugin.IsValid() && Plugin->IsEnabled() &&
            Plugin->GetDescriptor().bCanContainContent && Plugin->GetDescriptor().Modules.IsEmpty() && bParametersMatch && State->Widget();
        if (State->bPrepared)
        {
            const uint64 ExpectedGeneration = State->Generation;
            auto* View = State->Widget();
            const TStrongObjectPtr<UUserWidget> KeepView(View);
            const auto IsCurrent = [this, ExpectedGeneration, View]() { return State->bPrepared && State->Generation == ExpectedGeneration && State->Widget() == View && State->Context.World.IsValid() && !State->Context.World->bIsTearingDown; };
            auto* List = Cast<UListView>(View->GetWidgetFromName(TEXT("AssetPreviewList")));
            const auto* Viewport = Cast<UViewport>(View->GetWidgetFromName(TEXT("AssetPreviewViewport")));
            State->bPrepared = List && Viewport && Viewport->bIsEditorPreview && Cast<UImage>(View->GetWidgetFromName(TEXT("AssetPreviewImage"))) && Cast<UTextBlock>(View->GetWidgetFromName(TEXT("AssetPreviewStatusText")));
            if (State->bPrepared)
            {
                List->ClearListItems();
                if (!IsCurrent()) { Completion(Context.Token, FGamePlatformResult::Cancelled(TEXT("准备资源列表时运行已停止。"))); return; }
                TArray<FAssetData> Assets;
                auto& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
                Registry.GetAssetsByPath(FName(*(TEXT("/") + Owner)), Assets, true, true);
                Assets.Sort([](const FAssetData& A, const FAssetData& B) { return A.GetSoftObjectPath().ToString() < B.GetSoftObjectPath().ToString(); });
                for (const auto& Asset : Assets)
                {
                    if (Asset.IsRedirector()) continue;
                    auto* Item = NewObject<UDivineBeastsDemoMenuItem>(View);
                    Item->Identity = FName(*Asset.GetSoftObjectPath().ToString());
                    Item->Label = FText::FromString(Asset.GetSoftObjectPath().ToString());
                    Item->Status = FText::FromString(Asset.AssetClassPath.ToString());
                    List->AddItem(Item);
                    if (!IsCurrent()) { Completion(Context.Token, FGamePlatformResult::Cancelled(TEXT("资源列表通知期间运行已停止。"))); return; }
                }
                State->Status(TEXT("资源列表来自本包原生Registry首次快照。选择Asset路径、模式及尺寸后显式执行；列表存在不等于预览通过。"));
            }
        }
        Completion(Context.Token, State->bPrepared ? FGamePlatformResult::Success() : FGamePlatformResult::Failure(TEXT("Demo.ContentPrepare"), TEXT("内容包未启用、宿主/Token/Data/Monolith样板缺失或参数合同不匹配。")));
    }
    void Execute(FName Operation, const TArray<FDivineBeastsDemoParameterValue>& Values, FDivineBeastsDemoOperationCompletion Completion) override
    {
        auto* Service = State->Data();
        if (!State->bPrepared || !Service || !State->Context.World.IsValid() || State->Context.World->bIsTearingDown || State->bWaiting || State->bCleaning)
        { Completion(State->Context.Token, FGamePlatformResult::Failure(TEXT("Demo.ContentNotReady"), TEXT("内容预览未准备、正在执行/清理或World失效。"))); return; }
        if (Operation != TEXT("Execute") && Operation != TEXT("Reset"))
        { Completion(State->Context.Token, FGamePlatformResult::Unsupported(TEXT("Demo.ContentOperation"), TEXT("内容预览仅支持Execute/Reset/Stop，无伪暂停。"))); return; }
        const uint64 BeforeClear = State->Generation;
        const auto Cleared = State->Clear();
        if (!State->bPrepared || State->Generation != BeforeClear || !State->Context.World.IsValid() || State->Context.World->bIsTearingDown)
        { Completion(State->Context.Token, FGamePlatformResult::Cancelled(TEXT("清理旧预览期间运行已失效，不继续加载或报告重置成功。"))); return; }
        if (!Cleared.IsSuccess() || Operation == TEXT("Reset")) { Completion(State->Context.Token, Cleared); return; }
        if (Values.Num() != 5 || Values[0].Type != EDivineBeastsDemoParameterType::AssetPath || Values[1].Type != EDivineBeastsDemoParameterType::Identifier ||
            Values[2].Type != EDivineBeastsDemoParameterType::Integer || Values[2].IntegerValue < 16 || Values[2].IntegerValue > 512 ||
            Values[3].Type != EDivineBeastsDemoParameterType::Number || !FMath::IsFinite(Values[3].NumberValue) || Values[3].NumberValue < 100 || Values[3].NumberValue > 2000 ||
            Values[4].Type != EDivineBeastsDemoParameterType::Number || !FMath::IsFinite(Values[4].NumberValue) || Values[4].NumberValue < -180 || Values[4].NumberValue > 180 ||
            (Values[1].IdentifierValue != TEXT("Auto") && Values[1].IdentifierValue != TEXT("Inspect") && Values[1].IdentifierValue != TEXT("NativeEditor")))
        { Completion(State->Context.Token, FGamePlatformResult::Failure(TEXT("Demo.ContentParameters"), TEXT("需要Asset/Auto或Inspect或NativeEditor/16..512像素/100..2000厘米/±180度；非法输入不夹紧。"))); return; }
        const FSoftObjectPath AssetPath = Values[0].AssetPathValue;
        const auto Valid = ValidateContentAssetSelection(State->OwnerPlugin, AssetPath);
        if (!Valid.IsSuccess()) { Completion(State->Context.Token, Valid); return; }
        const auto& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
        const auto AssetData = Registry.GetAssetByObjectPath(AssetPath);
        if (!AssetData.IsValid() || AssetData.IsRedirector())
        { Completion(State->Context.Token, FGamePlatformResult::Failure(TEXT("Demo.ContentRegistry"), TEXT("实际AssetRegistry中缺少该本包资源或它只是重定向器。"))); return; }
        auto* Caller = State->Context.World->GetSubsystem<UDivineBeastsDemoWorldSubsystem>();
        if (!Caller) { Completion(State->Context.Token, FGamePlatformResult::Failure(TEXT("Demo.ContentCaller"), TEXT("World租约调用者不存在。"))); return; }
        State->Pending = MoveTemp(Completion); State->bWaiting = true; const uint64 Generation = ++State->Generation;
        const FName Mode = Values[1].IdentifierValue; const int32 Size = static_cast<int32>(Values[2].IntegerValue);
        const float Distance = static_cast<float>(Values[3].NumberValue), Yaw = static_cast<float>(Values[4].NumberValue);
        TWeakPtr<FContentAssetState> Weak(State);
        FGamePlatformResult Accepted;
        State->Lease = Service->AcquireResources({AssetPath, FSoftObjectPath(TEXT("/Engine/BasicShapes/Sphere.Sphere"))}, EGamePlatformDataLifetime::World, Caller,
            [Weak, Generation, AssetPath, Mode, Size, Distance, Yaw](const FGamePlatformDataLease& Lease, const FGamePlatformResult& Loaded)
            {
                auto Self = Weak.Pin();
                if (!Self || !Self->bPrepared || !Self->bWaiting || Self->Generation != Generation || !Self->Context.World.IsValid()) return;
                const auto& Expected = Self->Lease;
                if (Lease.ScopeId != Expected.ScopeId || Lease.LeaseId != Expected.LeaseId || Lease.Generation != Expected.Generation || Lease.IssuerProof != Expected.IssuerProof ||
                    Lease.DefinitionId != Expected.DefinitionId || Lease.ResourcePaths != Expected.ResourcePaths || Lease.Bundles != Expected.Bundles)
                { Self->Finish(FGamePlatformResult::Failure(TEXT("Demo.ContentLeaseMismatch"), TEXT("资源回调完整签发身份不匹配。"))); return; }
                if (!Loaded.IsSuccess()) { Self->Finish(Loaded); return; }
                UObject* Asset = AssetPath.ResolveObject();
                Self->Finish(Asset ? Self->Show(*Asset, Mode, Size, Distance, Yaw) : FGamePlatformResult::Failure(TEXT("Demo.ContentNotLoaded"), TEXT("租约通知成功但实际资源不可解析。")));
            }, Accepted);
        if (!Accepted.IsSuccess()) State->Finish(Accepted);
    }
    void Stop(FDivineBeastsDemoOperationCompletion Completion) override
    {
        State->bPrepared = false; ++State->Generation;
        auto Pending = MoveTemp(State->Pending); State->bWaiting = false;
        const auto Cleared = State->Clear();
        if (auto* View = State->Widget()) if (auto* List = Cast<UListView>(View->GetWidgetFromName(TEXT("AssetPreviewList")))) List->ClearListItems();
        if (Pending) Pending(State->Context.Token, FGamePlatformResult::Cancelled(TEXT("内容预览已停止，只撤销本次资源需求。")));
        Completion(State->Context.Token, Cleared);
    }
private:
    TSharedRef<FContentAssetState> State;
};
TUniquePtr<IDivineBeastsDemoScenarioAdapter> CreateContentAssetScenario() { return MakeUnique<FContentAssetScenario>(); }
}
