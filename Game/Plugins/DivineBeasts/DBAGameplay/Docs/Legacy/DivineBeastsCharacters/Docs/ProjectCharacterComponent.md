# ProjectCharacterComponent（项目角色组件）

UDivineBeastsCharacterComponent挂在ACharacter上，不Tick。

职责：可信CharacterId/HeroDefinitionId/Zodiac绑定、异步Definition lease、SpawnGeneration/AvatarGeneration、防旧回调、基础移动/碰撞应用、项目Readiness和诊断。

AuthorityBindTrustedContext只能在Owner具有Authority时执行，没有客户端Server RPC入口。远端观察者不要求拿到CharacterId即可根据复制的Hero/Zodiac/Generation初始化公开角色状态。

组件不拥有ASC、技能、伤害、AI Brain、装备、VFX/SFX/UI或动画Runtime。
