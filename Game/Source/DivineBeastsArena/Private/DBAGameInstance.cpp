#include "DBAGameInstance.h"
#include "Bootstrap/DBAFoundationCoordinator.h"

void UDBAGameInstance::Init()
{
    Super::Init();
    FoundationCoordinator = NewObject<UDBAFoundationCoordinator>(this);
    FoundationCoordinator->Initialize(*this);
}

void UDBAGameInstance::Shutdown()
{
    if (FoundationCoordinator) { FoundationCoordinator->Shutdown(); }
    FoundationCoordinator = nullptr;
    Super::Shutdown();
}

FString UDBAGameInstance::GetFoundationDiagnostics() const
{
    return FoundationCoordinator ? FoundationCoordinator->GetDiagnostics() : TEXT("基础工程开发验证未启用");
}
