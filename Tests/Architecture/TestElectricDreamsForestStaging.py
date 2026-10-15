#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Electric Dreams隔离MigratePackages入口的纯合同测试。

测试仅创建独立临时uproject文本配置与空Content目录，不创建或仿造UE二进制。
真实迁移只可在已登录Fab并下载的官方示例UE Editor内执行，当前不予假设。
"""
from __future__ import annotations

import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest import mock

TOOL = Path(__file__).resolve().parents[2]/"Tools/Unreal/PCG/StageElectricDreamsForestUE58.py"
s = importlib.util.spec_from_file_location("electric_dreams_staging", TOOL)
if not s or not s.loader:
    raise RuntimeError("资源迁移工具未找到")
m = importlib.util.module_from_spec(s)
s.loader.exec_module(m)


def make_source_and_stage(root: Path) -> tuple[Path, Path]:
    """创建真实目录和纯JSON项目配置，测试只读预检，不触碰UE。"""
    src = root/"ElectricDreamsEnv"/"ElectricDreamsEnv.uproject"
    stage = root/"IsolatedStaging"/m.STAGING_PROJECT_FILENAME
    for p in (src,stage):
        p.parent.mkdir(parents=True)
        p.write_text(json.dumps({"FileVersion":3}),encoding="utf-8")
    (stage.parent/"Content").mkdir()
    (src.parent/"Content").mkdir()
    return src, stage


class MigrationStagingTests(unittest.TestCase):
    def test_inspect_without_engine(self):
        r=m.execute("inspect",None,False)
        self.assertFalse(r["migrationExecuted"])
        self.assertEqual(r["sourcePackages"],[m.SOURCE_MAP,m.SOURCE_GRAPH])

    def test_empty_stage(self):
        with tempfile.TemporaryDirectory() as tmp:
            source,stage=make_source_and_stage(Path(tmp))
            state=m.validate_staging(source,stage)
            self.assertTrue(state["stageEmpty"])
            self.assertTrue(state["sourceContentExists"])
            self.assertEqual(state["stageAssetCount"],0)

    def test_reject_wrong_project_and_same_root(self):
        with tempfile.TemporaryDirectory() as tmp:
            source,stage=make_source_and_stage(Path(tmp))
            bad=stage.with_name("DivineBeastsArena.uproject")
            bad.write_text('{"FileVersion":3}',encoding="utf-8")
            with self.assertRaises(FileNotFoundError):
                m.validate_staging(source,bad)
            with self.assertRaises(RuntimeError):
                m.validate_staging(stage,stage)

    def test_existing_assets_prevent_apply(self):
        with tempfile.TemporaryDirectory() as tmp:
            source,stage=make_source_and_stage(Path(tmp))
            # 模拟Path.rglob返回已存在的UE资产，不实际创建或伪造.uasset二进制。
            asset = mock.Mock()
            asset.is_file.return_value = True
            asset.suffix = ".uasset"
            with mock.patch.object(Path, "rglob", return_value=[asset]):
                state=m.validate_staging(source,stage)
            self.assertEqual(state["stageAssetCount"],1)
            self.assertFalse(state["stageEmpty"])

    def test_header_rejects_non_native(self):
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp)
            self.assertFalse(m.native_package_present(p,m.MIGRATE_PATHS[0]))

    def test_license_gate_is_explicit(self):
        src=TOOL.read_text(encoding="utf-8")
        self.assertIn("acknowledge_license",src)
        self.assertIn("AssetMigrationConflict.SKIP",src)
        self.assertIn("migrationDestination",src)
        self.assertIn("isolated_staging_only",src)


if __name__=="__main__":
    unittest.main()
