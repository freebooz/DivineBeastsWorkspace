"""具名操作只注册稳定ID和中文标签；实际执行仍由对应适配的编译白名单约束。"""
import importlib.util
from pathlib import Path
import unittest

HERE=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('named_operation_fixture',HERE/'TestPluginDemoManifest.py')
fixture=importlib.util.module_from_spec(spec); spec.loader.exec_module(fixture)
spec=importlib.util.spec_from_file_location('named_operation_validator',HERE/'ValidatePluginDemoManifest.py')
validator=importlib.util.module_from_spec(spec); spec.loader.exec_module(validator)


class NamedOperationTests(unittest.TestCase):
    def document(self):
        document=fixture.make_manifest()
        document['scenarios'][0]['operations']=['AcquireDefinition','Inspect','Stop']
        document['scenarios'][0]['operation_labels']={'AcquireDefinition':'申请定义','Inspect':'查看实际状态'}
        return document

    def test_named_operations_with_readable_labels_are_accepted(self):
        self.assertEqual([],validator.validate_manifest(self.document(),fixture.WORKSPACE))

    def test_custom_operations_cannot_omit_labels(self):
        document=self.document(); document['scenarios'][0].pop('operation_labels')
        self.assertTrue(any(i['code']=='operation_label_missing' for i in validator.validate_manifest(document,fixture.WORKSPACE)))

    def test_label_cannot_bind_to_an_undeclared_operation(self):
        document=self.document(); document['scenarios'][0]['operation_labels']['Other']='其他'
        self.assertTrue(any(i['code']=='operation_label_unknown' for i in validator.validate_manifest(document,fixture.WORKSPACE)))

    def test_script_text_is_never_a_registered_operation_identity(self):
        document=self.document(); document['scenarios'][0]['operations'][0]='AcquireDefinition;quit'
        self.assertTrue(validator.validate_manifest(document,fixture.WORKSPACE))

    def test_empty_or_oversized_labels_are_rejected(self):
        for label in ['', '   ', '字'*81, 123]:
            with self.subTest(label=label):
                document=self.document(); document['scenarios'][0]['operation_labels']['Inspect']=label
                self.assertTrue(validator.validate_manifest(document,fixture.WORKSPACE))

    def test_case_aliases_cannot_hide_duplicate_fname_label_keys(self):
        document=self.document(); document['scenarios'][0]['operation_labels']['inspect']='重复标识'
        self.assertTrue(any(i['code']=='duplicate_id' for i in validator.validate_manifest(document,fixture.WORKSPACE)))


if __name__=='__main__':
    unittest.main()
