#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GamePlatformLoadingScreenService.generated.h"

USTRUCT(BlueprintType)
struct GAMEPLATFORMUICLIENT_API FGamePlatformLoadingToken
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="UI|Loading")
    FGuid Id;

    bool IsValid() const { return Id.IsValid(); }
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMUICLIENT_API FGamePlatformUILoadingSnapshot
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="UI|Loading")
    bool bIsLoading = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="UI|Loading")
    int32 ActiveTokenCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="UI|Loading")
    FText Stage;

    /** 小于0表示权威进度未知，UI不得伪造0~100。 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="UI|Loading")
    float Progress = -1.0f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FGamePlatformUILoadingSnapshotChanged,
    const FGamePlatformUILoadingSnapshot&, Snapshot);

/** 多事务 Loading Token 聚合服务。 */
UCLASS(BlueprintType)
class GAMEPLATFORMUICLIENT_API UGamePlatformLoadingScreenService : public UObject
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category="UI|Loading")
    FGamePlatformLoadingToken AcquireToken(FText Stage, float Progress = -1.0f);

    UFUNCTION(BlueprintCallable, Category="UI|Loading")
    bool UpdateToken(FGamePlatformLoadingToken Token, FText Stage, float Progress = -1.0f);

    UFUNCTION(BlueprintCallable, Category="UI|Loading")
    bool ReleaseToken(FGamePlatformLoadingToken Token);

    UFUNCTION(BlueprintCallable, Category="UI|Loading")
    void ReleaseAll();

    UFUNCTION(BlueprintPure, Category="UI|Loading")
    FGamePlatformUILoadingSnapshot GetSnapshot() const;

    UPROPERTY(BlueprintAssignable, Category="UI|Loading")
    FGamePlatformUILoadingSnapshotChanged OnSnapshotChanged;

private:
    struct FTokenState
    {
        FText Stage;
        float Progress = -1.0f;
        uint64 Sequence = 0;
    };

    TMap<FGuid, FTokenState> Tokens;
    uint64 NextSequence = 1;

    void BroadcastSnapshot();
};
