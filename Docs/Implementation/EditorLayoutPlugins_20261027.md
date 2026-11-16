# 蓝图与材质图自动排列插件安装

按用户要求，当前工程安装两个独立编辑器工具，置于`Game/Plugins/ThirdParty/Editor/`。第三方工具不计入平台、MOBA与项目层48个机制插件，不建立新的游戏运行框架。

| 插件 | 版本与上游固定提交 | 功能入口 |
| --- | --- | --- |
| Blueprint Anti-Pasta / BlueprintAutoLayout | 0.6.9；ibrews/blueprint-auto-layout；c1c02290f38f5dbf3c297db07531d64cc2f2740a | 蓝图图表：Ctrl+Shift+L；选中节点：Ctrl+Shift+K；工具栏Auto Layout |
| GlooPrint | 1.1.0；ishtms/glooprint；54869a10eba2154a0749936c9d76a43a8d792e14 | 蓝图与材质图表：光标位于画布时按F；右键GlooPrint → Format Graph |

两个插件均为Editor模块，主工程注册显式限制`TargetAllowList: Editor`。本机为源码版UE5.8.0，因此均从官方源码编译，不使用针对UE5.8.2发行引擎的GlooPrint预编译包。保留作者源文件、资源、配置与使用说明。Anti-Pasta采用MIT许可并保留LICENSE；GlooPrint源码标注作者保留所有权利，本次为用户要求的本地工程安装，未对外分发。

Anti-Pasta的三个CPP文件原来依赖共享PCH间接提供引擎版本宏。工程固定使用`-NoSharedPCH`，首次编译出现C4668并错误选择UE4分支。仅在这三个文件首次include之后增加`Runtime/Launch/Resources/Version.h`，不修改排列算法、快捷键或公共引擎代码。

使用既定`Build/Game/BuildFoundation.ps1`完成完整Editor目标增量构建，参数保持`-UsePrecompiled -AdditionalPlugins DBAServer -NoSharedPCH -NoUBA -MaxParallelActions 8`。首轮也遇到打开中的编辑器锁定项目DLL；通过Monolith确认地图和内容均无未保存修改后正常退出，再编译成功。原生读回两个模块均enabled/loaded，版本分别为0.6.9和1.1.0。

验证GlooPrint时误用Monolith同步Automation入口执行异步用例，触发`LatentCommands.IsEmpty()`断言，堆栈定位到MonolithEditor的`HandleRunAutomationTests`。保留崩溃日志，不将该调用写为测试通过；后续改用UE自身Automation队列。最终结果记录于`Saved/Validation/EditorLayoutPlugins20261027/InstallVerification.json`。

安装期间未自动整理正式蓝图或火法材质图。Anti-Pasta不支持材质和Niagara图；GlooPrint支持材质，但不支持Niagara。排列为用户按快捷键或菜单触发；单次排列可Ctrl+Z撤销。不要连续使用两套算法整理同一张图并期望得到相同的布局。

源ZIP、固定提交、主工程修改前副本、首次失败与成功构建、模块读回、原生测试及窗口恢复证据均在本工程Saved目录。游戏客户端与服务器没有因编辑器工具安装重新Cook或部署。
