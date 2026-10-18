# SSLM 按钮音效与新手村背景音乐实施记录（2026-10-18）

工程：DivineBeastsWorkspace；UE5.8。范围仅包括第三层项目客户端接线，不修改服务器权威或Widget蓝图美术。

## 功能实现

项目层 UDivineBeastsUIScreen（普通页面基类）在页面激活时，对现有WidgetTree中的UButton使用OnClicked.AddUniqueDynamic绑定一次性音效事件，按钮名称包含Cancel/Back/Close/Revert时播放取消音，包含Confirm/Accept/Apply/Save时播放确认音，其余播放点击音。失活时RemoveDynamic并清空弱按钮引用，不使用Tick。登录页已去除旧的独立播放调用，避免一次点击产生双声。非UButton的特殊自绘控件不在此次自动绑定范围。

统一调用 UDivineBeastsPresentationClientSubsystem::PlayStarterUISound，再通过GamePlatformPresentation（表现系统）→GamePlatformSFX（唯一音效播放器）→GamePlatformData（异步资源租约）完成真正播放；不直接加载SoundWave。

音效资产属于/DBASFXPack_Core/Audio/UI/：DA_DBA_SFX_UI_Click、DA_DBA_SFX_UI_Confirm、DA_DBA_SFX_UI_Cancel。

正式新手村 L_Village_Start 的本地客户端在进入世界或控制器就绪时，自动申请 Music.Village 独立内容包。音乐Definition 为 DA_DBA_SFX_Music_Village，声音为 S_DBA_Music_Village_01；配置循环、约1.8秒淡入淡出以及音乐并发策略。世界退出、切图和账号切换先用原请求ID取消循环，再撤销目录和数据租约。

旧9条音频统一预载已拆为 UI、Spell、Music.Village/Music.Login、Ambience.Forest 四组。Catalog在检查标签前按类别筛选，目录和每条Entry作用域统一设为ContentPack，资源可用后按音频包ID和世代匹配触发提交音乐；某类内容加载失败不会阻塞其他音频领域。旧的无参数全9条目录构建方式保留兼容。

## 真实验证与限制

- 三个受影响Client模块增量构建成功：Saved/Validation/FoundationM0/32e63471-78ca-4894-ae8d-28c2eb6b4ba8/Build。
- 本轮完整DivineBeastsArenaClient目标增量构建曾成功：Saved/Validation/FoundationM0/088f0fe7-fcf3-4c45-97d2-b1e5eee58fc5/Build。
- UE5.8独立Commandlet冷回读9份SoundWave/Definition与5份策略通过：Game/Saved/Logs/SSLM_ButtonVillageAudioColdVerify_Retry_20261018.log，标志SSLM_AUDIO_COLD_VERIFY_OK 9 5。
- Tools/Unreal/Audio/TestVillageMusicAndButtonWiring.ps1静态专项回归通过：SSLM_BUTTON_VILLAGE_BGM_WIRING_STATIC_OK 3-UI 1-VillageMusic 1-Ambience 3-Spells。
- 随后补充了每条Catalog Entry的ContentPack作用域，静态再次通过；最后的完整客户端构建由于其他Foundation构建/Cook锁返回2（未执行）：Saved/Validation/FoundationM0/431b61ed-120b-4095-b067-ce7d0f9dfb86/Build。因此不能声称最新一行源码修改后已经重新通过完整编译。
- 隔离Client可执行程序曾使用-nosound进行短暂无音频烟测，没有生成指定运行日志，只停止本次启动的进程；尚未确认真实Village场景运行的音乐请求和用户扬声器实际声音。

## 人工验收

在具备音频设备的真实UE5.8客户端/PIE中，先检查登录、返回、设置及角色页面各种按钮一次点击只响一次；进入正式L_Village_Start，监听SSLM场景音频请求已受理日志，核查村庄音乐开始、持续循环和音量平衡。随后退出/再次进入，应正确淡出而不重复叠音。独立验证缺失其它声音包不影响音乐及按钮、Dedicated Server最小Cook排除纯音频资产。音质及5v5 Audio Insights性能尚未签署人工验收。

本轮未重做SFX播放器，未创建新的音频插件，未修改Widget资产，也未提交其他任务的工作区更改。
