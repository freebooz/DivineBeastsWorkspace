# LocalizationAndText（本地化与文本）

平台和项目用户可见文案使用 `FText（本地化文本）`。LOCTEXT 仅用于框架级错误，例如页面未注册、加载失败、路由被阻止。

项目 ErrorCode、角色名、任务文本、匹配状态等不写入 GameFoundation（游戏平台基础层），由上层本地化资源提供。

技术 ID、UUID、ServerInstanceId 等内部标识默认不得直接展示给最终用户。

当前未创建 Localization Dashboard（本地化仪表板）资产或翻译包；真实本地化采集/编译状态为未执行。
