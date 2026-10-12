#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""P9 打包运行报告与当前发布包交叉验证，拒绝旧运行ID、伪汇总、0用例和丢失的客户端。"""
from __future__ import annotations
import importlib.util
import json
import sys
import tempfile
import unittest
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/"Build/Validation"))
SPEC=importlib.util.spec_from_file_location("p7p9_delivery_collector",ROOT/"Build/Validation/CollectP7P9Delivery.py")
assert SPEC and SPEC.loader
MODULE=importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)
CASES=(
    "GamePlatform.SFX.Review.ActorOwnershipAndRejection",
    "GamePlatform.VFX.BirdFlock.ActorInstanceLifecycle",
    "GamePlatform.VFX.BirdFlock.CountAndSeed",
)
RUN_ID="11111111-2222-4333-8444-555555555555"
REL=Path("Saved/Validation/PluginP7P9_20261012")


class PackagedRuntimeEvidenceTests(unittest.TestCase):
    def setUp(self):
        temp=tempfile.TemporaryDirectory()
        self.addCleanup(temp.cleanup)
        self.root=Path(temp.name)
        self.dir=self.root/REL/"PackagedRuntimeVerified"
        self.dir.mkdir(parents=True)
        self.exe=self.root/"Saved/Validation/FoundationM0"/RUN_ID/"Cook-FrontEndClient"/"Stage"/"WindowsClient"/"DivineBeastsArena"/"Binaries"/"Win64"/"DivineBeastsArenaClient.exe"
        self.exe.parent.mkdir(parents=True)
        self.exe.write_bytes(b"verified fixture binary")
        self.report=self.dir/"index.json"
        self.summary=self.dir/"Summary.json"
        self.tests=[{"fullTestPath":name,"state":"Success","errors":0,"warnings":0} for name in CASES]
        self.save_report()
        self.data={
            "report":(REL/"PackagedRuntimeVerified/index.json").as_posix(),
            "status":"passed","failed":0,"processExit":0,"tests":3,
            "executable":self.exe.relative_to(self.root).as_posix(),
            "scope":"仅3项包内原生逻辑回归，不代表声音或联机验收",
        }
        self.save_summary()
    def save_report(self):
        self.report.write_text(json.dumps({"tests":self.tests,"failed":0,"succeeded":len(self.tests)}),encoding="utf-8")
    def save_summary(self):
        self.summary.write_text(json.dumps(self.data),encoding="utf-8")
    def verify(self):
        return MODULE.verify_packaged_runtime(self.root,REL,{"clientCookRunId":RUN_ID})
    def test_matching_real_package_report_is_accepted(self):
        result=self.verify()
        self.assertEqual(result["status"],"passed")
        self.assertEqual(result["tests"],3)
    def test_old_unverified_directory_not_accepted(self):
        self.data["report"]=(REL/"PackagedRuntime/index.json").as_posix()
        self.save_summary()
        self.assertNotEqual(self.verify()["status"],"passed")
    def test_missing_summary_fails_closed(self):
        self.summary.unlink()
        self.assertNotEqual(self.verify()["status"],"passed")
    def test_summary_with_fail_status_rejected(self):
        self.data["status"]="failed";self.save_summary()
        self.assertNotEqual(self.verify()["status"],"passed")
    def test_nonzero_process_exit_rejected(self):
        self.data["processExit"]=41;self.save_summary()
        self.assertNotEqual(self.verify()["status"],"passed")
    def test_wrong_release_run_rejected(self):
        self.data["executable"]=str(self.exe.relative_to(self.root)).replace(RUN_ID,"unrelated")
        self.save_summary()
        self.assertNotEqual(self.verify()["status"],"passed")
    def test_missing_packaged_client_rejected(self):
        self.exe.unlink()
        self.assertNotEqual(self.verify()["status"],"passed")
    def test_empty_tests_rejected(self):
        self.tests.clear();self.save_report()
        self.assertNotEqual(self.verify()["status"],"passed")
    def test_report_case_fail_rejected_despite_summary(self):
        self.tests[0]["state"]="Fail";self.tests[0]["errors"]=1;self.save_report()
        self.assertNotEqual(self.verify()["status"],"passed")
    def test_missing_required_case_rejected(self):
        self.tests.pop();self.data["tests"]=2;self.save_report();self.save_summary()
        self.assertNotEqual(self.verify()["status"],"passed")
    def test_duplicate_case_rejected(self):
        self.tests[1]["fullTestPath"]=self.tests[0]["fullTestPath"]
        self.save_report()
        self.assertNotEqual(self.verify()["status"],"passed")
    def test_report_count_mismatch_rejected(self):
        self.data["tests"]=99;self.save_summary()
        self.assertNotEqual(self.verify()["status"],"passed")
    def test_path_escape_rejected(self):
        self.data["report"]="../outside/index.json";self.save_summary()
        self.assertNotEqual(self.verify()["status"],"passed")


if __name__=="__main__":
    unittest.main(verbosity=2)
