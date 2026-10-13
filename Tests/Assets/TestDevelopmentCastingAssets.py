# -*- coding: utf-8 -*-
"""施法/怪物真实磁盘资产的最早阻断门禁；路径缺失和文本占位必须失败。
只证明文件存在且非小文本，不证明UE类型、骨架、蓝图编译、Cook或人工视觉。
完整内容合同另由Monolith重载及原生资产测试核实，不能把本扫描冒充引擎验收。
"""
from pathlib import Path
import unittest

ROOT=Path(__file__).resolve().parents[2]
COMMON=ROOT/'Game/Plugins/DivineBeasts/ContentPacks/Common/DBAContentPack_Common/Content'
VFX=ROOT/'Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/Content/VFX/DevelopmentMagic/Casting'


class DevelopmentCastingAssetPresence(unittest.TestCase):
    """独立字面路径期望；缺一项失败，禁止自动创建资产或跳过用例。"""
    def test_real_casting_assets_exist(self):
        for name in ['Fireball','Frostbolt','ArcaneMissiles']:
            paths=[COMMON/f'Mannequins/DBA/Animations/DevelopmentCasting/AS_DBA_Dev{name}Cast.uasset',
                   COMMON/f'Mannequins/DBA/Animations/DevelopmentCasting/AM_DBA_Dev{name}Cast.uasset',
                   VFX/f'Niagara/NS_DBA_Dev{name}Cast.uasset',VFX/f'Definitions/DA_DBA_VFX_Dev{name}Cast.uasset']
            for path in paths:
                with self.subTest(asset=str(path.relative_to(ROOT))):
                    self.assertTrue(path.is_file(),'必须由Monolith制作真实UE资产')
                    self.assertGreater(path.stat().st_size,1024,'小文本/空文件不是资源')

    def test_real_monster_visual_exists(self):
        path=COMMON/'Development/Monsters/BP_DBA_AncientStoneSpiritVisual.uasset'
        self.assertTrue(path.is_file(),'训练石灵必须有真实可编辑视觉资源')
        self.assertGreater(path.stat().st_size,1024)


if __name__=='__main__': unittest.main()
