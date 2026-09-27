# DivineBeastsCharacters（神兽联盟角色插件）

DivineBeastsCharacters位于 DivineBeasts/Gameplay（神兽联盟项目玩法扩展层），唯一模块为 DivineBeastsCharactersRuntime（生肖角色运行模块），Runtime（双端）可在 OpenWorld/Village 非MOBA环境独立使用。

依赖边界：DivineBeastsRuntime（项目核心）、GamePlatformCore（平台核心）、GamePlatformCharacter（平台角色）。不依赖 Arena、MobaPresentation、ApplicationFlow、Presentation、UI 或 DivineBeastsContracts。

本插件使用一个通用 ACharacter（角色）运行模型 + UDivineBeastsCharacterComponent（项目角色组件），不创建12套生肖C++角色类。12个稳定身份为 Hero.Zodiac.Rat/Ox/Tiger/Rabbit/Dragon/Snake/Horse/Goat/Monkey/Rooster/Dog/Boar。

项目 Hero Definition（英雄定义）继承平台 UGamePlatformHeroDefinition，只增加真实项目字段：ZodiacIdentity、ZodiacTag、DisplayNameKey、ContentPackId、AppearanceSchema。Definition不硬引用技能类、Mesh、Niagara、Sound、UI、Material或Texture。

12个真实 .uasset 必须由 Unreal Editor 合法创建。当前Runner没有UE5.8，因此资产状态为“未执行”；Tools/Unreal/GenerateDivineBeastsHeroDefinitions.py 和 Build/Validation/GenerateDivineBeastsHeroDefinitions.ps1 仅提供合法生成路径，不伪造二进制资产。

新增Go业务后端接口：无。
