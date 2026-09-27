# RequestLifecycle（请求生命周期）

项目事实先构造FGamePlatformPresentationRequest（平台表现请求），然后由平台Coordinator统一补Context、解析Catalog并分发Provider。

项目Client对RequestId维护最多2048条Prediction状态：

- 同一Predicted重复：不重复提交。
- Predicted→Confirmed：更新状态但不双播。
- Confirmed后旧Predicted：忽略。
- Corrected/Cancelled等状态变化可继续提交。
- InvalidRequest/StaleWorld时移除去重状态，允许正确新事实重试。

ProviderMissing为安全降级，不改变Gameplay。
