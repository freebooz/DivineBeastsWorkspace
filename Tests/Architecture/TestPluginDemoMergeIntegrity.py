#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SSLM项目演示数据目录合并一致性门禁：不依赖UE Editor，验证源码可进入编译阶段。

本脚本不运行项目代码、不修改资产或业务配置；以保留新的Data定义/租约/预检生命周期
替代被合并冲突混入的旧硬编码地图导航。通过仅代表文本合同一致，不代表功能已运行。
"""
from pathlib import Path
import re
import unittest

ROOT=Path(__file__).resolve().parents[2]
BASE=ROOT/"Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsDemoClient"
CATALOG=BASE/"Private/Demo/DivineBeastsDemoCatalogSubsystem.cpp"
HEADER=BASE/"Public/Demo/DivineBeastsDemoCatalogSubsystem.h"
WORLD=BASE/"Private/Demo/DivineBeastsDemoWorldSubsystem.cpp"


class DemoCatalogMergeGate(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.sources={p:p.read_text(encoding="utf-8-sig") for p in (CATALOG,HEADER,WORLD)}

    def test_no_git_conflict_markers_in_core_catalog_files(self):
        for file,text in self.sources.items():
            for marker in ("<<<<<<<","=======" ,">>>>>>>"):
                with self.subTest(file=file.name,marker=marker):
                    self.assertNotIn(marker,text)

    def test_single_data_driven_catalog_not_direct_map_catalog(self):
        cpp=self.sources[CATALOG]
        self.assertIn("PrivateState->Snapshot.Publish(*Definition, Plugins)",cpp)
        self.assertIn("Data->AcquireDefinition(",cpp)
        self.assertIn("RebuildScenarioProjection()",cpp)
        self.assertIn("PreflightScenario(FName ScenarioId)",cpp)
        self.assertIn("ContinuePendingNavigation()",cpp)
        self.assertIn("PrivateState->DiagnosticMaps.Add(MoveTemp(Item))",cpp)
        self.assertNotIn("KnownScenarios",cpp)
        self.assertNotIn("ResolveScenarioPluginIds",cpp)
        self.assertNotIn("Item->MapAsset.GetLongPackageName()",cpp.split("PreflightScenario(FName ScenarioId)",1)[1].split("OpenScenario(FName InputId)",1)[0])

    def test_contract_stable_identity_not_hardcoded_paired_map(self):
        text=self.sources[HEADER]
        self.assertEqual(len(re.findall(r'UPROPERTY\(BlueprintReadOnly, Category="DivineBeasts\|Demo"\) FName ScenarioId;',text)),1)
        self.assertIn("正式案例使用稳定ID",text)

    def test_world_lookup_by_actual_soft_map(self):
        world=self.sources[WORLD]
        self.assertIn("FName(*Scenario.MapAsset.GetLongPackageName()) == CurrentMap",world)
        self.assertNotIn("Scenario.ScenarioId == CurrentMap",world)

if __name__=="__main__":
    unittest.main(verbosity=2)
