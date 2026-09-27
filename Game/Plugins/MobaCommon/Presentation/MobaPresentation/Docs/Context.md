# Context（上下文）

FMobaPresentationContext（MOBA表现上下文）使用明确字段，而不是 TMap<FName,FString>（通用字符串字典）承载核心语义。

字段覆盖MatchId、ArenaModeId、TeamId、Source/Target Entity与Character ID、HeroDefinitionId、AbilityId、StatusId、位置/法线、Magnitude（表现量）、Critical/Prediction/Confirmation、本地Source/Target标志，以及World/Avatar/Request Generation（世界/角色实例/请求世代）。

Generation用于拒绝跨Travel（切图）或旧Pawn（旧角色实例）的过期异步表现。
