# -*- coding: utf-8 -*-
"""验证工作室备份的失败封闭与哈希完整性，只操作Saved独占测试目录。

使用真实引擎保存UMAP的复制件，不伪造UE文件；不改正式地图或原始回退件。
不完整目录、缺失复制件不得被当成有效备份；测试目录保留以供人工复核。
"""
import importlib.util
from pathlib import Path
import uuid
import unreal


def main():
    """目录存在不代表备份完成；验证失败不能修改正式场景。"""
    spec=importlib.util.spec_from_file_location('sanctuary_backup_probe',Path(unreal.Paths.project_dir()).resolve().parent/'Tools/Unreal/FrontEnd/ImportZodiacSanctuary.py')
    module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    root=Path(unreal.Paths.project_saved_dir()).resolve()/'Validation/ZodiacSanctuary/BackupRegression'/uuid.uuid4().hex
    incomplete=root/'Incomplete';incomplete.mkdir(parents=True)
    assert hasattr(module,'ensure_complete_backup'), '尚无备份完整性验证，目录存在即被跳过'
    refused=False
    try:
        module.ensure_complete_backup(incomplete)
    except AssertionError:
        refused=True
    assert refused and not (incomplete/'L_DBA_CharacterStudio.umap').exists(), '不完整备份被接受或被悄悄补写'
    complete=root/'Complete';module.ensure_complete_backup(complete)
    module.ensure_complete_backup(complete)
    saved=complete/'L_DBA_CharacterStudio.umap'
    assert saved.is_file() and (complete/'BackupComplete.json').is_file(), '完整复制未发布'
    saved.rename(saved.with_suffix('.umap.retained'))
    refused=False
    try:
        module.ensure_complete_backup(complete)
    except AssertionError:
        refused=True
    assert refused, '标记存在却缺少备份资产仍被接受'
    print('SANCTUARY_BACKUP_COMPLETENESS_OK')


if __name__=='__main__':
    main()
