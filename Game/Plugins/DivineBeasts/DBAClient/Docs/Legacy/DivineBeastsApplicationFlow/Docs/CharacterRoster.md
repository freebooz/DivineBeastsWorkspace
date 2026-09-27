# CharacterRoster（持久角色列表）

Character Roster 来自 PlayerDataService 的 /v1/divinebeasts/characters。

FDivineBeastsCharacterSummary 明确使用 CharacterId、HeroDefinitionId、CharacterName、CharacterRevision、OnboardingState、Status。CharacterId 是玩家后端持久档案ID；HeroDefinitionId 是生肖英雄模板ID，两者不得混淆。

ArenaHeroSelection（竞技选人）属于 MainArena 单场比赛，不由本模块的 SelectPersistentCharacter 替代。
