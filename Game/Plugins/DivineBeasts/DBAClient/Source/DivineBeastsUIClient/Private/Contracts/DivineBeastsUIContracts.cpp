#include "Contracts/DivineBeastsUIContracts.h"

bool FDivineBeastsUICommand::IsValid(FString& OutError) const
{
    if (!RequestId.IsValid())
    {
        OutError = TEXT("RequestId不能为空。");
        return false;
    }
    if (PageGeneration < 0 || ExpectedRevision < 0)
    {
        OutError = TEXT("PageGeneration/ExpectedRevision不能为负数。");
        return false;
    }

    switch (Type)
    {
    case EDivineBeastsUICommandType::Login:
        if (LoginName.TrimStartAndEnd().IsEmpty() || Password.IsEmpty())
        {
            OutError = TEXT("Login命令需要账号与瞬时Password。");
            return false;
        }
        break;
    case EDivineBeastsUICommandType::CreateCharacter:
        if (HeroDefinitionId.IsNone() ||
            CharacterName.TrimStartAndEnd().IsEmpty())
        {
            OutError = TEXT("CreateCharacter命令缺少HeroDefinitionId或CharacterName。");
            return false;
        }
        break;
    case EDivineBeastsUICommandType::SelectPersistentCharacter:
        if (CharacterId.IsEmpty())
        {
            OutError = TEXT("SelectPersistentCharacter命令缺少CharacterId。");
            return false;
        }
        break;
    case EDivineBeastsUICommandType::RequestWorld:
        if (DesiredExperienceId.IsNone())
        {
            OutError = TEXT("RequestWorld命令缺少DesiredExperienceId。");
            return false;
        }
        break;
    case EDivineBeastsUICommandType::StartMatchmaking:
        if (ArenaModeId.IsNone())
        {
            OutError = TEXT("StartMatchmaking命令缺少ArenaModeId。");
            return false;
        }
        break;
    case EDivineBeastsUICommandType::ArenaSelectHero:
        if (HeroDefinitionId.IsNone())
        {
            OutError = TEXT("ArenaSelectHero命令缺少HeroDefinitionId。");
            return false;
        }
        break;
    default:
        break;
    }

    OutError.Reset();
    return true;
}
