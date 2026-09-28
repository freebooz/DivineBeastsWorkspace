# Freebooz Studio 与《神兽联盟》品牌源图交接

> 状态：源图设计已完成；当前 Cwebcodex Runner 尚未接收到 PNG 二进制。
> 本目录只记录品牌源图交接、命名和 UE 导入目标，不伪造任何 `.uasset`。

## 1. 已确认品牌

### Freebooz Studio（公司品牌）

- 正式标识文字：`Freebooz Studio`
- 概念源图文件名：`FreeboozStudio_Logo_Concept.png`
- 概念图尺寸：1254 × 1254
- SHA-256：`d99826d6e675da88414b0f3681fbd6d422d3c60ab91874591ff33e9afa399255`
- 视觉关键词：F 字母、飞翼／火焰／东方笔触、黑金红、高端游戏工作室
- 使用位置：原生启动封面、Boot 启动页、官网、版权页、制作方署名

### 神兽联盟（游戏品牌）

- 中文标题：`神兽联盟`
- 英文副标题：`Divine Beasts Arena`
- 概念源图文件名：`DivineBeastsArena_Logo_Concept.png`
- 概念图尺寸：1448 × 1086
- SHA-256：`bd2db2d016297c749c925bfa382a1254ffe1f59bbc61f4b118e4116599b51285`
- 视觉关键词：国风奇幻、神兽、十二生肖、金／玉／朱红／青蓝能量
- 使用位置：Boot 启动页、Login 登录页、Loading 加载页、官网与宣传物料

## 2. 源图与运行时资产分离

概念 PNG 是美术 SourceArt（源素材），不是 UE 运行时资产。

规划中的唯一运行时内容所有者：

```text
Game/Plugins/DivineBeasts/ContentPacks/
└── Presentation/
    └── DBAUIPack_Core/                 # 只有真实UE资产落地时才创建并登记
        └── Content/
            └── UI/
                └── Branding/
                    ├── T_DBA_Brand_FreeboozStudio_Logo
                    └── T_DBA_Brand_GameLogo
```

当前 `ContentPackRegistry.json` 继续保持真实交付清单语义。在 UE Editor 合法创建
`DBAUIPack_Core.uplugin` 并导入至少一个真实资产以前，不得创建空内容插件或把它登记为已交付。

## 3. UI 使用目标

首批 Widget Blueprint（控件蓝图）：

```text
WBP_DBA_UI_RootLayout
WBP_DBA_UI_Boot
WBP_DBA_UI_Login
WBP_DBA_UI_LoadingTravel
```

建议视觉关系：

```text
Boot
├── Freebooz Studio Logo
├── 神兽联盟 Logo
├── 背景主视觉
├── 版权/版本
└── 真实 Loading Stage / Progress

Login
├── 神兽联盟 Logo
├── 登录背景
├── AccountName
├── Password
├── Login Button
├── 登录状态/错误提示
└── 版本与服务器环境提示
```

Boot / Login 的 C++ 业务类不得直接硬引用上述纹理。资源应由
`UGamePlatformUIScreenDefinition（界面定义）`、Soft Reference（软引用）及
`GamePlatformData（平台数据插件）`异步加载。

## 4. 当前导入边界

本次 ChatGPT 会话已经生成两张设计源图，但调用
`Cwebcodex import_conversation_files_to_project（对话文件导入项目）` 时，
连接器返回：

```text
import_conversation_files_to_project requires trusted MCP host-file provenance
```

因此当前仓库只落地本说明和资源清单，不声称 PNG 已复制到 Runner。
禁止使用 Base64、伪造下载地址或生成假二进制资源绕过连接器来源校验。

恢复方式：

1. 将两张 PNG 作为可信对话附件重新上传后使用 Cwebcodex 导入；或
2. 在 Runner 本机把源图复制到本目录的 `SourceArt/`；或
3. 后续在 Unreal Editor 中直接从已审核源图导入 `DBAUIPack_Core`。

人工复制后必须核对本文件及 `BrandingSourceArtManifest.json` 中记录的 SHA-256，
校验一致后才能进入 Unreal Editor 导入步骤，避免误用压缩、重采样或旧版本源图。

## 5. 正式验收

在宣称品牌资产已进入游戏前，至少应完成：

- PNG 原图人工审核；
- 中文／英文 Logo 字形确认；
- 商标与版权检查；
- UE Editor 正式导入；
- TextureGroup / Mip / Compression 设置检查；
- PC 与移动端清晰度检查；
- Boot / Login / Loading 三界面实际运行；
- Cook / Stage 后资源存在性验证。
