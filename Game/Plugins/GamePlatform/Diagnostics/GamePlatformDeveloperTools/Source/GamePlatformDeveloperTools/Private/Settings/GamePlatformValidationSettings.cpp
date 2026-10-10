// 平台Editor验证默认值与确定性前缀解析；读取引擎反射，不引用项目类型，也不链接可选GameplayAbilities实现。
#include "Settings/GamePlatformValidationSettings.h"
#include "Engine/Blueprint.h"

UGamePlatformValidationSettings::UGamePlatformValidationSettings()
{
    CategoryName = TEXT("Game Platform");
    SectionName = TEXT("Validation");

    AssetClassPrefixes =
    {
        { TEXT("Texture"), TEXT("T_") },
        { TEXT("MaterialInstance"), TEXT("MI_") },
        { TEXT("Material"), TEXT("M_") },
        { TEXT("StaticMesh"), TEXT("SM_") },
        { TEXT("SkeletalMesh"), TEXT("SK_") },
        { TEXT("NiagaraSystem"), TEXT("NS_") },
        { TEXT("AnimBlueprint"), TEXT("ABP_") },
        { TEXT("WidgetBlueprint"), TEXT("WBP_") },
        { TEXT("GameplayAbility"), TEXT("GA_") },
        { TEXT("GameplayEffect"), TEXT("GE_") },
        { TEXT("Blueprint"), TEXT("BP_") },
        { TEXT("Sound"), TEXT("S_") }
    };

    RemovedGameplayTagPrefixes =
    {
        TEXT("FiveCamp."),
        TEXT("Faction."),
        TEXT("Element."),
        TEXT("KingSeal.")
    };

    ForbiddenServerDependencyPathTokens =
    {
        TEXT("/Client/"),
        TEXT("/Presentation/"),
        TEXT("/UI/"),
        TEXT("/VFX/"),
        TEXT("/SFX/"),
        TEXT("/Cinematic/"),
        TEXT("/CommerceUI/"),
        TEXT("/GamePlatformDebug/")
    };
}

FString UGamePlatformValidationSettings::ResolveAssetPrefix(const UObject* Asset) const
{
    if (!Asset) return FString();

    if (const UBlueprint* Blueprint = Cast<UBlueprint>(Asset))
    {
        // 专用蓝图表达资产领域，必须优先于其运行时父类；逐类检查也覆盖自定义Anim/Widget蓝图资产派生。
        static const TCHAR* BlueprintAssetKeys[] = { TEXT("AnimBlueprint"), TEXT("WidgetBlueprint") };
        for (const TCHAR* Key : BlueprintAssetKeys)
        {
            for (const UClass* AssetClass = Asset->GetClass(); AssetClass; AssetClass = AssetClass->GetSuperClass())
            {
                if (AssetClass->GetName() == Key)
                {
                    if (const FString* Prefix = AssetClassPrefixes.Find(Key)) return *Prefix;
                }
            }
        }

        // 从最近父类向上匹配精确身份，原生中间派生与蓝图中间派生均不丢失领域。
        // 完整路径消除同名类歧义；配置短类名保留跨游戏默认值，不按数组或哈希偶然顺序选规则。
        for (const UClass* ParentClass = Blueprint->ParentClass; ParentClass; ParentClass = ParentClass->GetSuperClass())
        {
            if (const FString* Prefix = AssetClassPrefixes.Find(ParentClass->GetPathName())) return *Prefix;
            if (const FString* Prefix = AssetClassPrefixes.Find(ParentClass->GetName())) return *Prefix;
        }
        if (const FString* Prefix = AssetClassPrefixes.Find(TEXT("Blueprint"))) return *Prefix;
        return FString();
    }

    // 其他既有资产保留类型关键字合同；具体类型先于宽泛类型，避免MI被Material、SK被普通Mesh规则抢占。
    static const TCHAR* PriorityKeys[] =
    {
        TEXT("MaterialInstance"), TEXT("NiagaraSystem"), TEXT("SkeletalMesh"),
        TEXT("StaticMesh"), TEXT("Texture"), TEXT("Material"), TEXT("Sound")
    };
    const FString ClassName = Asset->GetClass()->GetName();
    for (const TCHAR* Key : PriorityKeys)
    {
        if (ClassName.Contains(Key))
        {
            if (const FString* Prefix = AssetClassPrefixes.Find(Key)) return *Prefix;
        }
    }
    return FString();
}
