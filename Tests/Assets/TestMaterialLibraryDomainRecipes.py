#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""材质函数库60原生配方、12材质层和7项UI配方的统一静态门禁。

本测试不进入UE编辑器、不生成二进制，不执行Shader编译/Cook；仅检查源码
算法、三层所有权、命名、201项单一真源与已有第一/二批正式函数不冲突。
统一测试必须在代码、蓝图和资源制作完成后再由发布门禁执行。
"""
from __future__ import annotations

import importlib.util
import json
from collections import Counter
from pathlib import Path
import unittest

ROOT=Path(__file__).resolve().parents[2]
AUTHOR=ROOT/"Tools/Unreal/Surface/AuthorMaterialLibraryDomainUE58.py"
LAYERS=ROOT/"Tools/Unreal/Surface/MaterialLibraryLayerProfiles_V1.json"
UI=ROOT/"Tools/Unreal/UI/MaterialLibraryUIMonolithRecipes_V1.json"
REGISTRY=ROOT/"Docs/Implementation/Materials/MaterialCapabilityRegistry_V1.json"
FIRST=ROOT/"Docs/Implementation/Materials/MaterialReviewShaderEvidence_UE58_20261013.json"
SECOND=ROOT/"Docs/Implementation/Materials/MaterialBatch2ShaderAndUIEvidence_UE58_20261013.json"
DEMO=ROOT/"Docs/Implementation/Materials/MaterialLibraryBlueprintAndUIManifest_V1.json"


def read_json(path):
    """将每份来源与当前仓库文件对应，不接受来源不明的测试替身。"""
    return json.loads(path.read_text(encoding="utf-8"))


def load_author():
    """使用标准importlib只读加载真实项目作者模块，避免触发UE写入。"""
    spec=importlib.util.spec_from_file_location("domain_material_library_author",AUTHOR)
    if spec is None or spec.loader is None:
        raise RuntimeError("缺少真实材质领域函数源码")
    module=importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class MaterialLibraryDomainRecipes(unittest.TestCase):
    """完整覆盖真实能力清单，并保持资产生成与验收边界。"""

    @classmethod
    def setUpClass(cls):
        cls.author=load_author()
        cls.items=cls.author.load_catalog()
        cls.registry=read_json(REGISTRY)
        cls.rows={i["CapabilityId"]:i for i in cls.registry["items"]}

    def test_60_meaningful_recipes_have_unique_stable_paths(self):
        """60个必须真正有合法计算树，不允许同路径覆盖或生成直通占位。"""
        self.assertEqual(len(self.items),60)
        self.assertEqual(len({s["id"] for s in self.items}),60)
        paths=[self.author.asset_path(s) for s in self.items]
        self.assertEqual(len(paths),len(set(paths)))
        for entry in self.items:
            with self.subTest(capability=entry["id"]):
                self.author.assert_valid_recipe(entry)
                self.assertIn(entry["id"],self.rows)
                self.assertTrue(entry["outputs"])
                self.assertTrue(entry["zh"])
                self.assertFalse(any(x["expression"] in (None,[],{}) for x in entry["outputs"]))

    def test_three_layer_ownership_and_prefixes(self):
        """平台无硬引用项目技能/纹理；项目视觉函数只写第三层内容包。"""
        owners=Counter(s["owner"] for s in self.items)
        self.assertEqual(dict(owners),{"Surface":46,"VFX":4,"Project":10})
        self.assertEqual(sum(owners.values()),60)
        for item in self.items:
            path=self.author.asset_path(item)
            if item["owner"]=="Surface":
                self.assertTrue(path.startswith("/GamePlatformSurface/MaterialFunctions/"))
            elif item["owner"]=="VFX":
                self.assertTrue(path.startswith("/GamePlatformVFX/Materials/Functions/"))
            elif item["owner"]=="Project":
                self.assertTrue(path.startswith("/DBAPresentationPack_Core/MaterialFunctions/Character/"))
            else:
                self.fail("未知跨层资源Owner")

    def test_no_collision_with_verified_first_and_second_buckets(self):
        """新函数不得覆盖之前实际编译过的8+18项。"""
        old=read_json(FIRST)
        second=read_json(SECOND)
        previously={x["materialFunctionPath"] for x in old["records"]}
        previously.update(x["assetPath"] for x in second["newSecondBatch"]["records"])
        now={self.author.asset_path(s) for s in self.items}
        self.assertEqual(len(previously),26)
        self.assertFalse(previously & now,"新配方会与历史正式资产冲突")

    def test_material_layer_and_ui_delegation_contracts(self):
        """12项材质层与7项界面函数必须归既有插件，不得再创建没有独立职责的空壳插件。"""
        layers=read_json(LAYERS)
        ui=read_json(UI)
        self.assertEqual(len(layers["items"]),12)
        self.assertEqual(len(ui["items"]),7)
        observed=set()
        for layer in layers["items"]:
            row=self.rows[layer["capabilityId"]]
            self.assertEqual(row["ImplementationMode"],"ML")
            self.assertEqual(row["OwnerPlugin"],"GamePlatformSurface")
            self.assertIn(layer["capabilityId"],self.rows)
            self.assertTrue(layer["materialLayerAssetPath"].startswith("/GamePlatformSurface/MaterialLayers/ML_GP_"))
            observed.add(layer["capabilityId"])
        for item in ui["items"]:
            row=self.rows[item["capabilityId"]]
            self.assertEqual(row["ImplementationMode"],"MF")
            self.assertEqual(row["OwnerPlugin"],"GamePlatformUI")
            self.assertEqual(row["NameEN"],item["name"])
            self.assertEqual(len(item["graphSpec"]["outputs"]),1 if item["name"]!="UITransitionDissolve" else 2)
            self.assertGreaterEqual(len(item["graphSpec"]["connections"]),3)
            observed.add(item["capabilityId"])
        self.assertEqual(len(observed),19)

    def test_demo_assets_are_isolated_and_appropriately_owned(self):
        """开发Review场景不得替代正式Village、MainArena或客户端UI生产资产。"""
        demo=read_json(DEMO)
        self.assertEqual(len(demo["maps"]),4)
        for item in demo["maps"]:
            self.assertTrue(item["map"].startswith("/Game/Development/MaterialLibrary/"))
            self.assertIn(item["owner"],{
                "GamePlatformSurface","GamePlatformVFX","GamePlatformUI","DBAPresentationPack_Core"})
        self.assertTrue(demo["uiWidget"]["assetPath"].startswith("/DBAUIPack_Core/"))
        self.assertTrue(demo["standBlueprint"]["assetPath"].startswith("/Game/Development/MaterialLibrary/"))

    def test_native_material_layer_factories_are_class_specific(self):
        """先制作资产时必须用UE原生层/混合层工厂，防止错误类的二进制资产占用包名。"""
        author = (ROOT/"Tools/Unreal/Surface/AuthorMaterialLibraryLayersUE58.py"
                 ).read_text(encoding="utf-8")
        self.assertIn("unreal.MaterialFunctionMaterialLayerFactory()",author)
        self.assertIn("unreal.MaterialFunctionMaterialLayerBlendFactory()",author)
        self.assertNotIn("unreal.MaterialFunctionFactoryNew()",author,
                         "混合层与材质层不得调用通用MF工厂")
        self.assertIn("MaterialFunctionMaterialLayerBlend",author)
        self.assertIn("MaterialFunctionMaterialLayer",author)
        self.assertIn("native_file.is_file()",author)
        self.assertIn("get_num_material_expressions_in_function(obj)",author)
        self.assertIn("minimum=5 if path==BLEND else 7",author)

    def test_review_stand_blueprint_has_real_member_contract_and_final_compile_gate(self):
        """审查展示Actor应暴露可编辑材质软引用和中文标题；只有finalize才编译。"""
        author = (ROOT/"Tools/Unreal/Surface/AuthorMaterialLibraryStandBlueprintUE58.py"
                 ).read_text(encoding="utf-8")
        for token in ("ReviewMaterial","ReviewTitle","BlueprintEditorLibrary",
                      "list_member_variable_names", "add_member_variable",
                      "set_blueprint_variable_instance_editable",
                      "set_blueprint_variable_expose_on_spawn",
                      "if not unreal.BlueprintEditorLibrary.compile_blueprint(bp)"):
            self.assertIn(token,author)
        self.assertIn('if opts.mode=="finalize":',author)
        self.assertIn('preview.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)',author)

    def test_ui_action_queue_is_create_only_and_has_all_seven_functions(self):
        """UI功能必须通过Monolith创建而不能通过脚本假装已有UMaterialFunction。"""
        queue = read_json(ROOT/"Docs/Implementation/Materials/MaterialLibraryUIActionQueue_V1.json")
        self.assertEqual(queue.get("expectedFunctions"),7)
        self.assertEqual(len(queue.get("items",[])),7)
        for item in queue["items"]:
            self.assertEqual(len(item["operations"]),6)
            # 测试在首次制作后还需继续有效；动作被真实执行时必须附带证据。
            for action in item["operations"]:
                self.assertIn("status",action)
                if action["status"] != "not_executed":
                    self.assertTrue(action.get("evidence"),"执行状态变更必须带真实UE证据")
            calls = [x["action"] for x in item["operations"]]
            self.assertEqual(calls,["get_function_info","create_material_function",
                                    "build_function_graph","set_function_metadata",
                                    "save_material","get_function_info"])
            graph = item["operations"][2]["parameters"]["graph_spec"]
            self.assertFalse(item["operations"][2]["parameters"]["clear_existing"])
            self.assertGreaterEqual(len(graph["outputs"]),1)
            self.assertLessEqual(len(graph["outputs"]),2)
        self.assertEqual(len({item["capabilityId"] for item in queue["items"]}),7)

    def test_four_review_maps_are_development_only_and_not_claimed_created(self):
        """四张开发地图必须以UE World身份新建，绝不打开并覆盖正式游戏世界。"""
        queue = read_json(ROOT/"Docs/Implementation/Materials/MaterialLibraryMapActionQueue_V1.json")
        self.assertEqual(queue["count"],4)
        self.assertEqual({x["area"] for x in queue["items"]},
                         {"Surface","VFX","Project","UI"})
        for item in queue["items"]:
            self.assertTrue(item["path"].startswith("/Game/Development/MaterialLibrary/"))
            self.assertIn("status",item)
            if item["status"] != "not_executed":
                self.assertTrue(item.get("evidence"),"地图交付状态只能由真实UE注册表证据提升")
            self.assertEqual(item["steps"][1]["tool"],"create_empty_map")
            self.assertFalse(item["steps"][1]["params"]["open"])
            self.assertEqual(item["steps"][1]["params"]["dirty_policy"],"refuse")

    def test_twelve_ui_material_review_author_stays_development_only(self):
        """用户界面真实母材质作者仅生成MD_UI开发审核资产，不绕开平台主题或提前编译。"""
        path = ROOT/"Tools/Unreal/UI/AuthorMaterialLibraryUIReviewUE58.py"
        spec=importlib.util.spec_from_file_location("ui_material_review_author",path)
        self.assertIsNotNone(spec)
        self.assertIsNotNone(spec.loader)
        module=importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        items=module.load_specs()
        self.assertEqual(len(items),12)
        self.assertEqual(len({x["name"] for x in items}),12)
        self.assertEqual(set(module.EXISTING),{
            "SDFRoundRect","GradientBackground","RadialCooldown","OutlineGlow","ButtonShimmer"})
        for item in items:
            self.assertTrue(module.qa_path(item["name"]).startswith(
                "/Game/Development/MaterialLibrary/UI/Materials/"))
            self.assertTrue(any(row["NameEN"] == item["name"] and
                                row["OwnerPlugin"] == "GamePlatformUI"
                                for row in self.registry["items"]))
        content=path.read_text(encoding="utf-8")
        for native_api in ("get_material_function_expressions", "set_material_function(fn)",
                           "MaterialDomain.MD_UI", "BLEND_TRANSLUCENT",
                           "MP_EMISSIVE_COLOR", "MP_OPACITY","save_loaded_asset"):
            self.assertIn(native_api,content)
        self.assertNotIn("recompile_material",content,
                         "用户要求全部资源制作后再统一执行Shader编译")

    def test_material_library_ui_widget_spec_has_twelve_cards_and_real_demo_parent(self):
        """12个UI审查卡片应使用既有项目DemoHub基类；真实WBP仅通过Monolith制作。"""
        spec_path = ROOT/"Docs/Implementation/Materials/MaterialLibraryUIWidgetSpec_V1.json"
        data = read_json(spec_path)
        self.assertEqual(data["name"],"WBP_DBA_MaterialLibraryReview")
        self.assertEqual(data["parentClass"],
                         "/Script/DivineBeastsDemoClient.DivineBeastsDemoHubWidget")
        ids=[]
        def walk(node):
            ids.append(node["id"])
            for child in node.get("children",[]):
                walk(child)
        walk(data["rootWidget"])
        self.assertEqual(len(ids),len(set(ids)))
        self.assertGreaterEqual(len(ids),80)
        for required in ("ReturnButton","OverlayFeedbackText",
                         "StatusText","ReviewScroll","UIReviewImageCards"):
            self.assertIn(required,ids)
        info=data["metadata"]["pendingWidgetMaterialPaths"]
        self.assertEqual(len(info),12)
        self.assertEqual(len({entry["nameEN"] for entry in info}),12)
        for entry in info:
            self.assertIn("Preview_"+entry["nameEN"],ids)
            self.assertTrue(entry["reviewMaterial"].startswith(
                "/Game/Development/MaterialLibrary/UI/Materials/"))
        author = (ROOT/"Tools/Unreal/UI/AuthorMaterialLibraryWidgetMonolith.py"
                 ).read_text(encoding="utf-8")
        self.assertIn('from BirdFlockMonolithProtocol import call,editor_python',author)
        self.assertIn('"action":"build_ui_from_spec"',author)
        self.assertIn('"action":"get_widget_tree"',author)
        self.assertIn('does_asset_exist',author)
        self.assertNotIn('"action":"compile_widget"',author)

    def test_native_asset_authors_lock_exact_project_and_detect_incomplete_review_maps(self):
        """审查四类原生作者：完整路径+引擎版门禁，半成品地图不能被报告为完成。"""
        authors=(
            "AuthorMaterialLibraryDomainUE58.py",
            "AuthorMaterialLibraryLayersUE58.py",
            "AuthorMaterialLibraryStandBlueprintUE58.py",
            "AuthorMaterialLibraryDemoUE58.py",
        )
        for filename in authors:
            with self.subTest(source=filename):
                text=(ROOT/"Tools/Unreal/Surface"/filename).read_text(encoding="utf-8")
                self.assertIn("os.path.normcase(os.path.realpath(",text)
                self.assertIn("get_engine_version()",text)
                self.assertIn("5.8.",text)
        demo=(ROOT/"Tools/Unreal/Surface/AuthorMaterialLibraryDemoUE58.py").read_text(encoding="utf-8")
        self.assertIn("REVIEW_MAP_CONTENT_VERIFIED",demo)
        self.assertIn("expected_labels-set(review_labels)",demo)
        self.assertIn("preview.get_material(0).get_path_name()",demo)
        self.assertIn("禁止在同一地图叠加重复演示物",demo)

    def test_editor_recovery_keeps_build_scope_explicit_and_uses_readonly_inspection(self):
        """恢复入口不得悄悄扩大为服务器/客户端构建，诊断入口不启动或篡改UE产物。"""
        recovery=(ROOT/"Build/Game/RecoverUE58EditorForMaterialLibrary.ps1").read_text(encoding="utf-8")
        launcher=(ROOT/"Build/Game/RecoverUE58EditorForMaterialLibrary.cmd").read_text(encoding="utf-8")
        inspection=(ROOT/"Build/Game/InspectUE58EditorModuleState.ps1").read_text(encoding="utf-8")
        self.assertIn("OnlyModules",recovery)
        self.assertIn("if ($SkipBuild -and $OnlyModules.Count)",recovery)
        self.assertIn("NoUBA",recovery)
        self.assertIn("-NoCompile",recovery)
        self.assertIn('-File "%~dp0RecoverUE58EditorForMaterialLibrary.ps1" -SkipBuild',launcher)
        self.assertIn("exit /b %RESULT%",launcher)
        for key in ("BuildIdMismatches","PluginModuleCandidates","ProjectEditorManifest",
                    "UnrealProcesses","WriteReport"):
            self.assertIn(key,inspection)
        self.assertNotIn("Build.bat",inspection)
        self.assertNotIn("Start-Process",inspection)

    def test_p0_p10_native_production_routes_are_complete_and_resumable(self):
        """核对真正制作入口的数量、归属、断点及Monolith调用顺序；离线不造二进制。"""
        def load(filename, key):
            spec=importlib.util.spec_from_file_location(key, filename)
            self.assertIsNotNone(spec)
            self.assertIsNotNone(spec.loader)
            module=importlib.util.module_from_spec(spec)
            spec.loader.exec_module(module)
            return module
        src=ROOT/"Tools/Unreal/Surface"
        ui=ROOT/"Tools/Unreal/UI"
        runner=load(src/"RunMaterialLibraryNativeBatchesMonolith.py", "sslm_native_runner")
        tasks=runner.plan(3)
        self.assertEqual(sum(t[3] for t in tasks if t[0]=="functions"),60)
        self.assertEqual(sum(t[3] for t in tasks if t[0]=="layers"),12)
        self.assertEqual(sum(t[3] for t in tasks if t[0]=="blueprint"),1)
        self.assertEqual(sum(t[3] for t in tasks if t[0]=="domain_reviews"),60)
        self.assertEqual(sum(t[3] for t in tasks if t[0]=="ui_reviews"),12)
        self.assertEqual({t[1] for t in tasks if t[0]=="functions"},
                         {"Surface","VFX","Project"})
        self.assertTrue(all(t[4].is_file() for t in tasks))
        ui_author=load(ui/"RunMaterialLibraryUIProductionMonolith.py","sslm_ui_runner")
        self.assertEqual(len(ui_author.load_queue()),7)
        maps=load(src/"RunMaterialLibraryReviewMapsMonolith.py","sslm_map_runner")
        self.assertEqual({x["area"] for x in maps.load_rows()},
                         {"Surface","VFX","Project","UI"})
        brushes=load(ui/"BindMaterialLibraryUIReviewBrushesMonolith.py","sslm_brush_runner")
        self.assertEqual(len(brushes.entries()),12)
        full=load(src/"RunMaterialLibraryCompleteResourcesUE58.py","sslm_full_runner")
        full.preflight_source()
        self.assertEqual([name for name,_,_ in full.PHASES],
                         ["domain_functions","material_layers","review_stand_blueprint",
                          "domain_review_materials","ui_seven_functions",
                          "ui_twelve_review_materials","ui_widget",
                          "ui_widget_material_brushes","review_maps_four"])
        for target in (
            src/"RunMaterialLibraryNativeBatchesMonolith.py",
            src/"RunMaterialLibraryReviewMapsMonolith.py",
            ui/"RunMaterialLibraryUIProductionMonolith.py",
            ui/"BindMaterialLibraryUIReviewBrushesMonolith.py",
        ):
            content=target.read_text(encoding="utf-8")
            self.assertIn("BirdFlockMonolithProtocol",content)
            self.assertIn("UE",content)
        widget=(ui/"AuthorMaterialLibraryWidgetMonolith.py").read_text(encoding="utf-8")
        self.assertIn("UI_WIDGET_12_CARD_EXISTING_NATIVE_PRESERVED",widget)
        self.assertIn("SSLM_WBP_STATE",widget)
        # Monolith实际创建前必须拒绝未注册侧车，禁止误盖其他UE原生包。
        for protected in (ui/"RunMaterialLibraryUIProductionMonolith.py",
                          src/"RunMaterialLibraryReviewMapsMonolith.py",
                          ui/"AuthorMaterialLibraryWidgetMonolith.py"):
            self.assertIn("stale=[os.path.splitext(disk)[0]+ext",
                          protected.read_text(encoding="utf-8"))
        self.assertNotIn('("compile_widget"',full.__dict__["__doc__"] or "")

    def test_native_batch_requires_actual_ue_files_and_generated_script_parses(self):
        """模拟Monolith响应，只校验生成的UE Python可解析及逐资产落盘门禁。"""
        path=ROOT/"Tools/Unreal/Surface/RunMaterialLibraryNativeBatchesMonolith.py"
        spec=importlib.util.spec_from_file_location("sslm_native_batch_probe",path)
        runner=importlib.util.module_from_spec(spec)
        spec.loader.exec_module(runner)
        examples=(("functions","Surface",0,3),("functions","VFX",0,3),
                  ("functions","Project",0,3),("layers","Surface",0,3),
                  ("blueprint","Project",0,1),("domain_reviews","Surface",0,3),
                  ("ui_reviews","UI",0,3))
        for stage,owner,start,count in examples:
            with self.subTest(stage=stage,owner=owner):
                contracts=runner.asset_contracts(stage,owner,start,count)
                self.assertEqual(len(contracts),count + (stage=="layers"))
                self.assertTrue(all(p.endswith(".uasset") for _,p,_,_ in contracts))
                identity="offline_"+stage+"_"+owner
                seen=[]
                def mock_engine(source):
                    compile(source,"<UE5.8 generated Monolith source>","exec")
                    seen.append(source)
                    return {"text":"SSLM_NATIVE_BATCH_SAVED:"+identity+":"+str(len(contracts))}
                runner.call_ue_script(mock_engine,runner.SCRIPTS[stage],
                                      ("--mode","apply"),identity,stage,owner,start,count)
                self.assertEqual(len(seen),1)
                self.assertIn("os.path.isfile(disk)",seen[0])
                self.assertIn("getattr(unreal,cls)",seen[0])
                with self.assertRaises(RuntimeError):
                    runner.call_ue_script(lambda _: {"text":"gateway_online_only"},
                                          runner.SCRIPTS[stage],("--mode","apply"),
                                          identity,stage,owner,start,count)

    def test_source_phase_strict_editor_identity_and_no_premature_build(self):
        """真实资产制作前应锁定UE5.8和工作空间，禁止PIE及不完整AssetRegistry。"""
        path = ROOT/"Tools/Unreal/Surface/AuthorMaterialLibrarySourcePhaseUE58.py"
        text = path.read_text(encoding="utf-8")
        for invariant in (
            "os.path.normcase(os.path.realpath(project))",
            "get_engine_version", "5.8.", "get_game_world()",
            "is_loading_assets()", "inspect_only()", "resume-from",
            "MaterialLayers", "ReviewStandBlueprint"):
            self.assertIn(invariant,text)
        # 脚本只调度制作；最终Editor/Client/Server编译、Cook由另一个门禁调用。
        self.assertNotIn('("--mode", "compile")',text)
        self.assertNotIn('("--mode", "finalize")',text)


if __name__=="__main__":
    unittest.main(verbosity=2)
