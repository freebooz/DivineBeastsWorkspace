/** 平台层Editor/CI工具组合：注册中立验证规则、编辑器菜单与显式资产制作命令。
 * 不拥有项目玩法或服务器状态；资产修改只由调用者显式指定，模块启动不修改内容，关闭注销入口。
 */
#include "Modules/ModuleManager.h"
#include "Manual/GamePlatformEditorResources.h"
#include "Engine/World.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimData/IAnimationDataController.h"
#include "ContentBrowserModule.h"
#include "DataValidationModule.h"
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "FileHelpers.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "IContentBrowserSingleton.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Misc/DataValidation.h"
#include "Misc/EngineVersion.h"
#include "Performance/GamePlatformPerformanceTestRunner.h"
#include "Review/GamePlatformReviewTypes.h"
#include "ToolMenus.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/AppStyle.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Widgets/Text/STextBlock.h"
#include "Validation/GamePlatformStaticValidators.h"
#include "Validation/GamePlatformGlobalAssetValidator.h"
#include "Validation/GamePlatformValidationService.h"

#define LOCTEXT_NAMESPACE "GamePlatformDeveloperTools"

class FGamePlatformDeveloperToolsModule final : public IGamePlatformEditorResources
{

public:
    bool Acquire(UObject& Owner,UWorld& World,FGuid RunId,FGuid WorldId,uint64 Generation,FGamePlatformEditorResourceLease& Out,FString& Error)override {
        check(IsInGameThread());Error.Reset();Out={};
        if(!GEditor||!IsValid(&Owner)||!IsValid(&World)||World.bIsTearingDown||!RunId.IsValid()||!WorldId.IsValid()||Generation==0){Error=TEXT("Editor资源租约要求有效编辑器、来源和完整身份。");return false;}
        if(EditorLease.IsValid()||PendingEditorCleanup||bRetryingEditorCleanup){Error=TEXT("Editor全局资源已有拥有者或正在精确清理；不会抢占失效世界或后继工具。");return false;}
        EditorLease.LeaseId=FGuid::NewGuid();EditorLease.RunId=RunId;EditorLease.WorldId=WorldId;EditorLease.Generation=Generation;EditorLease.Editor=GEditor;EditorLease.Owner=&Owner;EditorLease.World=&World;Out=EditorLease;return true;
    }
    bool Owns(const FGamePlatformEditorResourceLease& Lease)const override {return Lease.IsValid()&&EditorLease==Lease&&Lease.Editor.Get()==GEditor&&Lease.Owner.IsValid()&&Lease.World.IsValid();}
    bool Release(const FGamePlatformEditorResourceLease& Lease,FString& Error)override {
        check(IsInGameThread());Error.Reset();
        if(!Lease.IsValid()||!(EditorLease==Lease)){Error=TEXT("Editor资源释放身份不匹配，保留原租约。");return false;}
        if(PendingEditorCleanup&&!bRetryingEditorCleanup){Error=TEXT("失败清理已由Editor作用域拥有，必须精确RetryCleanup确认后才能释放。");return false;}
        if(bRetryingEditorCleanup){bEditorCleanupReleaseConfirmed=true;return true;}
        EditorLease={};bEditorCleanupReleaseConfirmed=false;return true;
    }

    bool RetainCleanup(const FGamePlatformEditorResourceLease& Lease,FGamePlatformEditorCleanupRetry Retry,const FString& Error)override {
        check(IsInGameThread());if(!Lease.IsValid()||!(Lease==EditorLease)||!Retry||bRetryingEditorCleanup)return false;
        if(PendingEditorCleanup)return true; // 同一完整租约已保留；绝不替换原对象状态。
        PendingEditorCleanup=MoveTemp(Retry);PendingEditorCleanupError=Error;EditorCleanupChanged.Broadcast(GetCleanupSnapshot());return true;
    }
    bool RetryCleanup(const FGamePlatformEditorResourceLease& Lease,EGamePlatformEditorCleanupPolicy Policy,FString& Error)override {
        check(IsInGameThread());Error.Reset();
        if(bRetryingEditorCleanup||!PendingEditorCleanup||!Lease.IsValid()||!(Lease==EditorLease)){Error=TEXT("没有匹配完整租约的待清理状态，或重试同步重入。");return false;}
        TGuardValue<bool> Retrying(bRetryingEditorCleanup,true);
        // 回调可调用精确Release；本地移动强引用保证它不会在自己执行栈中被销毁。
        auto Work=MoveTemp(PendingEditorCleanup);const bool Done=Work(Policy,Error);
        if(!Done){PendingEditorCleanup=MoveTemp(Work);PendingEditorCleanupError=Error;bRetryingEditorCleanup=false;EditorCleanupChanged.Broadcast(GetCleanupSnapshot());return false;}
        if(!bEditorCleanupReleaseConfirmed){PendingEditorCleanup=MoveTemp(Work);Error=TEXT("回调未确认原租约释放，继续保留状态。");PendingEditorCleanupError=Error;bRetryingEditorCleanup=false;EditorCleanupChanged.Broadcast(GetCleanupSnapshot());return false;}
        // 原回调完整成功才真正清除租约；同步回调栈中不会出现可被后继Acquire抢占的空隙。
        EditorLease={};bEditorCleanupReleaseConfirmed=false;PendingEditorCleanupError.Reset();bRetryingEditorCleanup=false;EditorCleanupChanged.Broadcast(GetCleanupSnapshot());return true;
    }
    FGamePlatformEditorCleanupSnapshot GetCleanupSnapshot()const override {check(IsInGameThread());return {EditorLease,bool(PendingEditorCleanup)||bRetryingEditorCleanup,PendingEditorCleanupError};}
    FDelegateHandle AddCleanupChanged(FGamePlatformEditorCleanupChanged::FDelegate Handler)override{check(IsInGameThread());return EditorCleanupChanged.Add(MoveTemp(Handler));}
    void RemoveCleanupChanged(FDelegateHandle Handle)override{check(IsInGameThread());EditorCleanupChanged.Remove(Handle);}
    virtual void StartupModule() override

