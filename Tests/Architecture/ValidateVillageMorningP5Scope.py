"""SSLM新手村晨间环境P5审核范围静态回归。

职责：在不启动UE5.8编辑器的情况下，校验隔离范围、曝光版本及旧预设不变。
边界：不生成或修改引擎资产；不替代Monolith写入、独立Editor重载或真实视觉验收。
运行：python Tests/Architecture/ValidateVillageMorningP5Scope.py
"""
import ast
import importlib.util
import json
import sys
import types
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
TOOLS = ROOT / "Tools" / "Unreal" / "World"
AUTHOR_PATH = TOOLS / "ReviewVillageMorningEnvironmentUE.py"
PIE_PATH = TOOLS / "VillageMorningPIEReviewUE.py"


def verify():
    """检查独立P5命名空间与旧版保护；断言失败即表示不可进入真实UE作者阶段。"""
    assert AUTHOR_PATH.is_file() and PIE_PATH.is_file(), "正式工具脚本缺失"
    for path in (AUTHOR_PATH, PIE_PATH):
        ast.parse(path.read_text(encoding="utf-8"), filename=str(path))

    legacy = json.loads((TOOLS / "VillageMorningPresets.json").read_text(encoding="utf-8"))
    p5 = json.loads((TOOLS / "VillageMorningP5Presets.json").read_text(encoding="utf-8"))
    assert legacy["shared"]["fixed_exposure_bias"] == 2.5, "历史候选的曝光不得被修改"
    assert p5["shared"]["fixed_exposure_bias"] == 1.0, "P5必须使用当前正式曝光基线"
    assert p5["exposure_provenance"]["last_reported_formal_exposure_bias_ev"] == 1.0
    assert p5["presets"] == legacy["presets"], "P5只更新对照曝光，不应擅改三套雾参数"
    assert p5["quality"] == legacy["quality"], "P5不得更改历史画质档"
    for key, value in legacy["shared"].items():
        if key != "fixed_exposure_bias":
            assert p5["shared"][key] == value, "P5改变了非曝光光雾参数：" + key

    # 只为验证模块常量和参数选择而替身化虚幻接口；绝不模拟真实引擎操作。
    previous_unreal = sys.modules.get("unreal")
    fake_unreal = types.ModuleType("unreal")

    class FakePaths:
        @staticmethod
        def project_dir():
            return str(ROOT / "Game")

    fake_unreal.Paths = FakePaths
    sys.modules["unreal"] = fake_unreal
    try:
        spec = importlib.util.spec_from_file_location("SSLMVillageMorningScopeTest", AUTHOR_PATH)
        assert spec and spec.loader
        author = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(author)
        assert author.REVIEW == author.LEGACY_REVIEW
        assert author.presets()["shared"]["fixed_exposure_bias"] == 2.5

        chosen = author.select_review_scope("p5")
        assert author.REVIEW == author.P5_REVIEW
        assert author.OUT == author.P5_OUT
        assert author.P5_REVIEW != author.LEGACY_REVIEW
        assert author.P5_OUT != author.LEGACY_OUT
        assert chosen["review_map_root"] == p5["review_root"]
        assert author.presets()["shared"]["fixed_exposure_bias"] == 1.0

        author.select_review_scope("legacy")
        assert author.REVIEW == author.LEGACY_REVIEW
        assert author.presets()["shared"]["fixed_exposure_bias"] == 2.5
        try:
            author.select_review_scope("production")
        except AssertionError:
            pass
        else:
            raise AssertionError("未知审核范围未被拒绝")
    finally:
        if previous_unreal is None:
            del sys.modules["unreal"]
        else:
            sys.modules["unreal"] = previous_unreal

    author_source = AUTHOR_PATH.read_text(encoding="utf-8")
    pie_source = PIE_PATH.read_text(encoding="utf-8")
    for marker in ("P5基线必须从正式地图只读采集", "P5基线证据已存在", "P5拒绝使用历史旧基线", "FOUR_MAPS_P5_COLD_RELOAD_PASS"):
        assert marker in author_source, "缺少隔离/保护机制：" + marker
    assert "P5运行时曝光偏离基线" in pie_source, "PIE运行时曝光回读缺失"
    print("VILLAGE_MORNING_P5_STATIC_PASS: legacy EV=2.5; P5 EV=1.0; isolated paths; PIE exposure guard")


if __name__ == "__main__":
    verify()
