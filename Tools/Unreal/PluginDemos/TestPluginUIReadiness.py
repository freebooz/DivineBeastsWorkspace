# -*- coding: utf-8 -*-
"""插件演示覆盖率不能借菜单数、已注册案例或合成夹具伪报为100%。"""
from __future__ import annotations
import unittest
from AuditPluginUIReadiness import MANIFEST, summarize
import json


class PluginUIReadinessTests(unittest.TestCase):
    def setUp(self):
        self.raw = json.loads(MANIFEST.read_text(encoding="utf-8-sig"))

    def test_live_manifest_explicitly_not_ready(self):
        result = summarize(self.raw)
        self.assertFalse(result["productionDemoVerified"])
        self.assertIsNone(result["overallCoveragePercent"])
        self.assertEqual(len(self.raw["plugins"]), result["pluginCount"])
        self.assertEqual(len(self.raw["scenarios"]), result["registeredScenarioCount"])

    def test_unreviewed_is_not_zero_features(self):
        fake = {"plugins":[{"plugin_id":"PluginA", "architecture_layer":"Platform",
                              "plugin_kind":"code", "feature_audit_status":"unreviewed"}],
                "features": [], "scenarios": []}
        result = summarize(fake)
        self.assertEqual(0, result["plugins"][0]["registeredFeatures"])
        self.assertIsNone(result["plugins"][0]["coveragePercent"])
        self.assertEqual("audit_real_features_and_owners", result["plugins"][0]["nextAction"])

    def test_registered_and_wired_without_review_is_not_ready(self):
        clone = dict(self.raw)
        cloned_plugins = [dict(p) for p in self.raw["plugins"]]
        clone["plugins"] = cloned_plugins
        for plugin in cloned_plugins:
            plugin["feature_audit_status"] = "complete"
        result = summarize(clone)
        self.assertFalse(result["productionDemoVerified"])
        self.assertEqual(0, result["reviewedFeatureCount"])

    def test_forged_approval_in_author_file_cannot_prove_acceptance(self):
        clone = dict(self.raw)
        clone["features"] = [dict(feature, review_status="approved") for feature in self.raw["features"]]
        clone["plugins"] = [dict(p, feature_audit_status="complete") for p in self.raw["plugins"]]
        report = summarize(clone)
        self.assertEqual(0, report["reviewedFeatureCount"])
        self.assertFalse(report["productionDemoVerified"])

    def test_unknown_scenario_feature_is_reported(self):
        clone = dict(self.raw)
        clone["scenarios"] = list(self.raw["scenarios"]) + [{
            "scenario_id":"Synthetic.Bad", "feature_id":"NoSuchFeature"
        }]
        result = summarize(clone)
        self.assertIn("Synthetic.Bad", result["unknownScenarioFeatureRefs"])
        self.assertFalse(result["productionDemoVerified"])


if __name__ == "__main__":
    unittest.main(verbosity=2)