    {

        FGamePlatformValidationService::RegisterBuiltInRules();
        EditorCleanupInspect=MakeUnique<FAutoConsoleCommand>(TEXT("GP.EditorResources.Pending"),TEXT("只读列出待清理完整身份与冲突；不会自动重试。"),FConsoleCommandDelegate::CreateLambda([this]{const auto V=GetCleanupSnapshot();UE_LOG(LogTemp,Display,TEXT("EditorCleanup pending=%d lease=%s run=%s world=%s generation=%llu error=%s"),V.bPending,*V.Lease.LeaseId.ToString(),*V.Lease.RunId.ToString(),*V.Lease.WorldId.ToString(),V.Lease.Generation,*V.LastError);}));
        EditorCleanupRetryCommand=MakeUnique<FAutoConsoleCommand>(TEXT("GP.EditorResources.Retry"),TEXT("显式精确重试：LeaseId RunId WorldId Generation retry|preserve_external；只操作已保留原状态。"),FConsoleCommandWithArgsDelegate::CreateRaw(this,&FGamePlatformDeveloperToolsModule::RetryEditorCleanupCommand));
        // 引擎原生初始化是动画关键帧制作的前置条件；只注册入口，不扫描或自动改写资产。
        AnimationDataModelCommand = MakeUnique<FAutoConsoleCommand>(
            TEXT("GP.Animation.InitializeDataModel"),
            TEXT("显式初始化指定AnimSequence的数据模型并同步当前骨架；参数为资产路径，不自动保存或覆盖关键帧。"),
            FConsoleCommandWithArgsDelegate::CreateStatic(&InitializeAnimationDataModel));

        if (!IsRunningCommandlet())
        {
            UToolMenus::RegisterStartupCallback(
                FSimpleMulticastDelegate::FDelegate::CreateRaw(
                    this,
                    &FGamePlatformDeveloperToolsModule::RegisterMenus));
        }
    }

    virtual void ShutdownModule() override
    {
        PluginUIFontAuditCommand.Reset();
        EditorCleanupInspect.Reset();EditorCleanupRetryCommand.Reset();
        if(PendingEditorCleanup)UE_LOG(LogTemp,Error,TEXT("Editor模块关闭时仍有未解决清理；没有宣称恢复或抢占外部状态：%s"),*PendingEditorCleanupError);
        AnimationDataModelCommand.Reset();
        if (!IsRunningCommandlet())
        {
            UToolMenus::UnRegisterStartupCallback(this);
            UToolMenus::UnregisterOwner(this);
        }

        if (PluginMenuStyle.IsValid())
        {
            if (FSlateApplication::IsInitialized()) FSlateApplication::Get().DismissAllMenus();
            FSlateStyleRegistry::UnRegisterSlateStyle(*PluginMenuStyle);
            PluginMenuStyle.Reset();
        }

        FGamePlatformValidationService::ResetForShutdown();
    }

private:

    /** 只供本地插件菜单借用的样式；不写入引擎 AppStyle/CoreStyle。 */
    TSharedPtr<FSlateStyleSet> PluginMenuStyle;
    TUniquePtr<FAutoConsoleCommand> PluginUIFontAuditCommand;

