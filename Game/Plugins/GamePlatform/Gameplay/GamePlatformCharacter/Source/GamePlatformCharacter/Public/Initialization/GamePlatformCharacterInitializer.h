#pragma once

#include "CoreMinimal.h"
#include "GamePlatformCharacterInitializer.generated.h"

class ACharacter;

/** FGamePlatformCharacterInitializationContext（平台角色出生初始化上下文）。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMCHARACTER_API FGamePlatformCharacterInitializationContext
{
    GENERATED_BODY()

    /** 后端持久角色档案ID；AI等非持久Avatar可为空。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString CharacterId;

    /** 已由可信后端/服务器流程校验的Hero Definition ID。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName HeroDefinitionId = NAME_None;

    /** Pawn出生代次。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 SpawnGeneration = 0;

    /** Avatar绑定代次。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 AvatarGeneration = 0;

    /** Player-controlled持久角色通常要求CharacterId；AI可设false。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bPersistentCharacterIdRequired = true;

    bool IsValid(FString& OutError) const
    {
        if (HeroDefinitionId.IsNone())
        {
            OutError = TEXT("HeroDefinitionId不能为空。");
            return false;
        }
        if (SpawnGeneration <= 0 || AvatarGeneration <= 0)
        {
            OutError = TEXT("SpawnGeneration/AvatarGeneration必须大于0。");
            return false;
        }
        if (bPersistentCharacterIdRequired && CharacterId.IsEmpty())
        {
            OutError = TEXT("持久角色初始化必须提供CharacterId。");
            return false;
        }
        return true;
    }
};

/**
 * IGamePlatformCharacterInitializer（平台角色初始化器）。
 * 由统一Spawn Operation在Actor已创建但进入Gameplay Active前调用；
 * 实现不得自行SpawnActor或Possess。
 */
class GAMEPLATFORMCHARACTER_API IGamePlatformCharacterInitializer
{
public:
    virtual ~IGamePlatformCharacterInitializer() = default;

    virtual bool InitializeCharacter(
        ACharacter& Character,
        const FGamePlatformCharacterInitializationContext& Context,
        FString& OutError) = 0;
};
