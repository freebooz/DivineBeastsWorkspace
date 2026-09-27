# TestingAndEvidence（测试与证据）

源码包含UE Automation（虚幻自动化）模式/状态机/客户端边界测试和Go匹配领域测试，并提供PowerShell Arena验证脚本与Dedicated模式测试入口。

当前Runner缺少Go工具链和UE5.8可执行环境，因此Go test、UE编译、Dedicated Server、Cook及性能测试必须保持“未执行”。静态源代码验证通过时只代表对应静态范围。