    void RegisterPluginMenuTypography()
    {
        if (!PluginMenuStyle.IsValid())
        {
            PluginMenuStyle = MakeShared<FSlateStyleSet>(TEXT("GamePlatform.PluginMenuTypography"));
            PluginMenuStyle->SetParentStyleName(FAppStyle::GetAppStyleSetName());
            for (const TCHAR* Key : { TEXT("Menu.Label"), TEXT("Menu.Keybinding"), TEXT("Menu.Heading"), TEXT("ToolBar.Label"), TEXT("ToolBar.Heading") })
            {
                if (!FAppStyle::Get().HasWidgetStyle<FTextBlockStyle>(Key)) continue;
                FTextBlockStyle Text = FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>(Key);
                Text.Font.Size = 10;
                PluginMenuStyle->Set(Key, Text);
            }
            FEditableTextBoxStyle Input = FAppStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>(TEXT("Menu.EditableText"));
            Input.TextStyle.Font.Size = 10;
            PluginMenuStyle->Set(TEXT("Menu.EditableText"), Input);
            FSlateStyleRegistry::RegisterSlateStyle(*PluginMenuStyle);
            PluginUIFontAuditCommand = MakeUnique<FAutoConsoleCommand>(TEXT("GP.UI.PluginMenuFontAudit"),
                TEXT("只读核对插件局部菜单字体及引擎原字体；不修改样式。"),
                FConsoleCommandDelegate::CreateLambda([Style = PluginMenuStyle]
                {
                    const UToolMenu* Generated = UToolMenus::Get()->GenerateMenu(TEXT("LevelEditor.MainMenu.Tools"), FToolMenuContext());
                    const float GeneratedLabel = Generated ? Generated->GetStyleSet()->GetWidgetStyle<FTextBlockStyle>(TEXT("Menu.Label")).Font.Size : -1;
                    UE_LOG(LogTemp, Display, TEXT("PluginMenuFontAudit LocalLabel=%.1f LocalHeading=%.1f LocalKeybinding=%.1f EngineLabel=%.1f GeneratedEngineLabel=%.1f ScopePreserved=%d"),
                        Style->GetWidgetStyle<FTextBlockStyle>(TEXT("Menu.Label")).Font.Size,
                        Style->GetWidgetStyle<FTextBlockStyle>(TEXT("Menu.Heading")).Font.Size,
                        Style->GetWidgetStyle<FTextBlockStyle>(TEXT("Menu.Keybinding")).Font.Size,
                        FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>(TEXT("Menu.Label")).Font.Size, GeneratedLabel,
                        Generated && Generated->GetStyleSet()->GetStyleSetName() != Style->GetStyleSetName());
                }));
        }
        // 仅独占的插件子菜单可以选择整份局部样式；共享的引擎菜单不得整体改字号。
        for (const TCHAR* MenuName : { TEXT("LevelEditor.MainMenu.Tools.GamePlatformValidation"),
            TEXT("LevelEditor.MainMenu.Tools.GamePlatformPluginDemos") })
        {
            UToolMenus::Get()->ExtendMenu(MenuName)->AddDynamicSection(TEXT("GamePlatformPluginMenuTypography"),
                FNewToolMenuDelegate::CreateLambda([Style = PluginMenuStyle](UToolMenu* Generated)
                { if (Generated) Generated->SetStyleSet(Style.Get()); }));
        }
        UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Tools"))->AddDynamicSection(
            TEXT("GamePlatformPluginRootTypography"), FNewToolMenuDelegate::CreateLambda([Style = PluginMenuStyle](UToolMenu* Generated)
            {
                if (!Generated) return;
                for (FToolMenuSection& Section : Generated->Sections)
                {
                    if (Section.Name != TEXT("GamePlatformValidation") && Section.Name != TEXT("GamePlatformPluginDemos")) continue;
                    for (FToolMenuEntry& Entry : Section.Blocks)
                    {
                        const TAttribute<FText> Label = Entry.Label;
                        Entry.MakeCustomWidget = FNewToolMenuCustomWidget::CreateLambda(
                            [Style, Label](const FToolMenuContext&, const FToolMenuCustomWidgetContext&) -> TSharedRef<SWidget>
                            { return SNew(STextBlock).TextStyle(&Style->GetWidgetStyle<FTextBlockStyle>(TEXT("Menu.Label"))).Text(Label); });
                    }
                }
            }));
    }

    FGamePlatformEditorResourceLease EditorLease;
    FGamePlatformEditorCleanupChanged EditorCleanupChanged;
    FGamePlatformEditorCleanupRetry PendingEditorCleanup;FString PendingEditorCleanupError;bool bRetryingEditorCleanup=false,bEditorCleanupReleaseConfirmed=false;
    TUniquePtr<FAutoConsoleCommand> EditorCleanupInspect,EditorCleanupRetryCommand;
    /** 不接受脚本文本/路径；完整原租约与封闭处置枚举都必须匹配。 */
    void RetryEditorCleanupCommand(const TArray<FString>& Args){
        if(Args.Num()!=5){UE_LOG(LogTemp,Error,TEXT("需要完整LeaseId RunId WorldId Generation和retry|preserve_external。"));return;}
        auto Lease=GetCleanupSnapshot().Lease;FGuid L,Run,W;uint64 Generation=0;
        if(!FGuid::Parse(Args[0],L)||!FGuid::Parse(Args[1],Run)||!FGuid::Parse(Args[2],W)||!LexTryParseString(Generation,*Args[3])||L!=Lease.LeaseId||Run!=Lease.RunId||W!=Lease.WorldId||Generation!=Lease.Generation||(Args[4]!=TEXT("retry")&&Args[4]!=TEXT("preserve_external"))){UE_LOG(LogTemp,Error,TEXT("完整清理身份或封闭处置不匹配。"));return;}
        FString Error;const bool Done=RetryCleanup(Lease,Args[4]==TEXT("retry")?EGamePlatformEditorCleanupPolicy::RetryUnchanged:EGamePlatformEditorCleanupPolicy::PreserveExternalChanges,Error);UE_LOG(LogTemp,Display,TEXT("EditorCleanup success=%d error=%s"),Done,*Error);
    }
    /** 仅游戏线程Editor制作调用；失败明确报错，保存及后续关键帧仍归调用者，不含项目资源身份。 */
    static void InitializeAnimationDataModel(const TArray<FString>& Arguments)
    {
        if (!IsInGameThread() || Arguments.Num() != 1)
        {
            UE_LOG(LogTemp, Error, TEXT("GP.Animation.InitializeDataModel要求游戏线程和一个资产路径。"));
            return;
        }
        UAnimSequence* Sequence = LoadObject<UAnimSequence>(nullptr, *Arguments[0]);
        if (!Sequence || !Sequence->GetSkeleton())
        {
            UE_LOG(LogTemp, Error, TEXT("动画或骨架不可用：%s"), *Arguments[0]);
            return;
        }
        // 直接NewObject的序列可能只有模型对象，没有Sequencer的MovieScene/ControlRig子对象。
        // 使用引擎初始化入口补齐。已存在的FK Control Rig可能仍是换骨架前的骨树，
        // 只初始化不会增加新骨骼，随后写轨道会失败；显式同步当前骨架后才能制作关键帧。
        // 不手工改Rig内部对象、不改第三方实现，正常兼容骨架保留已有轨道。
        Sequence->GetController().InitializeModel();
        Sequence->GetController().UpdateWithSkeleton(Sequence->GetSkeleton(), false);
        Sequence->GetController().NotifyPopulated();
        Sequence->MarkPackageDirty();
        UE_LOG(LogTemp, Display, TEXT("动画数据模型初始化完成，仍需关键帧、保存及独立Cook验证：%s"), *Sequence->GetPathName());
    }
    /** 编辑器模块独占命令注册；关闭时释放，避免重载后重复入口。 */
    TUniquePtr<FAutoConsoleCommand> AnimationDataModelCommand;

    void RegisterMenus()
    {
        FToolMenuOwnerScoped OwnerScoped(this);
        RegisterPluginMenuTypography();

        UToolMenu* ToolsMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Tools");
        if (!ToolsMenu)
        {
            return;
        }

        UToolMenu* ValidationMenu = ToolsMenu->AddSubMenu(
            this,
            "GamePlatformValidation",
            "GamePlatformValidation",
            LOCTEXT("MenuLabel", "Game Platform Validation（游戏平台验证）"),
            LOCTEXT("MenuTooltip", "资产验证、架构门禁、端侧泄漏、人工验收与性能证据入口。"));
        PopulateValidationMenu(ValidationMenu);

        // 仅注册跨项目可复用的开发演示入口：所有项目配置从DefaultEditor.ini读取。
        // 若项目没有指定地图和启动标志，按钮仅显示失败日志，不会跳过登录。
        UToolMenu* PluginDemoMenu = ToolsMenu->AddSubMenu(
            this, "GamePlatformPluginDemos", "GamePlatformPluginDemos",
            LOCTEXT("PluginDemoMenu", "Plugin Demos（插件演示中心）"),
            LOCTEXT("PluginDemoHelp", "打开经授权的项目演示关卡并进入编辑器内演示。"));
        if (PluginDemoMenu)
        {
            FToolMenuSection& DemoSection = PluginDemoMenu->AddSection(
                "GamePlatformPluginDemoActions",
                LOCTEXT("PluginDemoActions", "Development Demos（开发插件演示）"));
            DemoSection.AddMenuEntry(
                "OpenPluginDemoHub",
                LOCTEXT("PluginDemoHub", "打开插件演示总览并运行"),
                LOCTEXT("PluginDemoHubHelp", "仅在显式演示模式下打开已制作的总览地图并开始PIE；不接正式登录。"),
                FSlateIcon(),
                FUIAction(FExecuteAction::CreateRaw(
                    this, &FGamePlatformDeveloperToolsModule::ExecuteMenuAction,
                    FString(TEXT("OpenPluginDemoHub")))));
        }
    }

    void PopulateValidationMenu(UToolMenu* Menu)
    {
        if (!Menu)
        {
            return;
        }

        FToolMenuSection& Section = Menu->AddSection(
            "GamePlatformValidationActions",
            LOCTEXT("SectionLabel", "Validation Actions（验证操作）"));

        const struct FMenuSpec
        {
            const TCHAR* Name;
            const TCHAR* Label;
            const TCHAR* Description;
        } Specs[] =
        {
            { TEXT("ValidateSelected"), TEXT("Validate Selected（验证所选资产）"), TEXT("复用UE Data Validation验证当前选择。") },
            { TEXT("ValidateFolder"), TEXT("Validate Folder（验证目录）"), TEXT("目录级验证与资产审计。") },
            { TEXT("ValidateProject"), TEXT("Validate Project（验证项目）"), TEXT("运行项目级数据验证与质量门禁。") },
            { TEXT("ValidateArchitecture"), TEXT("Validate Architecture（验证架构）"), TEXT("检查三层依赖、循环依赖与端侧边界。") },
            { TEXT("AuditServerAssets"), TEXT("Audit Server Assets（审计服务器资产）"), TEXT("检查Server-safe引用与客户端表现泄漏。") },
            { TEXT("AuditClientAssets"), TEXT("Audit Client Assets（审计客户端资产）"), TEXT("检查客户端Cook/构建工件中的ServerOnly内容。") },
            { TEXT("RunReviewCases"), TEXT("Run Review Cases（运行人工验收用例）"), TEXT("只创建/整理待人工记录，不自动代签Passed。") },
            { TEXT("PerformanceTests"), TEXT("Performance Tests（性能测试）"), TEXT("运行预定义场景并与兼容基线比较。") },
            { TEXT("OpenLatestReport"), TEXT("Open Latest Report（查看最新报告）"), TEXT("报告目录为Saved/GamePlatformValidation。") }
        };

        for (const FMenuSpec& Spec : Specs)
        {
            Section.AddMenuEntry(
                FName(Spec.Name),
                FText::FromString(Spec.Label),
                FText::FromString(Spec.Description),
                FSlateIcon(),
                FUIAction(FExecuteAction::CreateRaw(
                    this,
                    &FGamePlatformDeveloperToolsModule::ExecuteMenuAction,
                    FString(Spec.Name))));
        }
    }

    static bool IsProjectOwnedAsset(const FAssetData& Asset)
    {
        const FString Package = Asset.PackageName.ToString();
        return Package.StartsWith(TEXT("/Game/"))
            || Package.StartsWith(TEXT("/GamePlatform"))
            || Package.StartsWith(TEXT("/DBA"));
    }

    static void RunDataValidation(const TArray<FAssetData>& Assets, bool bDependencies)
    {
        if (Assets.IsEmpty())
        {
            UE_LOG(LogTemp, Warning, TEXT("GamePlatformDeveloperTools：没有可验证资产。"));
            return;
        }

        IDataValidationModule::Get().ValidateAssets(
            Assets,
            bDependencies,
            EDataValidationUsecase::Manual);
    }

    TArray<FAssetData> GetSelectedAssets() const
    {
        TArray<FAssetData> Assets;
        FContentBrowserModule& ContentBrowser =
            FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
        ContentBrowser.Get().GetAllSelectedAssets(Assets);
        return Assets;
    }

    TArray<FAssetData> GetSelectedFolderAssets() const
    {
        TArray<FString> Folders;
        FContentBrowserModule& ContentBrowser =
            FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
        ContentBrowser.Get().GetSelectedFolders(Folders);

        FAssetRegistryModule& RegistryModule =
            FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
        IAssetRegistry& Registry = RegistryModule.Get();

        TArray<FAssetData> Assets;
        for (const FString& Folder : Folders)
        {
            TArray<FAssetData> FolderAssets;
            Registry.GetAssetsByPath(FName(*Folder), FolderAssets, true, true);
            Assets.Append(MoveTemp(FolderAssets));
        }

        return Assets;
    }

    TArray<FAssetData> GetProjectAssets() const
    {
        FAssetRegistryModule& RegistryModule =
            FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
        IAssetRegistry& Registry = RegistryModule.Get();

        TArray<FAssetData> AllAssets;
        Registry.GetAllAssets(AllAssets, true);

        TArray<FAssetData> ProjectAssets;
        ProjectAssets.Reserve(AllAssets.Num());
        for (const FAssetData& Asset : AllAssets)
        {
            if (IsProjectOwnedAsset(Asset))
            {
                ProjectAssets.Add(Asset);
            }
        }
        return ProjectAssets;
    }

    void RunArchitectureValidation()
    {
        FGamePlatformValidationRunSummary Summary;
        Summary.RunId = FGamePlatformValidationService::CreateRunId();
        Summary.StartedAt = FDateTime::UtcNow();

        UGamePlatformDependencyValidator* DependencyValidator =
            NewObject<UGamePlatformDependencyValidator>(GetTransientPackage());
        DependencyValidator->ValidateWorkspace(Summary.Results);

        UGamePlatformInheritanceBoundaryValidator* InheritanceValidator =
            NewObject<UGamePlatformInheritanceBoundaryValidator>(GetTransientPackage());
        InheritanceValidator->ValidateWorkspace(Summary.Results);

        UGamePlatformRPCAndAuthorityValidator* RpcValidator =
            NewObject<UGamePlatformRPCAndAuthorityValidator>(GetTransientPackage());
        RpcValidator->ValidateWorkspace(Summary.Results);

        FGamePlatformValidationService::ApplyAllowlist(Summary.Results);

        Summary.FinishedAt = FDateTime::UtcNow();

        FString ReportDirectory;
        FString Error;
        if (!FGamePlatformValidationService::WriteReports(
            Summary,
            ReportDirectory,
            Error))
        {
            UE_LOG(LogTemp, Error, TEXT("架构验证报告写入失败：%s"), *Error);
            return;
        }

        UE_LOG(
            LogTemp,
            Display,
            TEXT("架构验证完成：RunId=%s Report=%s"),
            *Summary.RunId,
            *ReportDirectory);
    }

    void RunGlobalAssetValidation()
    {
        FGamePlatformValidationRunSummary Summary;
        Summary.RunId = FGamePlatformValidationService::CreateRunId();
        Summary.StartedAt = FDateTime::UtcNow();

        FGamePlatformGlobalAssetValidator::ValidateProject(Summary.Results);

        FGamePlatformValidationService::ApplyAllowlist(Summary.Results);

        Summary.FinishedAt = FDateTime::UtcNow();

        FString ReportDirectory;
        FString Error;
        if (!FGamePlatformValidationService::WriteReports(
            Summary,
            ReportDirectory,
            Error))
        {
            UE_LOG(LogTemp, Error, TEXT("全局资产验证报告写入失败：%s"), *Error);
            return;
        }

        UE_LOG(
            LogTemp,
            Display,
            TEXT("全局资产验证完成：RunId=%s Report=%s"),
            *Summary.RunId,
            *ReportDirectory);
    }

    void ValidateServerSafeAssets()
    {
        TArray<FAssetData> ServerAssets;
        for (const FAssetData& Asset : GetProjectAssets())
        {
            const FString Package = Asset.PackageName.ToString();
            if (Package.Contains(TEXT("/Server/"))
                || Package.Contains(TEXT("/ServerSafe/")))
            {
                ServerAssets.Add(Asset);
            }
        }

        RunDataValidation(ServerAssets, true);
    }

    void ValidateClientDomainAssets()
    {
        TArray<FAssetData> ClientAssets;
        for (const FAssetData& Asset : GetProjectAssets())
        {
            const FString Package = Asset.PackageName.ToString();
            if (Package.Contains(TEXT("/Client/"))
                || Package.Contains(TEXT("/Presentation/"))
                || Package.Contains(TEXT("/UI/"))
                || Package.Contains(TEXT("/VFX/"))
                || Package.Contains(TEXT("/SFX/")))
            {
                ClientAssets.Add(Asset);
            }
        }

        RunDataValidation(ClientAssets, true);
    }

    void RunSelectedReviewCases()
    {
        const TArray<FAssetData> SelectedAssets = GetSelectedAssets();

        FGamePlatformValidationRunSummary Summary;
        Summary.RunId = FGamePlatformValidationService::CreateRunId();
        Summary.StartedAt = FDateTime::UtcNow();

        UGamePlatformReviewSubsystem* ReviewSubsystem =
            GEditor
                ? GEditor->GetEditorSubsystem<UGamePlatformReviewSubsystem>()
                : nullptr;

        int32 CaseCount = 0;
        for (const FAssetData& AssetData : SelectedAssets)
        {
            UGamePlatformReviewCase* ReviewCase =
                Cast<UGamePlatformReviewCase>(AssetData.GetAsset());
            if (!ReviewCase)
            {
                continue;
            }

            ++CaseCount;
            FGamePlatformValidationResult Result;
            Result.RuleId = TEXT("GP.ManualReview");
            Result.Target = ReviewCase->CaseId.IsNone()
                ? AssetData.PackageName.ToString()
                : ReviewCase->CaseId.ToString();

            FString CaseError;
            if (!ReviewCase->IsCaseComplete(CaseError))
            {
                Result.Status = EGamePlatformValidationStatus::Failed;
                Result.Message = TEXT("Review Case（人工验收用例）不完整：") + CaseError;
                Result.SuggestedFix = TEXT("补齐CaseId/Category/Description/Steps/ExpectedResult/RequiredEvidence。");
                Summary.Results.Add(MoveTemp(Result));
                continue;
            }

            if (!ReviewSubsystem)
            {
                Result.Status = EGamePlatformValidationStatus::Failed;
                Result.Message = TEXT("UGamePlatformReviewSubsystem（人工验收子系统）不可用。");
                Summary.Results.Add(MoveTemp(Result));
                continue;
            }

            UGamePlatformReviewReport* Pending =
                ReviewSubsystem->CreatePendingReport(
                    ReviewCase,
                    Summary.RunId,
                    FApp::GetBuildVersion(),
                    TEXT("non-git-unavailable"));

            if (!Pending)
            {
                Result.Status = EGamePlatformValidationStatus::Failed;
                Result.Message = TEXT("无法创建待人工Review Report（验收报告）。");
                Summary.Results.Add(MoveTemp(Result));
                continue;
            }

            Result.Status = EGamePlatformValidationStatus::NotRun;
            Result.Message = TEXT("Review Report已创建为NotRun（待人工）；真实Reviewer完成前不得改为Passed。");
            Result.Evidence = FString::Join(ReviewCase->RequiredEvidence, TEXT("; "));
            Summary.Results.Add(MoveTemp(Result));
        }

        if (CaseCount == 0)
        {
            UE_LOG(
                LogTemp,
                Warning,
                TEXT("Run Review Cases：请先在Content Browser（内容浏览器）选择UGamePlatformReviewCase资产。"));
            return;
        }

        Summary.FinishedAt = FDateTime::UtcNow();

        FString ReportDirectory;
        FString Error;
        if (!FGamePlatformValidationService::WriteReports(
            Summary,
            ReportDirectory,
            Error))
        {
            UE_LOG(LogTemp, Error, TEXT("人工验收待办报告写入失败：%s"), *Error);
            return;
        }

        UE_LOG(
            LogTemp,
            Display,
            TEXT("已生成%d个人工验收待办，状态保持NotRun：%s"),
            CaseCount,
            *ReportDirectory);
    }

    void RunSelectedPerformanceProfiles()
    {
        const TArray<FAssetData> SelectedAssets = GetSelectedAssets();

        FGamePlatformValidationRunSummary Summary;
        Summary.RunId = FGamePlatformValidationService::CreateRunId();
        Summary.StartedAt = FDateTime::UtcNow();

        TArray<FGamePlatformPerformanceRunResult> CompletedRuns;
        int32 ProfileCount = 0;

        UGamePlatformPerformanceTestRunner* Runner =
            NewObject<UGamePlatformPerformanceTestRunner>(GetTransientPackage());

        for (const FAssetData& AssetData : SelectedAssets)
        {
            UGamePlatformPerformanceProfileAsset* ProfileAsset =
                Cast<UGamePlatformPerformanceProfileAsset>(AssetData.GetAsset());
            if (!ProfileAsset)
            {
                continue;
            }

            ++ProfileCount;
            const FGamePlatformPerformanceProfile& Profile = ProfileAsset->Profile;

            FGamePlatformValidationResult Result;
            Result.RuleId = TEXT("GP.Performance");
            Result.Target = Profile.ScenarioId.IsNone()
                ? AssetData.PackageName.ToString()
                : Profile.ScenarioId.ToString();

            FString Error;
            if (!Runner->IsProfileValid(Profile, Error))
            {
                Result.Status = EGamePlatformValidationStatus::Failed;
                Result.Message = TEXT("Performance Profile（性能场景配置）无效：") + Error;
                Summary.Results.Add(MoveTemp(Result));
                continue;
            }

            if (!FGamePlatformPerformanceExecutorRegistry::IsRegistered())
            {
                Result.Status = EGamePlatformValidationStatus::NotRun;
                Result.Message =
                    TEXT("Performance Profile有效，但当前未注册项目层Scenario Executor（场景执行器），因此未执行真实性能场景。");
                Summary.Results.Add(MoveTemp(Result));
                continue;
            }

            FGamePlatformPerformanceRunResult RunResult;
            const FString HardwareProfile = FString::Printf(
                TEXT("%s | %s"),
                *FPlatformMisc::GetCPUBrand(),
                *FPlatformMisc::GetPrimaryGPUBrand());

            const bool bRan = Runner->RunProfile(
                Profile,
                FApp::GetBuildVersion(),
                TEXT("non-git-unavailable"),
                FEngineVersion::Current().ToString(),
                HardwareProfile,
                [](const FGamePlatformPerformanceProfile& InProfile,
                   TMap<FName, TArray<double>>& OutSamples,
                   FString& OutError)
                {
                    return FGamePlatformPerformanceExecutorRegistry::Execute(
                        InProfile,
                        OutSamples,
                        OutError);
                },
                RunResult,
                Error);

            if (!bRan)
            {
                Result.Status = EGamePlatformValidationStatus::Failed;
                Result.Message = TEXT("性能场景执行失败：") + Error;
                Summary.Results.Add(MoveTemp(Result));
                continue;
            }

            Result.Status = EGamePlatformValidationStatus::Passed;
            Result.Message = FString::Printf(
                TEXT("性能场景已真实执行并采集%d类指标。"),
                RunResult.Metrics.Num());
            Result.Evidence = TEXT("performance.json");
            Summary.Results.Add(MoveTemp(Result));
            CompletedRuns.Add(MoveTemp(RunResult));
        }

        if (ProfileCount == 0)
        {
            UE_LOG(
                LogTemp,
                Warning,
                TEXT("Performance Tests：请先选择UGamePlatformPerformanceProfileAsset（性能场景配置资产）。"));
            return;
        }

        Summary.FinishedAt = FDateTime::UtcNow();

        FString ReportDirectory;
        FString ReportError;
        if (!FGamePlatformValidationService::WriteReports(
            Summary,
            ReportDirectory,
            ReportError))
        {
            UE_LOG(LogTemp, Error, TEXT("性能验证报告写入失败：%s"), *ReportError);
            return;
        }

        for (int32 Index = 0; Index < CompletedRuns.Num(); ++Index)
        {
            const FString FileName =
                Index == 0
                    ? TEXT("performance.json")
                    : FString::Printf(
                        TEXT("performance-%s.json"),
                        *CompletedRuns[Index].ScenarioId.ToString());

            FString Error;
            if (!Runner->WriteResultJson(
                CompletedRuns[Index],
                ReportDirectory / FileName,
                Error))
            {
                UE_LOG(LogTemp, Error, TEXT("性能结果写入失败：%s"), *Error);
            }
        }

        UE_LOG(
            LogTemp,
            Display,
            TEXT("Performance Tests处理%d个Profile；真实执行数=%d；报告=%s"),
            ProfileCount,
            CompletedRuns.Num(),
            *ReportDirectory);
    }

    void OpenLatestReport()
    {
        const FString Root = FGamePlatformValidationService::GetReportRoot();
        TArray<FString> Directories;
        IFileManager::Get().FindFiles(
            Directories,
            *(Root / TEXT("*")),
            false,
            true);

        if (Directories.IsEmpty())
        {
            UE_LOG(LogTemp, Warning, TEXT("暂无 GamePlatformValidation（游戏平台验证）报告。"));
            return;
        }

        Directories.Sort([](const FString& A, const FString& B)
        {
            return A > B;
        });

        const FString Latest = Root / Directories[0];
        FPlatformProcess::ExploreFolder(*Latest);
    }

    /** 中立编辑器演示入口：配置由主项目提供，平台插件不认识具体项目地图或登录类型。 */
    static void OpenPluginDemoHub()
    {
        FString MapLongPath;
        FString DevelopmentSwitch;
        FString MapArgumentName;
        if (!GConfig ||
            !GConfig->GetString(TEXT("GamePlatform.PluginDemo"), TEXT("HubMap"), MapLongPath, GEditorIni) ||
            !GConfig->GetString(TEXT("GamePlatform.PluginDemo"), TEXT("OptInSwitch"), DevelopmentSwitch, GEditorIni) ||
            !GConfig->GetString(TEXT("GamePlatform.PluginDemo"), TEXT("MapArgumentKey"), MapArgumentName, GEditorIni) ||
            MapLongPath.IsEmpty() || DevelopmentSwitch.IsEmpty() || MapArgumentName.IsEmpty())
        {
            UE_LOG(LogTemp, Error, TEXT("当前项目未配置完整插件演示入口；保持原编辑器与登录行为。"));
            return;
        }

        // 编辑器快捷入口必须和开发客户端共享双重显式资格，不能在普通编辑器误启动免登录流程。
        FString RequestedMap;
        const FString MapArgument = MapArgumentName + TEXT("=");
        if (!FParse::Param(FCommandLine::Get(), *DevelopmentSwitch) ||
            !FParse::Value(FCommandLine::Get(), *MapArgument, RequestedMap) ||
            RequestedMap != MapLongPath)
        {
            UE_LOG(LogTemp, Error, TEXT("未以开发演示专用启动参数打开Editor，拒绝自动PIE：%s"), *MapLongPath);
            return;
        }
        FString MapFilename;
        if (!FPackageName::IsValidLongPackageName(MapLongPath) ||
            !FPackageName::TryConvertLongPackageNameToFilename(
                MapLongPath, MapFilename, FPackageName::GetMapPackageExtension()) ||
            !FPaths::FileExists(MapFilename))
        {
            UE_LOG(LogTemp, Error, TEXT("演示总览地图未由UE生成或路径无效：%s"), *MapLongPath);
            return;
        }
        if (!GEditor || !FEditorFileUtils::LoadMap(MapFilename, false, true))
        {
            UE_LOG(LogTemp, Error, TEXT("UE编辑器无法打开演示总览地图：%s"), *MapLongPath);
            return;
        }
        // 仅用户主动点击菜单后提交PIE请求，不在模块启动时加载地图、自动登录或抢占现有Editor。
        GEditor->RequestPlaySession(FRequestPlaySessionParams());
    }

    void ExecuteMenuAction(FString ActionName)
    {
        if (ActionName == TEXT("OpenPluginDemoHub"))
        {
            OpenPluginDemoHub();
            return;
        }
        if (ActionName == TEXT("ValidateSelected"))
        {
            RunDataValidation(GetSelectedAssets(), false);
        }
        else if (ActionName == TEXT("ValidateFolder"))
        {
            RunDataValidation(GetSelectedFolderAssets(), false);
        }
        else if (ActionName == TEXT("ValidateProject"))
        {
            RunDataValidation(GetProjectAssets(), false);
            RunGlobalAssetValidation();
        }
        else if (ActionName == TEXT("ValidateArchitecture"))
        {
            RunArchitectureValidation();
        }
        else if (ActionName == TEXT("AuditServerAssets"))
        {
            ValidateServerSafeAssets();
        }
        else if (ActionName == TEXT("AuditClientAssets"))
        {
            ValidateClientDomainAssets();
        }
        else if (ActionName == TEXT("OpenLatestReport"))
        {
            OpenLatestReport();
        }
        else if (ActionName == TEXT("RunReviewCases"))
        {
            RunSelectedReviewCases();
        }
        else if (ActionName == TEXT("PerformanceTests"))
        {
            RunSelectedPerformanceProfiles();
        }
    }
};

IMPLEMENT_MODULE(FGamePlatformDeveloperToolsModule, GamePlatformDeveloperTools)

#undef LOCTEXT_NAMESPACE
