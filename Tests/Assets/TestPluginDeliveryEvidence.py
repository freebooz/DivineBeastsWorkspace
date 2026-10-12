#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""插件交付证据门禁回归；临时目录只模拟报告文件，绝不替代真实UE结果或人工签审。"""
from __future__ import annotations
import hashlib
import importlib.util
import json
import tempfile
import unittest
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
POLICY=ROOT/'Build/Validation/PluginDeliveryEvidence.py'
if POLICY.is_file():
    spec=importlib.util.spec_from_file_location('plugin_delivery_evidence',POLICY)
    module=importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    spec.loader.exec_module(module)
else:
    module=None

class MissingPolicyTest(unittest.TestCase):
    def test_policy_implementation_exists(self):
        self.assertTrue(POLICY.is_file(),'尚未实现交付证据门禁，不能把缺少检查当成通过')

@unittest.skipIf(module is None,'先由上面的显式失败用例阻断缺少实现')
class DeliveryEvidenceTest(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name)
        self.src=self.root/'Source/input.cpp'
        self.src.parent.mkdir(parents=True)
        self.src.write_text('// evidence input\n',encoding='utf-8')
        self.inputs={'Source/input.cpp':self.sha(self.src)}
    @staticmethod
    def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
    def write(self,value,name='Saved/Validation/report.json'):
        p=self.root/name;p.parent.mkdir(parents=True,exist_ok=True)
        p.write_text(json.dumps(value),encoding='utf-8')
        return {'path':name,'sha256':self.sha(p)}
    def evidence(self,tests=None):
        tests=tests if tests is not None else [{'fullTestPath':'GamePlatform.SFX.Policy.Valid','state':'Success','errors':0,'warnings':0}]
        return self.write({'tests':tests,'succeeded':sum(x['state']=='Success' for x in tests),'failed':sum(x['state']!='Success' for x in tests)})
    def assess(self,ev=None,**kwargs):
        return module.verify_automation(self.root,ev or self.evidence(),self.inputs,('GamePlatform.SFX.',),**kwargs)
    def test_success_requires_a_real_nonempty_report(self):
        result=self.assess();self.assertEqual(result['status'],'passed');self.assertEqual(result['passed'],1)
    def test_success_with_warnings_is_not_counted_as_zero(self):
        ev=self.evidence([{'fullTestPath':'GamePlatform.SFX.Policy.Valid','state':'Success','errors':0,'warnings':1}])
        self.assertEqual(self.assess(ev)['warning_cases'],1)
    def test_missing_report_fails_closed(self):
        self.assertEqual(self.assess({'path':'absent.json','sha256':'0'*64})['status'],'blocked')
    def test_wrong_hash_fails_closed(self):
        ev=self.evidence();ev['sha256']='0'*64
        self.assertEqual(self.assess(ev)['status'],'blocked')
    def test_edited_input_invalidates_the_evidence(self):
        ev=self.evidence();self.src.write_text('// changed',encoding='utf-8')
        self.assertEqual(self.assess(ev)['status'],'blocked')
    def test_empty_test_list_never_passes(self):
        self.assertEqual(self.assess(self.evidence([]))['status'],'failed')
    def test_missing_tests_array_never_passes(self):
        self.assertEqual(self.assess(self.write({'succeeded':15,'failed':0}))['status'],'failed')
    def test_wrong_test_group_never_passes(self):
        ev=self.evidence([{'fullTestPath':'Unrelated.Pass','state':'Success','errors':0,'warnings':0}])
        self.assertEqual(self.assess(ev)['status'],'failed')
    def test_failed_case_is_not_hidden_by_summary(self):
        ev=self.write({'succeeded':10,'failed':0,'tests':[{'fullTestPath':'GamePlatform.SFX.Bad','state':'Fail','errors':1,'warnings':0}]})
        self.assertEqual(self.assess(ev)['status'],'failed')
    def test_success_state_with_errors_still_fails(self):
        ev=self.evidence([{'fullTestPath':'GamePlatform.SFX.Bad','state':'Success','errors':1,'warnings':0}])
        self.assertEqual(self.assess(ev)['status'],'failed')
    def test_not_run_case_fails(self):
        ev=self.evidence([{'fullTestPath':'GamePlatform.SFX.Pending','state':'NotRun','errors':0,'warnings':0}])
        self.assertEqual(self.assess(ev)['status'],'failed')
    def test_duplicate_test_identity_fails(self):
        t={'fullTestPath':'GamePlatform.SFX.Duplicate','state':'Success','errors':0,'warnings':0}
        self.assertEqual(self.assess(self.evidence([t,t]))['status'],'failed')
    def test_missing_one_required_prefix_fails(self):
        r=module.verify_automation(self.root,self.evidence(),self.inputs,('GamePlatform.SFX.','GamePlatform.VFX.'))
        self.assertEqual(r['status'],'failed')
    def test_parent_traversal_not_allowed(self):
        self.assertEqual(self.assess({'path':'../report.json','sha256':'0'*64})['status'],'blocked')
    def test_absolute_path_not_allowed(self):
        self.assertEqual(self.assess({'path':str(self.src),'sha256':self.sha(self.src)})['status'],'blocked')
    def test_missing_input_baseline_not_allowed(self):
        r=module.verify_automation(self.root,self.evidence(),{},('GamePlatform.SFX.',))
        self.assertEqual(r['status'],'blocked')
    def test_manual_requires_user_confirmation(self):
        r=module.verify_manual({'status':'Passed','reviewer':'Tester','explicit_human_confirmation':False,'evidence':['frame.png']})
        self.assertEqual(r['status'],'not_run')
    def test_blank_reviewer_never_passes(self):
        r=module.verify_manual({'status':'Passed','reviewer':' ','explicit_human_confirmation':True,'evidence':['frame.png']})
        self.assertEqual(r['status'],'blocked')
    def test_manual_requires_observation_evidence(self):
        r=module.verify_manual({'status':'Passed','reviewer':'Tester','explicit_human_confirmation':True,'evidence':[]})
        self.assertEqual(r['status'],'blocked')
    def test_manual_failure_is_preserved(self):
        r=module.verify_manual({'status':'Failed','reviewer':'Tester','explicit_human_confirmation':True,'evidence':['frame.png']})
        self.assertEqual(r['status'],'failed')
    def test_overall_cannot_pass_with_pending_manual(self):
        r=module.combine_gates({'G0':{'status':'passed'},'G5':{'status':'not_run'}},('G0','G5'))
        self.assertFalse(r['complete'])
    def test_missing_gate_blocks_overall(self):
        self.assertFalse(module.combine_gates({'G0':{'status':'passed'}},('G0','G1'))['complete'])
    def test_nonapplicable_requires_reason(self):
        self.assertFalse(module.combine_gates({'G1':{'status':'not_applicable'}},('G1',))['complete'])
    def test_nonapplicable_with_reason_is_accepted(self):
        self.assertTrue(module.combine_gates({'G1':{'status':'not_applicable','reason':'纯内容包没有独立C++目标'}},('G1',))['complete'])

