# MenusDialogsToasts（菜单、对话框与短提示）

UGamePlatformMenuScreen（平台菜单页面）是可激活菜单基类。UGamePlatformDialogWidget（平台对话框）只允许一次Resolve（解析结果）；重复Resolve返回false，避免同一对话框重复提交。

UGamePlatformToastWidget（平台短提示）包含 ToastKey（去重键）、Priority（优先级）和DurationSeconds（持续时间）。Manager当前规则：

- ToastKey为空时生成当前LocalPlayer范围内的唯一Key。
- 相同非空ToastKey且新Priority小于或等于当前Toast时拒绝重复。
- 相同ToastKey但新Priority更高时，先移除旧Toast，再接纳高优先级Toast。
- DurationSeconds大于0且World有效时使用World Timer自动过期。
- Travel会主动清理全部Toast与Notification Overlay。

Toast走Notification Layer（通知层），不成为CommonUI Activatable节点，因此不主动抢焦点。

平台层不硬编码项目错误文案；上层应把ErrorCode映射为本地化FText后再展示。真实Toast/Dialog Blueprint与运行测试当前未执行。

