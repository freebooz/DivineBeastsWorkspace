#include "Bootstrap/DBAFoundationHUD.h"
#include "DBAGameInstance.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

void ADBAFoundationHUD::DrawHUD()
{
    Super::DrawHUD();
#if !UE_BUILD_SHIPPING
    if (!GetWorld() || GetWorld()->GetNetMode() == NM_DedicatedServer ||
        !FParse::Param(FCommandLine::Get(), TEXT("FoundationStandalone"))) { return; }
    const UDBAGameInstance* Instance = Cast<UDBAGameInstance>(GetWorld()->GetGameInstance());
    if (Instance) { DrawText(Instance->GetFoundationDiagnostics(), FLinearColor::White, 30.f, 30.f); }
#endif
}
