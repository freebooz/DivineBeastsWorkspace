#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Types/GamePlatformQuestTypes.h"
#include "GamePlatformQuestStateComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(
    FGamePlatformQuestSnapshotsChanged);

UCLASS(ClassGroup=(GamePlatform), meta=(BlueprintSpawnableComponent))
class GAMEPLATFORMQUEST_API UGamePlatformQuestStateComponent final
    : public UActorComponent
{
    GENERATED_BODY()

public:
    UGamePlatformQuestStateComponent();

    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION(BlueprintPure, Category="Quest")
    const TArray<FGamePlatformQuestSnapshot>& GetQuestSnapshots() const
    {
        return QuestSnapshots;
    }

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Quest")
    void SetServerQuestSnapshots(
        const TArray<FGamePlatformQuestSnapshot>& InSnapshots);

    UPROPERTY(BlueprintAssignable, Category="Quest")
    FGamePlatformQuestSnapshotsChanged OnQuestSnapshotsChanged;

private:
    UPROPERTY(ReplicatedUsing=OnRep_QuestSnapshots)
    TArray<FGamePlatformQuestSnapshot> QuestSnapshots;

    UFUNCTION()
    void OnRep_QuestSnapshots();
};
