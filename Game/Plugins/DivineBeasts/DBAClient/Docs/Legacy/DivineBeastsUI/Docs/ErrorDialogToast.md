# ErrorDialogToast（错误对话框与短提示）

项目UI复用 GamePlatformDialogWidget / GamePlatformToastWidget（平台对话框/短提示）和平台Modal/Notification层，不创建第二套弹窗系统。

FDivineBeastsUILocalization（项目UI本地化映射）把业务ErrorCode转换为FText。当前覆盖常见认证、网络、服务器容量、重连、Hero目录和外观错误。

未知内部错误统一降级为用户可理解的通用文案，不把数据库、URL、Token、堆栈、内部服务名等技术信息泄漏给Shipping用户。

ErrorReconnect使用Modal层并提供Retry焦点目标；Toast属于Notification层，不应抢占主要输入焦点。

当前真实Dialog/Toast Widget Blueprint资产为0，因此实际视觉展示为“未执行”。
