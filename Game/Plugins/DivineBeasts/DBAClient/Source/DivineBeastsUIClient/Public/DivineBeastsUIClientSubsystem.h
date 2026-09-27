#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Contracts/DivineBeastsUIContracts.h"
#include "GamePlatformUITypes.h"
#include "Loading/GamePlatformLoadingScreenService.h"
#include "DivineBeastsUIClientSubsystem.generated.h"

class UDivineBeastsUIViewModel;
class UGamePlatformHUDWidget;
class UGamePlatformLoadingScreenService;
class UGamePlatformToastWidget;
class UGamePlatformUILayerStack;
class UGamePlatformUIManagerSubsystem;
class UGamePlatformUIScreen;
class UGamePlatformUIScreenDefinition;

/** FDivineBeastsUIContractHandle（项目UI应用契约注册句柄）。 */
USTRUCT(BlueprintType)
struct DIVINEBEASTSUICLIENT_API FDivineBeastsUIContractHandle
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    FGuid Id;

    bool IsValid() const { return Id.IsValid(); }
};

DECLARE_MULTICAST_DELEGATE_OneParam(
    FDivineBeastsUIStateChangedNative,
    const FDivineBeastsUIViewState&);

/**
 * UDivineBeastsUIClientSubsystem（神兽联盟项目UI客户端子系统）。
 * 只管理项目UI契约/页面定义注册并复用GamePlatformUIManagerSubsystem。
 */
UCLASS()
class DIVINEBEASTSUICLIENT_API UDivineBeastsUIClientSubsystem final
    : public ULocalPlayerSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    FDivineBeastsUIContractHandle RegisterApplicationContract(
        FName AdapterId,
        UObject* QuerySourceObject,
        UObject* CommandPortObject);

    bool UnregisterApplicationContract(
        const FDivineBeastsUIContractHandle& Handle);

    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI")
    FDivineBeastsUIViewState GetViewState() const { return ViewState; }

    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI")
    TArray<FName> GetRegisteredScreenIds() const;

    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI")
    FName GetRecommendedPrimaryScreenId() const;

    UFUNCTION(BlueprintCallable, Category="DivineBeasts|UI")
    FGamePlatformUIAsyncRequest OpenScreen(
        FName ScreenId,
        UDivineBeastsUIViewModel* ViewModel);

    UFUNCTION(BlueprintCallable, Category="DivineBeasts|UI")
    bool CancelScreenOpen(FGuid RequestId);

    UFUNCTION(BlueprintCallable, Category="DivineBeasts|UI")
    bool CloseScreen(UGamePlatformUIScreen* Screen);

    UFUNCTION(BlueprintCallable, Category="DivineBeasts|UI")
    bool InstallRootLayoutClass(
        TSubclassOf<UGamePlatformUILayerStack> RootLayoutClass);

    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI")
    UGamePlatformLoadingScreenService* GetLoadingScreenService() const;

    UFUNCTION(BlueprintCallable, Category="DivineBeasts|UI")
    bool AttachHUDWidget(UGamePlatformHUDWidget* Widget);

    UFUNCTION(BlueprintCallable, Category="DivineBeasts|UI")
    bool AttachToastWidget(UGamePlatformToastWidget* Widget);

    /**
     * 按界面身份创建项目 ViewModel。
     * P0 登录/加载页面返回专用 ViewModel，其余页面保持兼容通用 ViewModel。
     * 对象 Outer 为当前 LocalPlayer 子系统，生命周期随本地玩家结束而清理。
     */
    UDivineBeastsUIViewModel* CreateViewModel(FName ScreenId);

    void SubmitCommand(
        FDivineBeastsUICommand Command,
        TFunction<void(const FDivineBeastsUICommandResult&)> Completion);

    bool CancelCommand(const FGuid& RequestId);

    FDivineBeastsUIStateChangedNative& OnStateChanged()
    {
        return StateChanged;
    }

private:
    void RegisterDefaultScreenDefinitions();
    void UnregisterDefaultScreenDefinitions();
    void HandleViewStateChanged(const FDivineBeastsUIViewState& NewState);
    void PullInitialState();
    void SyncLoadingService();
    void DetachContract();

    UPROPERTY(Transient)
    TObjectPtr<UGamePlatformUIManagerSubsystem> PlatformUI = nullptr;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UGamePlatformUIScreenDefinition>> RegisteredDefinitions;

    UPROPERTY(Transient)
    TWeakObjectPtr<UObject> QuerySourceObject;

    UPROPERTY(Transient)
    TWeakObjectPtr<UObject> CommandPortObject;

    FName ContractAdapterId = NAME_None;
    FDivineBeastsUIContractHandle ContractHandle;
    FDelegateHandle QueryStateHandle;

    FDivineBeastsUIViewState ViewState;
    FGamePlatformLoadingToken LoadingToken;
    FDivineBeastsUIStateChangedNative StateChanged;
};
