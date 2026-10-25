# 新手村阴天光照与增强风力

用户要求仅增强游戏运行时风力，并将光照调整为阴天。保留初始Clear天气类型，不额外引入Cloudy雾和冷色滤镜，只修改正式地图已有灯光基线。

首轮直接Monolith原位修改并保存：方向光/天光强度均2.5，光源角4°，原月光偏蓝灯色调整为中性白；当时保留固定曝光2.5、灯光旋转、雾、材质RGB、所有Actor/PlayerStart和PCG实例。先比较4.0与2.0候选固定镜头后选择2.5。该首轮结果随后仍被用户指出地面泛白，不能作为最终视觉签收。

晴天Weather Definition仅WindIntensity由0.45提高至0.70，方向0°、阵风0.20、频率0.10Hz保持，不新增地图风源。正式新手村PIE在10秒读取实际MPC目标0.70196（网络量化值）。灯光运行时读值与已保存基线一致，天气刷新不会恢复旧月光。

固定镜头原生帧、视频、前后快照和冷重载在Saved/Validation/VillageOvercastLighting20261025；风定义单字段备份/保存/重载证据在Saved/Validation/WindStrength07020261025。视频为Editor PIE，最终双端发布与客户端状态以Saved/Validation/WorldControlsCastingLighting20261025/FinalValidation.json为准。人工视觉和GPU预算验收继续单列。

2026-10-25地面泛白修正：实际正式地图仍使用月光阶段遗留的Manual曝光补偿+2.5 EV。真实PIE同机位对比后，只将既有全局后处理体积的`AutoExposureBias`改为+1.0 EV；方向光与天光继续为2.5，曝光模式、光源角、灯色、雾、材质、场景实例和出生点均未修改。运行时风场回读仍为0.7019608。完整环境快照除这一字段外相同，独立编辑器PID48372冷重载通过；保存地图SHA256为`2fa3662054514dc65f6e97ecb50b05d85bca9481a5608325fd18ca2cb393012f`。原地图字节、前后原生帧、单字段检查与后续交付证据归`Saved/Validation/VillageGroundExposure20261025`。这次直接授权的曝光修正没有合入尚待P5批准的晨间光雾副本。
