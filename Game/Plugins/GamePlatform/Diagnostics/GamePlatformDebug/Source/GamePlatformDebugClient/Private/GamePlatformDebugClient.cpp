#include "Modules/ModuleManager.h"

#if !UE_BUILD_SHIPPING

#include "Registry/GamePlatformDebugRegistry.h"

#include "Containers/Ticker.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformApplicationMisc.h"
#include "HAL/PlatformTime.h"
#include "Input/Reply.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
    UWorld* ResolvePanelWorld()
    {
        if (GEngine && GEngine->GameViewport)
        {
            if (UWorld* World = GEngine->GameViewport->GetWorld())
            {
                return World;
            }
        }

        if (GEngine)
        {
            for (const FWorldContext& Context : GEngine->GetWorldContexts())
            {
                if (Context.World() &&
                    (Context.WorldType == EWorldType::PIE ||
                     Context.WorldType == EWorldType::Game))
                {
                    return Context.World();
                }
            }
        }

        return nullptr;
    }

    class SGamePlatformDebugPanel final : public SCompoundWidget
    {
    public:
        SLATE_BEGIN_ARGS(SGamePlatformDebugPanel) {}
        SLATE_END_ARGS()

        void Construct(const FArguments&)
        {
            Categories =
            {
                TEXT("World"),
                TEXT("Character"),
                TEXT("Ability"),
                TEXT("Combat"),
                TEXT("AI"),
                TEXT("Navigation"),
                TEXT("Network"),
                TEXT("Online"),
                TEXT("Session"),
                TEXT("Loading"),
                TEXT("Telemetry")
            };

            RefreshRatesHz = { 0.0, 1.0, 5.0, 10.0 };
            RefreshRateIndex = 1;

            TSharedRef<SHorizontalBox> CategoryTabs = SNew(SHorizontalBox);
            for (int32 Index = 0; Index < Categories.Num(); ++Index)
            {
                const FName Category = Categories[Index];
                CategoryTabs->AddSlot()
                    .AutoWidth()
                    .Padding(2.0f)
                    [
                        SNew(SButton)
                        .Text(FText::FromName(Category))
                        .OnClicked_Lambda(
                            [this, Index]()
                            {
                                SelectedCategoryIndex = Index;
                                RefreshNow();
                                return FReply::Handled();
                            })
                    ];
            }

            ChildSlot
            [
                SNew(SBorder)
                .Padding(8.0f)
                [
                    SNew(SVerticalBox)

                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(
                            TEXT("GamePlatformDebug｜游戏平台调试面板（Development/Test）")))
                    ]

                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 6.0f)
                    [
                        CategoryTabs
                    ]

                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 4.0f)
                    [
                        SNew(SHorizontalBox)

                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .Padding(2.0f)
                        [
                            SNew(SButton)
                            .Text_Lambda(
                                [this]()
                                {
                                    return FText::FromString(
                                        bUseViewTarget
                                            ? TEXT("目标：ViewTarget（视图目标）")
                                            : TEXT("目标：LocalPawn（本地角色）"));
                                })
                            .OnClicked_Lambda(
                                [this]()
                                {
                                    bUseViewTarget = !bUseViewTarget;
                                    RefreshNow();
                                    return FReply::Handled();
                                })
                        ]

                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .Padding(2.0f)
                        [
                            SNew(SButton)
                            .Text_Lambda(
                                [this]()
                                {
                                    const double Hz =
                                        RefreshRatesHz.IsValidIndex(RefreshRateIndex)
                                            ? RefreshRatesHz[RefreshRateIndex]
                                            : 1.0;
                                    return FText::FromString(
                                        Hz <= 0.0
                                            ? TEXT("刷新：Manual（手动）")
                                            : FString::Printf(TEXT("刷新：%.0fHz"), Hz));
                                })
                            .OnClicked_Lambda(
                                [this]()
                                {
                                    RefreshRateIndex =
                                        (RefreshRateIndex + 1) % RefreshRatesHz.Num();
                                    LastRefreshSeconds = 0.0;
                                    return FReply::Handled();
                                })
                        ]

                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .Padding(2.0f)
                        [
                            SNew(SButton)
                            .Text_Lambda(
                                [this]()
                                {
                                    return FText::FromString(
                                        bPaused ? TEXT("继续") : TEXT("暂停"));
                                })
                            .OnClicked_Lambda(
                                [this]()
                                {
                                    bPaused = !bPaused;
                                    return FReply::Handled();
                                })
                        ]

                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .Padding(2.0f)
                        [
                            SNew(SButton)
                            .Text(FText::FromString(TEXT("立即刷新")))
                            .OnClicked_Lambda(
                                [this]()
                                {
                                    RefreshNow();
                                    return FReply::Handled();
                                })
                        ]

                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .Padding(2.0f)
                        [
                            SNew(SButton)
                            .Text(FText::FromString(TEXT("复制脱敏摘要")))
                            .OnClicked_Lambda(
                                [this]()
                                {
                                    const FString Combined =
                                        ClientViewText + TEXT("\n\n") + ServerViewText;
                                    FPlatformApplicationMisc::ClipboardCopy(*Combined);
                                    return FReply::Handled();
                                })
                        ]

                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .Padding(2.0f)
                        [
                            SNew(SButton)
                            .Text_Lambda(
                                [this]()
                                {
                                    return FText::FromString(
                                        bOverlayDraw
                                            ? TEXT("Overlay Draw：开")
                                            : TEXT("Overlay Draw：关"));
                                })
                            .OnClicked_Lambda(
                                [this]()
                                {
                                    bOverlayDraw = !bOverlayDraw;
                                    return FReply::Handled();
                                })
                        ]
                    ]

                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(2.0f)
                    [
                        SNew(SSearchBox)
                        .HintText(FText::FromString(TEXT("搜索字段")))
                        .OnTextChanged_Lambda(
                            [this](const FText& Text)
                            {
                                SearchText = Text.ToString();
                                RefreshNow();
                            })
                    ]

                    + SVerticalBox::Slot()
                    .FillHeight(1.0f)
                    .Padding(0.0f, 6.0f)
                    [
                        SNew(SHorizontalBox)

                        + SHorizontalBox::Slot()
                        .FillWidth(1.0f)
                        .Padding(2.0f)
                        [
                            SNew(SScrollBox)
                            + SScrollBox::Slot()
                            [
                                SAssignNew(ClientTextWidget, STextBlock)
                                .Text(FText::FromString(TEXT("Client View（客户端视角）")))
                                .AutoWrapText(true)
                            ]
                        ]

                        + SHorizontalBox::Slot()
                        .FillWidth(1.0f)
                        .Padding(2.0f)
                        [
                            SNew(SScrollBox)
                            + SScrollBox::Slot()
                            [
                                SAssignNew(ServerTextWidget, STextBlock)
                                .Text(FText::FromString(TEXT("Server View（服务器权威视角）")))
                                .AutoWrapText(true)
                            ]
                        ]
                    ]
                ]
            ];

            RefreshNow();
        }

        bool TickRefresh(float)
        {
            if (bPaused)
            {
                return true;
            }

            const double Hz =
                RefreshRatesHz.IsValidIndex(RefreshRateIndex)
                    ? RefreshRatesHz[RefreshRateIndex]
                    : 1.0;

            if (Hz <= 0.0)
            {
                return true;
            }

            const double Now = FPlatformTime::Seconds();
            if (LastRefreshSeconds <= 0.0 ||
                Now - LastRefreshSeconds >= (1.0 / Hz))
            {
                RefreshNow();
            }
            return true;
        }

    private:
        AActor* ResolveTarget(UWorld* World) const
        {
            APlayerController* PC = World
                ? World->GetFirstPlayerController()
                : nullptr;
            if (!PC)
            {
                return nullptr;
            }

            if (bUseViewTarget)
            {
                return PC->GetViewTarget();
            }

            return PC->GetPawn();
        }

        FString BuildFilteredText(
            const TCHAR* Heading,
            const FGamePlatformDebugSnapshot& Snapshot) const
        {
            FString Result = FString::Printf(
                TEXT("%s\nCategory=%s\nRevision=%llu\nTarget=%s\n"),
                Heading,
                *Snapshot.CategoryId.ToString(),
                Snapshot.Revision,
                *Snapshot.TargetId.ToString(EGuidFormats::DigitsWithHyphens));

            if (!Snapshot.FailureReason.IsEmpty())
            {
                Result += FString::Printf(
                    TEXT("Failure=%s\n"),
                    *Snapshot.FailureReason);
            }

            for (const FGamePlatformDebugField& Field : Snapshot.Fields)
            {
                if (!SearchText.IsEmpty() &&
                    !Field.Key.ToString().Contains(SearchText) &&
                    !Field.DisplayName.Contains(SearchText) &&
                    !Field.Value.Contains(SearchText))
                {
                    continue;
                }

                Result += FString::Printf(
                    TEXT("%s：%s\n"),
                    *Field.DisplayName,
                    *Field.Value);
            }

            if (Snapshot.bTruncated)
            {
                Result += TEXT("[已按安全上限截断]\n");
            }

            return Result;
        }

        void RefreshNow()
        {
            LastRefreshSeconds = FPlatformTime::Seconds();

            UWorld* World = ResolvePanelWorld();
            if (!World || !Categories.IsValidIndex(SelectedCategoryIndex))
            {
                ClientViewText = TEXT("Client View（客户端视角）\nN/A：没有活动游戏世界。");
                ServerViewText = TEXT("Server View（服务器权威视角）\nN/A：没有活动游戏世界。");
                UpdateWidgets();
                return;
            }

            AActor* Target = ResolveTarget(World);
            const FName ProviderId = Categories[SelectedCategoryIndex];

            FGamePlatformDebugCollectContext ClientContext;
            ClientContext.World = World;
            ClientContext.Target = Target;
            ClientContext.SourceView = EGamePlatformDebugSourceView::Client;
            ClientContext.RequestId = FGuid::NewGuid();
            ClientContext.Filter = SearchText;

            const double Hz =
                RefreshRatesHz.IsValidIndex(RefreshRateIndex)
                    ? RefreshRatesHz[RefreshRateIndex]
                    : 1.0;
            ClientContext.bAllowExpensive = Hz <= 1.0;

            FGamePlatformDebugRegistry& Registry =
                FGamePlatformDebugRegistry::Get();
            FGamePlatformDebugSnapshot ClientSnapshot;
            Registry.CollectSnapshot(
                ProviderId,
                ClientContext,
                ClientSnapshot);

            ClientViewText =
                BuildFilteredText(
                    TEXT("Client View（客户端视角）"),
                    ClientSnapshot);

            if (World->GetNetMode() != NM_Client)
            {
                FGamePlatformDebugCollectContext ServerContext = ClientContext;
                ServerContext.SourceView = EGamePlatformDebugSourceView::Server;
                ServerContext.RequestId = FGuid::NewGuid();

                FGamePlatformDebugSnapshot ServerSnapshot;
                Registry.CollectSnapshot(
                    ProviderId,
                    ServerContext,
                    ServerSnapshot);

                ServerViewText =
                    BuildFilteredText(
                        TEXT("Server View（服务器权威视角）"),
                        ServerSnapshot);
            }
            else
            {
                ServerViewText =
                    TEXT("Server View（服务器权威视角）\n")
                    TEXT("由 Gameplay Debugger（玩法调试器）的 GP.* 分类在权威端采集并复制。\n")
                    TEXT("V1 不开放任意 Remote Debug RPC（远程调试调用），避免扩大攻击面。\n")
                    TEXT("使用 ' 键启用 Gameplay Debugger，并选择对应 GP.* 分类查看服务器视角。");
            }

            if (bOverlayDraw && Target)
            {
                const FBox Bounds = Target->GetComponentsBoundingBox(true, false);
                DrawDebugBox(
                    World,
                    Bounds.GetCenter(),
                    Bounds.GetExtent(),
                    FColor::Cyan,
                    false,
                    0.12f,
                    0,
                    1.0f);
            }

            UpdateWidgets();
        }

        void UpdateWidgets()
        {
            if (ClientTextWidget.IsValid())
            {
                ClientTextWidget->SetText(
                    FText::FromString(ClientViewText));
            }
            if (ServerTextWidget.IsValid())
            {
                ServerTextWidget->SetText(
                    FText::FromString(ServerViewText));
            }
        }

        TArray<FName> Categories;
        TArray<double> RefreshRatesHz;
        int32 SelectedCategoryIndex = 0;
        int32 RefreshRateIndex = 1;
        bool bPaused = false;
        bool bUseViewTarget = false;
        bool bOverlayDraw = false;
        double LastRefreshSeconds = 0.0;
        FString SearchText;
        FString ClientViewText;
        FString ServerViewText;
        TSharedPtr<STextBlock> ClientTextWidget;
        TSharedPtr<STextBlock> ServerTextWidget;
    };

    class FGamePlatformDebugClientModule final : public IModuleInterface
    {
    public:
        virtual void StartupModule() override
        {
            RegisterPanelCommand();
        }

        virtual void ShutdownModule() override
        {
            ClosePanel();
            if (PanelConsoleObject)
            {
                IConsoleManager::Get().UnregisterConsoleObject(
                    PanelConsoleObject,
                    false);
                PanelConsoleObject = nullptr;
            }
            FGamePlatformDebugRegistry::Get().UnregisterCommandDescriptor(
                TEXT("gp.Debug.Panel"));
        }

    private:
        void RegisterPanelCommand()
        {
            PanelConsoleObject =
                IConsoleManager::Get().RegisterConsoleCommand(
                    TEXT("gp.Debug.Panel"),
                    TEXT("Toggle Development/Test client debug panel."),
                    FConsoleCommandDelegate::CreateLambda(
                        [this]()
                        {
                            TogglePanel();
                        }),
                    ECVF_Default);

            FGamePlatformDebugCommandDescriptor Descriptor;
            Descriptor.Name = TEXT("gp.Debug.Panel");
            Descriptor.Help =
                TEXT("Toggle Development/Test client debug panel.");
            Descriptor.Category = TEXT("ClientPanel");
            Descriptor.Args = TEXT("");
            Descriptor.Kind = EGamePlatformDebugCommandKind::Mutating;
            Descriptor.RequiredPrivilege = TEXT("Developer");
            Descriptor.AllowedBuilds =
            {
                TEXT("DebugGame"),
                TEXT("Development"),
                TEXT("Test")
            };
            FGamePlatformDebugRegistry::Get().RegisterCommandDescriptor(
                Descriptor);
        }

        void TogglePanel()
        {
            if (Panel.IsValid())
            {
                ClosePanel();
            }
            else
            {
                OpenPanel();
            }
        }

        void OpenPanel()
        {
            if (!GEngine ||
                !GEngine->GameViewport ||
                Panel.IsValid())
            {
                return;
            }

            SAssignNew(Panel, SGamePlatformDebugPanel);
            GEngine->GameViewport->AddViewportWidgetContent(
                Panel.ToSharedRef(),
                10000);

            TWeakPtr<SGamePlatformDebugPanel> WeakPanel = Panel;
            TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
                FTickerDelegate::CreateLambda(
                    [WeakPanel](float DeltaSeconds)
                    {
                        if (TSharedPtr<SGamePlatformDebugPanel> Pinned =
                                WeakPanel.Pin())
                        {
                            return Pinned->TickRefresh(DeltaSeconds);
                        }
                        return false;
                    }),
                0.1f);
        }

        void ClosePanel()
        {
            if (TickerHandle.IsValid())
            {
                FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
                TickerHandle.Reset();
            }

            if (Panel.IsValid() &&
                GEngine &&
                GEngine->GameViewport)
            {
                GEngine->GameViewport->RemoveViewportWidgetContent(
                    Panel.ToSharedRef());
            }

            Panel.Reset();
        }

        TSharedPtr<SGamePlatformDebugPanel> Panel;
        FTSTicker::FDelegateHandle TickerHandle;
        IConsoleObject* PanelConsoleObject = nullptr;
    };
}

IMPLEMENT_MODULE(FGamePlatformDebugClientModule, GamePlatformDebugClient)

#else

IMPLEMENT_MODULE(FDefaultModuleImpl, GamePlatformDebugClient)

#endif
