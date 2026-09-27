# CharacterCreationUI（角色创建界面）

角色创建命令字段按现有ApplicationFlow/Characters契约：

- HeroDefinitionId
- CharacterName
- AppearanceSelection

旧 Element / FiveCamp / Faction / Pantheon / KingSeal（元素/阵营/派系/神系/王印）字段没有进入UI Contract，也不得恢复。

ViewModel每次提交生成RequestId，并携带PageGeneration和ExpectedRevision；ApplicationFlow最终创建仍使用自己的OperationId（操作幂等编号），UI防双击不是业务幂等替代品。

本地 IsValid 只做基础必填校验。Hero资格、名字规则、外观合法性、持久化结果由业务Owner/服务器权威决定。
