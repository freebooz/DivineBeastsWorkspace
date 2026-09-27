# ShieldAndMitigation（护盾与减伤边界）

平台第一版实现基础 Shield/MaxShield，不支持 OverShield。默认 Damage 先扣 Shield，溢出进入 Health；Health 不会低于 0。

`Combat.Damage.BypassShield`允许服务器受信任的 CombatSpec 绕过护盾。没有公开客户端 RPC 可以上传该标签要求服务器接受。

纯函数 `FGamePlatformCombatMath::ResolveDamage`被运行组件和 Automation 测试共同使用，覆盖护盾吸收、溢出、绕盾和过量伤害。

最终 Armor、Magic Resistance、Crit、百分比减伤等公式没有在本轮伪实现，保留给后续通用或项目 Execution 扩展。
