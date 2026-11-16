"""通过直接 Monolith 修正开发测试页的长文本换行，保持字号与既有布局。"""
from pathlib import Path
import json
import os
import time
import unreal

ROOT = Path(__file__).resolve().parents[3]
PREFIX = '/DBAUIPack_Core/Development/PluginDemos/UI/WBP_DBA_PluginDemo'
FIELDS = {
    'Detail': ['HeaderText', 'PanelDetailsText', 'FooterText'],
    'Controls': ['HeaderText', 'PanelStatusText', 'SampleTitle', 'FooterText'],
    'Report': ['HeaderText', 'PanelReportText', 'PanelEvidencePathText', 'PanelStatusText', 'FooterText'],
    'MenuRow': ['LabelText', 'StatusText'],
    'ParameterRow': ['ParameterLabelText', 'ParameterErrorText'],
}


def author():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    if world is not None:
        raise RuntimeError('先停止本任务 PIE 再修改共享开发 Widget')
    packages = [PREFIX + suffix for suffix in FIELDS]
    dirty = unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages() + unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    foreign = [p.get_path_name() for p in dirty if p.get_path_name() not in packages]
    if foreign:
        raise RuntimeError('保留其它脏包，拒绝修改: ' + repr(foreign))
    objects = []
    for suffix, names in FIELDS.items():
        bp = unreal.load_asset(PREFIX + suffix)
        if not isinstance(bp, unreal.WidgetBlueprint):
            raise RuntimeError('缺少真实 WidgetBlueprint: ' + suffix)
        widgets = [unreal.find_object(None, bp.get_path_name() + ':WidgetTree.' + name) for name in names]
        if any(not isinstance(w, unreal.TextBlock) for w in widgets):
            raise RuntimeError('既有文本节点不匹配: ' + suffix)
        objects.append((bp, widgets))
    changed = []
    for bp, widgets in objects:
        bp.modify()
        for widget in widgets:
            font = widget.get_editor_property('font').export_text()
            widget.modify()
            widget.set_editor_property('auto_wrap_text', True)
            widget.set_editor_property('wrapping_policy', unreal.TextWrappingPolicy.ALLOW_PER_CHARACTER_WRAPPING)
            assert font == widget.get_editor_property('font').export_text()
            changed.append(widget.get_path_name())
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        if bp.status != unreal.BlueprintStatus.BS_UP_TO_DATE:
            raise RuntimeError('真实编译失败: ' + bp.get_path_name())
    report = {'editor_pid': os.getpid(), 'author_epoch': time.time(), 'packages': packages,
              'text_nodes': changed, 'font_preserved': True, 'auto_wrap': True,
              'wrapping_policy': 'AllowPerCharacterWrapping', 'save': 'requires_exact_monolith_save_packages',
              'native_render': 'pending', 'human_review': 'pending'}
    path = ROOT / 'Saved/Validation/PluginManualUI20261027/TextWrap.Authored.json'
    path.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({'packages': len(packages), 'text_nodes': len(changed), 'compiled': True, 'font_preserved': True}))


if __name__ == '__main__':
    author()
