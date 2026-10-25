# 新手村阴天光照与增强风力

用户要求仅增强游戏运行时风力，并将光照调整为阴天。保留初始Clear天气类型，不额外引入Cloudy雾和冷色滤镜，只修改正式地图已有灯光基线。

直接Monolith原位修改并保存：方向光/天光强度均2.5，光源角4°，原月光偏蓝灯色调整为中性白；固定曝光2.5、灯光旋转、雾、材质RGB、所有Actor/PlayerStart和PCG实例保持。先比较4.0与2.0候选固定镜头后选择2.5，避免第一版叶片泛白。地图保存前备份并独立Editor冷重载通过。

晴天Weather Definition仅WindIntensity由0.45提高至0.70，方向0°、阵风0.20、频率0.10Hz保持，不新增地图风源。正式新手村PIE在10秒读取实际MPC目标0.70196（网络量化值）。灯光运行时读值与已保存基线一致，天气刷新不会恢复旧月光。

固定镜头原生帧、视频、前后快照和冷重载在Saved/Validation/VillageOvercastLighting20261025；风定义单字段备份/保存/重载证据在Saved/Validation/WindStrength07020261025。视频为Editor PIE，最终双端发布与客户端状态以Saved/Validation/WorldControlsCastingLighting20261025/FinalValidation.json为准。人工视觉和GPU预算验收继续单列。
