# SSLM首批音频资源许可与制作记录（2026-10-18）

使用真实网络下载的九段音频作为第一轮客户端音频样板：项目音效内容包为`DBASFXPack_Core`，源文件位于`SourceArt/Audio`。此目录不是专用服务器的AlwaysCook资源。

| 声音用途 | 原来源 | 下载许可证 | SHA256 |
|---|---|---|---|
| UI Click | [Kenney Interface Sounds](https://kenney.nl/assets/interface-sounds) click_002.ogg | CC0 | adcd1f4adc35f1b41bc1b5bbefeff7aa44f2f3f0d96d3199b544140c7c1e761c |
| UI Confirm | Kenney confirmation_001.ogg | CC0 | 063564703b6094d70718a3e787a55cc9141611e4ecd6b6637f8828f79b4a8c3a |
| UI Cancel | Kenney back_001.ogg | CC0 | 07db973f79f6ae0f2edc34561e7592e24d0577455919fb602cb8ecc0da991dcf |
| Frost Cast | [JaggedStone Magic Spell SFX](https://opengameart.org/content/magic-spell-sfx) magical_1.ogg | CC0 | 119f9061a566c72c1c58444b03f8685b833079ef268dda02fe742eb34d3bc68f |
| Frost Impact | JaggedStone magical_3.ogg | CC0 | 9fac648a1de869cf9ca2f4b3f1cd47f1766a3ee9d2fdec45831234401d7b2392 |
| Fire Impact | [Julien Matthey Fireball](https://lpc.opengameart.org/content/fireball-1) | CC0 | 3992598c814de25c318780e506aeda673a7e410b7b3c4f0793883483ff9766b8 |
| Login Music | [Tozan Asianoriental1](https://opengameart.org/content/asianoriental1) | CC0 | 172d95262348d020d7d1428046b100aefbad0a6cbaa93ef87efafeb8add107d0 |
| Village Music | [troubadour Fantasy Song Pack](https://opengameart.org/content/fantasy-song-pack-volume-1) homestead.mp3 | CC0 | b6ce65fceb9373f32697ed27455ed995fa03a77649b04a6f3e86a0243d25d4b3 |
| Forest Ambience | [TinyWorlds Forest Ambience](https://opengameart.org/content/forest-ambience) | CC0 | 9850aa1d0d5d66bd9c5daf8bb77c6d852e01f2f4de22f283bd5621e8bed13b75 |

## 职责与验收边界

`Tools/Unreal/Audio/ImportStarterAudio.py`（编辑器真实音频导入脚本）导入音源为SoundWave（声波资产），并生成现有`UGamePlatformSFXDefinition`（平台音效定义）。导入之前检查实际文件与SHA256；源文件不应该被误当成真实二进制资产。

本批为可替换的免费CC0底稿，不代表3A正式录制或专业混音完成。首批BGM循环作为World级SFX播放时不保证跨地图连续；正式跨图音乐导演需另建实现。所有三维技能声须在生产验收前补齐并发策略、空间衰减和用户音量设置。不得提取YouTube预览音频，也不绕过Fab登录/付费授权限制。

人工审核必须独立确认声音悦耳、符合东方神话美术方向、音频无爆音、首尾能循环、5v5高负载声音可读和专用服务器Cook排除。
