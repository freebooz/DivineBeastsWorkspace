// 项目客户端创建介绍软路径门禁；真实资源类型/英雄身份在Data租约成功后由VM复核。
#include "Settings/DivineBeastsUISettings.h"
#include "Catalog/DivineBeastsHeroCatalog.h"

bool UDivineBeastsUISettings::TryGetCharacterCreationProfilePath(
    FName HeroDefinitionId, FSoftObjectPath& OutPath, FString& OutError) const
{
    OutPath.Reset();
    const FSoftObjectPath* Configured = CharacterCreationProfiles.Find(HeroDefinitionId);
    if (!FDivineBeastsHeroCatalog::IsCoreHeroId(HeroDefinitionId) || !Configured || !Configured->IsValid())
    {
        OutError = TEXT("该英雄尚未配置有效创建介绍资源。");
        return false;
    }
    if (!Configured->GetLongPackageName().StartsWith(TEXT("/DBAUIPack_Core/UI/Profiles/CharacterCreation/")) ||
        !Configured->GetSubPathString().IsEmpty())
    {
        OutError = TEXT("创建介绍必须指向DBAUIPack_Core内独立Profile对象，不能读取开发技能或对象子路径。");
        return false;
    }
    OutPath = *Configured;
    OutError.Reset();
    return true;
}