class MissingContainerPolicyTest(unittest.TestCase):
    def test_container_policy_exists(self):
        self.assertTrue(hasattr(module,'verify_content_container'),'必须读取最终容器，不以源声音存在推断发布成功')

@unittest.skipUnless(hasattr(module,'verify_content_container'),'由显式缺失用例阻断尚未实现')
class ContentContainerPolicyTest(unittest.TestCase):
    ROOT='/Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBASFXPack_Core/Content/Weather/'
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.csv=Path(self.temp.name)/'Container.csv'
        self.required=[self.ROOT+'Definitions/DA_DBA_SFX_Weather_Rain.uasset',
            self.ROOT+'Definitions/DA_DBA_SFX_Weather_Snow.uasset',
            self.ROOT+'Audio/SW_DBA_Weather_RainHeavy_Loop.uasset',
            self.ROOT+'Audio/SW_DBA_Weather_Wind_Loop.uasset']
    def write(self,files,platform='WindowsClient'):
        self.csv.write_text('Filename,Platform\n'+''.join(p+','+platform+'\n' for p in files),encoding='utf-8')
    def test_all_referenced_client_sound_assets_exist(self):
        self.write(self.required)
        result=module.verify_content_container(self.csv,'Client')
        self.assertEqual(result['status'],'passed');self.assertEqual(result['required_count'],4)
    def test_missing_referenced_sound_is_rejected(self):
        self.write(self.required[:-1]);self.assertEqual(module.verify_content_container(self.csv,'Client')['status'],'failed')
    def test_editor_only_unused_preview_sound_is_not_required(self):
        self.write(self.required);self.assertEqual(module.verify_content_container(self.csv,'Client')['status'],'passed')
    def test_server_rejects_all_sound_pack_files(self):
        self.write([self.required[0]],'WindowsServer')
        self.assertEqual(module.verify_content_container(self.csv,'Server')['status'],'failed')
    def test_server_accepts_without_client_audio(self):
        self.write(['/Game/Content/Maps/Server.umap'],'WindowsServer')
        self.assertEqual(module.verify_content_container(self.csv,'Server')['status'],'passed')
    def test_development_review_map_is_forbidden(self):
        self.write(self.required+['/Game/Content/Development/Foundation/PluginReviews/Maps/L_GP_SFXReview.umap'])
        self.assertEqual(module.verify_content_container(self.csv,'Client')['status'],'failed')
    def test_external_development_objects_are_forbidden(self):
        self.write(self.required+['/Game/Content/__ExternalActors__/Development/Foundation/PluginReviews/Maps/A.uasset'])
        self.assertEqual(module.verify_content_container(self.csv,'Client')['status'],'failed')
    def test_case_and_separator_normalization(self):
        self.write([p.upper().replace('/','\\') for p in self.required])
        self.assertEqual(module.verify_content_container(self.csv,'Client')['status'],'passed')
    def test_wrong_platform_is_rejected(self):
        self.write(self.required,'WindowsServer')
        self.assertEqual(module.verify_content_container(self.csv,'Client')['status'],'blocked')
    def test_empty_container_is_rejected(self):
        self.write([]);self.assertEqual(module.verify_content_container(self.csv,'Client')['status'],'blocked')
    def test_unknown_target_is_rejected(self):
        self.write(self.required);self.assertEqual(module.verify_content_container(self.csv,'Unknown')['status'],'blocked')

if __name__=='__main__':unittest.main(verbosity=2)
