# PlayerProfile（玩家资料）

登录成功后通过 Gateway→PlayerDataService 加载 FDivineBeastsPlayerProfile。

第一版资料字段：PlayerId、ProfileRevision、OnboardingState、LastSelectedCharacterId、LastExperienceId、LastWorldId。

CurrentHP、Buff、Cooldown、MatchGold 等实时状态不得进入长期 Profile。
