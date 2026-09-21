# Architecture（架构说明）

## 三层定位

- `GameFoundation（游戏平台基础层）`：`GamePlatformVFX` 负责 How（如何播放）。
- `MobaCommon（MOBA通用层）`：`MobaPresentation` 负责 What Happened（发生了什么）。
- `DivineBeasts（神兽联盟项目层）`：项目 Catalog / Definition / ContentPack 负责 What It Looks Like（具体长什么样）。

三层不是三套 VFX 播放系统。

## 依赖规则

允许：项目内容 → `GamePlatformVFX`。  
禁止：`GamePlatformVFX` → `MobaCommon` / `DivineBeasts`。

## 运行链

`Semantic Request → Catalog Resolver → DefinitionId → Definition → Niagara → Instance Handle`

## 网络权威

本插件只执行客户端表现。Projectile / Area / Shield 等 Definition 只描述视觉行为，不能承担服务器命中、伤害、治疗、碰撞或传送权威。
