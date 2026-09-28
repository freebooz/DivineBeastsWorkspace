# 神兽联盟内容插件规划与登记

`ContentPacks`只是第三层内部的分类目录，不是第四层。当前已登记公共角色内容包 `DBAContentPack_Common`、十二个生肖英雄内容包及 `DBAUIPack_Core`。角色内容包在原型阶段承载 Manny/Quinn 与十二套颜色外观，正式美术到位后在相同插件身份和稳定 Profile 路径内替换，不改变 HeroDefinitionId、协议或存档。

## 内容所有权规划

```text
ContentPacks/                              # 内容插件分类根目录
├── ContentPackRegistry.json               # 仅登记实际交付的独立内容插件
├── Common/                               # 公共基础资源，按需创建
│   └── DBAContentPack_Common/             # 共用骨架、材质等唯一所有者
├── Heroes/                               # 英雄完整内容；十二生肖共用模板
│   ├── DBAHeroPack_Rat/                   # 子鼠内容
│   ├── DBAHeroPack_Ox/                    # 丑牛内容
│   ├── DBAHeroPack_Tiger/                 # 寅虎内容
│   ├── DBAHeroPack_Rabbit/                # 卯兔内容
│   ├── DBAHeroPack_Dragon/                # 辰龙内容
│   ├── DBAHeroPack_Snake/                 # 巳蛇内容
│   ├── DBAHeroPack_Horse/                 # 午马内容
│   ├── DBAHeroPack_Goat/                  # 未羊内容
│   ├── DBAHeroPack_Monkey/                # 申猴内容
│   ├── DBAHeroPack_Rooster/               # 酉鸡内容
│   ├── DBAHeroPack_Dog/                   # 戌狗内容
│   └── DBAHeroPack_Boar/                  # 亥猪内容
├── Worlds/                               # 世界地图与专属资源
│   ├── DBAWorldPack_OpenWorld/            # 大厅、主城、野外；无独立大厅包
│   ├── DBAWorldPack_Village/              # 新手村、教学、训练
│   └── DBAWorldPack_MainArena/            # 五竞技模式的场景内容
├── Presentation/                         # 公共表现资源，各领域独立所有权
│   ├── DBAPresentationPack_Core/          # 公共VFX定义、目录、Niagara资源
│   ├── DBASFXPack_Core/                   # 公共音效
│   ├── DBAAnimationPack_Core/             # 公共动画，区分必要权威与纯表现
│   └── DBAUIPack_Core/                    # 已登记公共界面内容插件；资产由Monolith MCP生产
└── Optional/                             # 按真实需求评审，禁止空包占位
    ├── Skins/                            # 皮肤包，不能修改权威玩法
    ├── Seasons/                          # 赛季内容
    └── Events/                           # 活动内容
```

`DBAContentPack_Common` 与十二个 `DBAHeroPack_*` 已进入实际交付清单；World/Presentation 中未登记的条目仍只是目标归属。DBAWorlds持有定义类型与项目校验，世界包持有地图；DBAClient持有上下文与注册协调，英雄、世界和公共包各自持有美术，任何资产只有一个源所有者。二进制资产跨挂载点迁移必须通过 Unreal Editor 更新引用，禁止简单文件改名。

## 登记格式与交付门槛

`ContentPackRegistry.json`使用`SchemaVersion: 1`及`ContentPacks`数组。每项含`Name`（稳定插件身份）和`RelativePath`（相对此目录的路径，如`Heroes/DBAHeroPack_Rat`）。规划中的包不进入实际清单；不得重复既有插件身份、使用绝对路径或`..`越界。

每个登记项必须对应同名`.uplugin`、`CanContainContent: true`、无C++模块及Content下的引擎生成资产。结构审计只检查文件存在和声明，资产真假、引用正确性、授权、Cook与产物仍由引擎验证。禁止在内容包中再嵌套插件。

登记文件是工程交付清单，不是第二套运行时资产管理器；登记不会自动激活内容。项目组合选择、GamePlatformData发现与加载、领域目录预检、完整成功后提交、失败撤销与释放租约，均按插件规范P13—P16执行。

## 端侧与验收

- `DBAGameplay` 持有 Server-safe Hero Definition；`DBAHeroPack_*` 只持有客户端外观 Profile、材质和真实/占位美术。Server Target不启用英雄美术包，避免 Manny/Quinn、纹理和材质进入 Dedicated Server Cook。
- 世界包保留引擎外置Actor／对象文件、碰撞、导航和权威PCG结果；装饰与纯VFX不进入Server产物。
- 可选皮肤／活动包不被核心硬引用。公共VFX回退保持可读性，目录冲突、异步取消、世界退出及多实例租约必须验证。
- 未来按模块拆出的代码能力必须先修改正式代码插件清单，不能伪装为内容插件绕过46个基线。
