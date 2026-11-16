#include "GamePlatformWaterEditorLibrary.h"
#include "GamePlatformNoCarveLakeFactory.h"
#include "AssetRegistry/AssetData.h"
#include "Editor.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "ScopedTransaction.h"
#include "Subsystems/EditorActorSubsystem.h"
#include "WaterBodyLakeActor.h"
#include "WaterBodyComponent.h"

UGamePlatformNoCarveLakeFactory::UGamePlatformNoCarveLakeFactory()
{
    bShouldAutoRegister = false;
    NewActorClass = AWaterBodyLake::StaticClass();
}

AActor* UGamePlatformNoCarveLakeFactory::SpawnActor(UObject* Asset, ULevel* Level,
    const FTransform& Transform, const FActorSpawnParameters& Parameters)
{
    if (Asset != AWaterBodyLake::StaticClass() || !IsValid(Level) || !IsValid(Level->OwningWorld)) return nullptr;
    FActorSpawnParameters Spawn = Parameters;
    Spawn.OverrideLevel = Level;
    Spawn.bDeferConstruction = true;
    AWaterBodyLake* Lake = Level->OwningWorld->SpawnActor<AWaterBodyLake>(AWaterBodyLake::StaticClass(), Transform, Spawn);
    if (!IsValid(Lake) || !IsValid(Lake->GetWaterBodyComponent())) return nullptr;
    Lake->GetWaterBodyComponent()->bAffectsLandscape = false;
    Lake->FinishSpawning(Transform, true);
    return Lake;
}

AActor* UGamePlatformWaterEditorLibrary::ReplaceWithNativeNoCarveLake(AActor* Source, FString& OutError)
{
    OutError.Reset();
    AWaterBodyLake* OldLake = Cast<AWaterBodyLake>(Source);
    UWorld* World = IsValid(OldLake) ? OldLake->GetWorld() : nullptr;
    if (!IsInGameThread() || !GEditor || !IsValid(World) || World != GEditor->GetEditorWorldContext().World() ||
        World->WorldType != EWorldType::Editor || OldLake->GetClass() == AWaterBodyLake::StaticClass() ||
        !IsValid(OldLake->GetWaterBodyComponent()) || OldLake->GetWaterBodyComponent()->bAffectsLandscape)
    {
        OutError = TEXT("仅允许替换当前编辑器世界中已禁用地形雕刻的湖体子类。");
        return nullptr;
    }
    const FGuid OriginalGuid = OldLake->GetActorGuid();
    UGamePlatformNoCarveLakeFactory* Factory = NewObject<UGamePlatformNoCarveLakeFactory>();
    TArray<AActor*> Replacements;
    const FScopedTransaction Transaction(NSLOCTEXT("GamePlatformWater", "ReplaceLake", "Replace lake with native WaterBodyLake"));
    UEditorActorSubsystem::ReplaceActors(Factory, FAssetData(AWaterBodyLake::StaticClass()), {OldLake}, &Replacements, false);
    AWaterBodyLake* Lake = Replacements.Num() == 1 ? Cast<AWaterBodyLake>(Replacements[0]) : nullptr;
    if (!IsValid(Lake) || Lake->GetClass() != AWaterBodyLake::StaticClass() || Lake->GetActorGuid() != OriginalGuid ||
        !IsValid(Lake->GetWaterBodyComponent()) || Lake->GetWaterBodyComponent()->bAffectsLandscape)
    {
        OutError = TEXT("原生替换未能确认湖体身份、GUID或禁雕刻状态；不得保存地图。");
        return nullptr;
    }
    return Lake;
}
