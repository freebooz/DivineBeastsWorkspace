#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""鸟群P5接入源码门禁：防止只有孤立GPU资产、没有正式表现分发与世界销毁取消路径。
只检查源码接线约束，不代替真实Client编译或联机/视觉测试。
"""
from pathlib import Path
root=Path(__file__).resolve().parents[2]
base=root/'Game/Plugins/DivineBeasts/DBAClient/Source'
header=(base/'DivineBeastsPresentationClient/Public/DivineBeastsPresentationClientSubsystem.h').read_text(encoding='utf-8-sig')
for api in ('StartAmbientBirdFlock','StopAmbientBirdFlock','StopAllAmbientBirdFlocks','SubmitPendingBirdFlocks'):
    assert api in header, '缺少正式鸟群表现API：'+api
source=(base/'DivineBeastsPresentationClient/Private/Environment/DivineBeastsBirdFlockPresentation.cpp').read_text(encoding='utf-8-sig')
assert 'ActivateContentPack' in source and 'SubmitProjectRequest' in source and 'Cancelled' in source
assert 'SpawnSystemAtLocation' not in source and 'NewObject<UNiagara' not in source,'项目层不能绕过平台VFX执行器'
print('BIRD_INTEGRATION_SOURCE_CONTRACT_OK')
