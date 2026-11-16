"""项目权威弹体执行的源码结构门禁；不替代C++编译或UE原生回归。

防止历史蓝图适配类重新拥有目标、Commit、Timer和Spawn状态机。只读源码，失败无副作用。
"""
from pathlib import Path
import ast
import re
import unittest

ROOT = Path(__file__).resolve().parents[2]
MODULE = ROOT / 'Game/Plugins/DivineBeasts/DBAGameplay/Source/DivineBeastsAbilitiesRuntime'
OWNER = 'DivineBeastsCombatGameplayAbility'
ADAPTERS = ('DivineBeastsPlaceholderGameplayAbility', 'DivineBeastsDevelopmentMagicAbility')


class ProjectileExecutionOwnership(unittest.TestCase):
    """同一权威执行只有一个拥有者；适配类可读取各自CDO/Definition但不能重新执行。"""

    def test_exactly_one_projectile_state_machine(self):
        # 最终V6技能有独立机制，未纳入本次两个兼容入口的收敛，不据此误报或扩大整改。
        sources = [MODULE / 'Private/Abilities' / f'{name}.cpp'
                   for name in (*ADAPTERS, OWNER) if (MODULE / 'Private/Abilities' / f'{name}.cpp').exists()]
        for pattern in (r'SpawnActor<AGamePlatformCombatProjectile>',
                        r'::ReleaseNextProjectile\(', r'GetSelectedProjectileTargetIntent\('):
            owners = {p.stem for p in sources if re.search(pattern, p.read_text(encoding='utf-8-sig'))}
            self.assertEqual(owners, {OWNER}, f'{pattern}必须仅由公共执行拥有，实际为{owners}')

    def test_serialized_adapters_have_no_execution_state(self):
        forbidden = ('FTimerHandle', 'FrozenProjectileTarget', 'ActivationToken', 'FGamePlatformCombatSpec')
        for adapter in ADAPTERS:
            header = (MODULE / 'Public/Abilities' / f'{adapter}.h').read_text(encoding='utf-8-sig')
            source = (MODULE / 'Private/Abilities' / f'{adapter}.cpp').read_text(encoding='utf-8-sig')
            for token in forbidden:
                self.assertNotIn(token, header, adapter + '不能拥有' + token)
            for token in ('CommitAbility(', 'SetTimer(', 'CancelConfirmedCast(', 'SpawnActor<'):
                self.assertNotIn(token, source, adapter + '不能执行' + token)

    def test_shared_base_keeps_existing_development_ancestor(self):
        header = (MODULE / 'Public/Abilities/DivineBeastsDevelopmentGameplayAbility.h').read_text(encoding='utf-8-sig')
        self.assertRegex(header, r':\s*public U' + OWNER)
        magic = (MODULE / 'Public/Abilities/DivineBeastsDevelopmentMagicAbility.h').read_text(encoding='utf-8-sig')
        self.assertRegex(magic, r':\s*public UDivineBeastsDevelopmentGameplayAbility')


class FallbackRuleSynchronization(unittest.TestCase):
    """在无UE环境以明确测试替身验证作者的字段同步，不改真实资产或伪造原生验收。"""

    def test_existing_copy_refreshes_all_rules_and_preserves_identity(self):
        tool = ROOT / 'Tools/Unreal/Abilities/AuthorPlayableHotbarFallbacksUE58.py'
        tree = ast.parse(tool.read_text(encoding='utf-8-sig'))
        functions = [node for node in tree.body if isinstance(node, ast.FunctionDef) and
                     node.name == 'synchronize_rule_defaults']
        self.assertEqual(len(functions), 1, '作者必须对新旧副本使用同一完整规则同步函数')
        namespace = {}
        exec(compile(ast.Module(body=functions, type_ignores=[]), str(tool), 'exec'), namespace)

        class Properties:
            def __init__(self, values): self.values = values.copy()
            def get_editor_property(self, key): return self.values[key]
            def set_editor_property(self, key, value): self.values[key] = value

        rules = ('PlaceholderMode', 'ProjectileMotion', 'PresentationStyleOverride', 'ProjectileCount',
                 'ProjectileIntervalSeconds', 'PlaceholderCooldownTags', 'CostGameplayEffectClass',
                 'CooldownGameplayEffectClass', 'AbilityTags', 'ActivationPolicy')
        source = Properties({key: object() for key in rules})
        destination = Properties({key: object() for key in rules})
        destination.values['AbilityDefinitionId'] = 'fallback-identity'
        namespace['synchronize_rule_defaults'](source, destination)
        for key in rules:
            self.assertIs(destination.values[key], source.values[key], key + '仍是旧副本')
        self.assertEqual(destination.values['AbilityDefinitionId'], 'fallback-identity')


if __name__ == '__main__':
    unittest.main()
