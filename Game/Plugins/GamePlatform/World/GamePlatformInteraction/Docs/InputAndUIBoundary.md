# InputAndUIBoundary（输入与UI边界）

`GamePlatformInteraction`不依赖 `GamePlatformInputClient（平台输入客户端）`或 `GamePlatformUI（平台界面）`。公开的 `BeginFocusedInteraction`和 `CancelCurrentInteraction`是组合层可绑定的输入意图入口。

当前 GamePlatformInput 仍是骨架，因此尚未建立真实 `Input.Interact`映射/按下释放绑定；Development TestPawn 只提供 Interactor 和 Gameplay Active，不伪造“输入已联调”。

UI未来只消费 FocusSnapshot、Option/PromptId、SessionState、ServerStartTime、RequiredDuration、Result 和 HoldProgress。Interaction 不 CreateWidget、不打开页面、不直接绘制进度条。

即使本地菜单/文本输入未来抑制 Interaction 输入，服务器仍必须执行完整验证，不能把本地输入状态当作授权。
