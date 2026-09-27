# InputFocusNavigation（输入、焦点与导航）

项目不创建第二套Input Router（输入路由器），复用 GamePlatformUI/CommonUI（平台UI/通用UI）的 Input Policy（输入策略）、Activatable Widget（可激活控件）和Back导航。

源码页面清单为关键页面声明DefaultFocusWidgetName：

- Login → AccountInput
- Character Roster/Select → CharacterList
- Character Create → CharacterNameInput
- Matchmaking → ModeList
- Match Found → ReadyButton
- Arena Hero Selection → HeroList
- Scoreboard → ScoreboardList
- PostMatch → ReturnWorldButton
- System Menu → ResumeButton

平台 UGamePlatformUIScreen 使用NativeGetDesiredFocusTarget实现CommonUI期望焦点。

Keyboard/Mouse（键盘鼠标）、Gamepad（手柄）、Touch（触控）真实运行测试当前未执行，因为没有UE5.8工具链和Widget资产。
