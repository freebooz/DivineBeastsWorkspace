# -*- coding: utf-8 -*-
"""冰箭 GPU 编译回归：独立夹具证明容器存在不能掩盖 Cook 的着色器错误。

属于项目发布验证工具，不启动 UE、不修改真实资产。临时 CSV 和日志只验证门禁行为，
不能作为实际 Cook 成功证据；HLSL 检查覆盖锁定 UE5.8 的输入 token 替换边界。
"""
import importlib.util
from pathlib import Path
import re
import tempfile
import unittest

from ValidateDevelopmentCastingContainer import required_assets, validate


ROOT = Path(__file__).resolve().parents[2]
STAGES = ("Cast", "GroundRune", "Launch", "Flight", "Impact", "FrostResidue")


class IceArrowCookShaderTest(unittest.TestCase):
    """CSV 为完整 WindowsClient 导出闭包；每个负例只改变 Cook 日志一个条件。"""

    def check(self, extra="", missing_stage=None, completed=True, log_present=True):
        """构造隔离的同次 Cook 目录，退出清理夹具；不扫描或删除用户输出。"""
        scratch = ROOT / "Saved/Validation/IceArrow/CookShaderFixtures"
        scratch.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="CookShaderFixture-", dir=scratch) as directory:
            directory = Path(directory)
            csv_path = directory / "ContainerInspection/ClientContainer.csv"
            csv_path.parent.mkdir()
            rows = ["Filename,ChunkType,Platform,Size"]
            rows.extend("../../../Plugins/" + path + ",ExportBundleData,WindowsClient,1024"
                        for path in required_assets())
            csv_path.write_text("\n".join(rows), encoding="utf-8")
            if log_present:
                log = directory / "UATLogs/Cook-Fixture.txt"
                log.parent.mkdir()
                lines = ["LogNiagara: Compiling System NiagaraSystem "
                         f"/DBAPresentationPack_Core/VFX/IceArrow/Niagara/NS_DBA_IceArrow_{stage}."
                         f"NS_DBA_IceArrow_{stage} took 0.1 sec (time since issued)."
                         for stage in STAGES if stage != missing_stage]
                lines.append(extra)
                if completed:
                    lines.extend(("LogCook: Display: Cooked packages 42 Packages Remain 0 Total 42",
                                  "LogInit: Display: Success - 0 error(s), 0 warning(s)"))
                log.write_text("\n".join(lines), encoding="utf-8")
            return validate(csv_path)

    def test_clean_cook_and_complete_exports(self):
        self.assertTrue(self.check()["Passed"])

    def test_gpu_error_disguised_as_warning_rejected(self):
        # 真实缺陷：UE 的总错误数仍为0，但GPU编译失败被记作LogNiagara Warning。
        self.assertFalse(self.check(
            "LogNiagara: Warning: /Engine/Generated/NiagaraEmitterInstance.ush:455:33: "
            "error: use of undeclared identifier 'Index'")["Passed"])

    def test_niagara_compile_failure_context_rejected(self):
        self.assertFalse(self.check(
            'LogNiagara: Display: There were issues for compilation of script '
            '"NS_DBA_IceArrow_Flight/Core/ParticleGPUComputeScript"')["Passed"])

    def test_shader_compiler_error_rejected(self):
        self.assertFalse(self.check("LogShaderCompilers: Error: Shader compile failed")["Passed"])

    def test_missing_stage_evidence_rejected(self):
        self.assertFalse(self.check(missing_stage="Flight")["Passed"])

    def test_incomplete_cook_rejected(self):
        self.assertFalse(self.check(completed=False)["Passed"])

    def test_missing_cook_log_rejected(self):
        self.assertFalse(self.check(log_present=False)["Passed"])

    def test_unrelated_environment_warning_allowed(self):
        self.assertTrue(self.check("LogZenServiceInstance: Warning: sponsor unavailable")["Passed"])

    def test_index_input_is_an_independent_ue58_token(self):
        # UE5.8 NiagaraNodeCustomHlsl.cpp 的分隔符不含 %；Index%128 无法替换成 In_Index。
        # 只借用锁定词法边界，不复制项目修复实现，真实着色器仍须由同次Cook日志验收。
        source = ROOT / "Tools/Unreal/VFX/DevelopmentMagic/AuthorIceArrowNiagara.py"
        spec = importlib.util.spec_from_file_location("ice_arrow_authoring_source", source)
        author = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(author)
        hlsl = re.sub(r"//[^\n]*|/\*.*?\*/", "", author.HLSL, flags=re.S)
        tokens = re.split(r'[!;/*+\-=)(?:, \[\]<>"\t\r\n{}]', hlsl)
        broken = [token for token in tokens if re.search(r"\bIndex\b", token) and token != "Index"]
        self.assertEqual([], broken, "Index必须是独立token，才能被UE替换为函数输入In_Index")
        self.assertIn("Index", tokens)


if __name__ == "__main__":
    unittest.main()
