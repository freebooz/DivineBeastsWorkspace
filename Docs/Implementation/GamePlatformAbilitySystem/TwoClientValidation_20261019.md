# SSLM 双客户端发布与测试账号按钮修复（2026-10-19）

## 构建与实际服务

- 完整Editor：`12ba527d-6172-4ef4-8458-055c34505119`，Passed/0、Eligible。
- 完整Client：`c31bb485-2840-4989-b7ab-3239beed618d`，Passed/0。
- 完整Server：`b2f33dd8-b977-4201-b0c4-2967b16d1f45`，Passed/0。
- Server Cook/Stage：`58c9465f-4e61-4fb9-a62a-283d8f6334ab`，Passed/0；Village与MainArena均从此Stage受控启动，替换旧包，后端及账号角色数据保留。
- Client Cook/Stage：`91fc975d-2c5d-4e2a-8588-f6cfc1006a36`，Passed/0；最终容器门禁629项必需资源完整，五英雄40张图标在容器中。
- 公共UI真实RHI：`16607bf6-5815-4df0-afab-80abcf3efc16`，通过并更新当前资产指纹。

启动前共享战斗/生成契约输入290份核对一致，并核对receipt中的实际Runtime二进制及Stage副本，详见 `Saved/Validation/TwoClients20261019/CompatibilityPreflight.json`。实例身份在 `ServerRestart.json`，新Village运行目录 `fc955cd6-7e56-48ed-b60e-5eab3767c98b`，MainArena池 `7cd01a85-c347-4a6b-9f8c-d8c2083ae2a3`。

五英雄唯一Profile均保存并包重载明确八槽顺序；真实技能栏通过Monolith补齐四个可选槽控件，替换三个空占位图，保持48像素槽位并增设只读标识。选择生命周期新增原生回归1项、既有资源生命周期3项、公共UI颜色1项通过。无服务器V6授予。

## 测试账号按钮不能点击的原因与修复

首轮 `05e7f297-335b-43d4-af2c-0b5e54dad25c` 启动回执明确 `DevelopmentTestAccounts=false`、账号数量0。普通启动器会清理继承凭据，登录页虽然显示十按钮，但缺少完整 `DBA_DEV_TEST_LOGIN_ACCOUNTS_JSON` 时明确禁用，并提示从开发启动器重开。原因是此次启动调用漏传配置，不是按钮事件丢失或后端账号被删除。

新增 `Build/Game/StartLocalTestClients.ps1`，只读验证当前工作空间的Compose容器标签和显式网关端口，从正在运行的身份服务复用已有十账号种子，并显式启用 `DevelopmentTestAccounts`。凭据仅经当前进程和子进程环境传递，不打印、落盘或加入参数；不自动登录、不重建后端、不重置档案。普通发行入口仍保持原默认。

第一次专用入口因Compose配置解析要求无关控制令牌而拒绝启动，未产生额外客户端。修正为只读运行容器标签查询后，实际启动成功：`881e5d14-82be-4b9c-9c66-519f3c2f48ba`，PID 48812/65864，均启用测试账号，账号数量10，`AutoLogin=false`。网关解析回归与脚本语法检查通过。

修复后只读观察到两个客户端均已进入新手村游戏场景，两端日志无Fatal及已检查的复制版本不匹配错误。助手没有点击登录或提交认证。日志摘要在 `Saved/Validation/TwoClients20261019/TestAccountsFixedChecks.json`，真实运行证据在 `Saved/Validation/FrontEndClients/881e5d14-82be-4b9c-9c66-519f3c2f48ba/`。

## 验收边界

上述确认构建、发布、启动配置及当前场景加载。技能图标视觉仍须按所选英雄逐项验收；目前观察存在空槽/绿色显示，未判定五英雄HUD通过。竞技场匹配准入、完整双人玩法及人工美术签收不由进程或日志存在性替代。服务器启动还保留客户端专用Viewport/Inventory类扫描的旧配置诊断，此次未扩大到无关配置整改。
