# LocalizationAndText（本地化与文本）

用户可见固定文案使用FText/LOCTEXT（本地化文本），不把ErrorCode、UUID、内部服务名直接显示给Shipping用户。

FDivineBeastsUILocalization 当前映射：

- InvalidCredentials
- AccountLocked
- Maintenance
- NetworkUnavailable
- AuthExpired
- NoServerCapacity
- ReconnectExhausted
- HeroCatalogUnavailable
- InvalidAppearance
- NotConfigured

未知错误统一降级为通用用户文案。

玩家自己创建的CharacterName属于用户数据，组合层通过FText::FromString投影；这不是静态本地化字符串。

当前尚未建立正式Localization Target（本地化目标）和多语言资源，实际本地化打包验证为“未执行”。
