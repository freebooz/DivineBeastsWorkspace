"""直接Monolith中的独立原生页回读；只读资产，证据写当前工程Saved。

要求Editor进程不同于作者进程以证明保存后冷加载；不关闭Editor，不把回读当运行验收。
控件树/导航由直接Monolith ui_query另行审核，不通过加载同名文件推断控件存在。
"""
from __future__ import annotations
import importlib.util
import json
import os
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]


def validate_pages(require_fresh_editor: bool = True):
    spec = importlib.util.spec_from_file_location('plugin_page_author_readback', Path(__file__).with_name('AuthorIndependentPluginPagesMonolith.py'))
    helper = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(helper)
    document, fingerprint = helper.checked_source()
    plan = helper.make_page_plan(document)
    import unreal
    helper.check_editor(unreal)
    previous_path = helper.EVIDENCE / 'T07.Pages.Authored.json'
    previous = json.loads(previous_path.read_text(encoding='utf-8-sig'))
    if previous['status'] != 'saved_requires_independent_reload' or previous['source_fingerprint'] != fingerprint:
        raise RuntimeError('authoring did not complete for current source')
    if set(p['plugin_id'] for p in previous['pages']) != set(p['plugin_id'] for p in plan):
        raise RuntimeError('authoring omitted plugin pages')
    started_epoch = helper.current_process_started_epoch()
    fresh = helper.is_cold_reload(previous['editor_pid'], previous.get('saved_epoch', 0), os.getpid(), started_epoch)
    if require_fresh_editor and not fresh:
        raise RuntimeError('cold reload requires a fresh editor process; current author process cannot certify it')
    targets = {p['widget'] for p in plan} | {helper.HUB_DEFINITION}
    dirty = {p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()}
    if dirty.intersection(targets):
        raise RuntimeError('target has unsaved changes; saved assets cannot be certified')
    result = {'source_fingerprint': fingerprint, 'editor_pid': os.getpid(), 'author_pid': previous['editor_pid'],
              'reader_started_epoch': started_epoch, 'author_saved_epoch': previous.get('saved_epoch', 0),
              'cold_reload': fresh, 'pages': [helper.read_page(unreal, page) for page in plan],
              'runtime': 'not_run', 'human_review': 'pending'}
    path = helper.EVIDENCE / 'T07.Pages.Readback.json'
    path.write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    return {'report': str(path), 'page_count': len(result['pages']), 'cold_reload': fresh}


if __name__ == '__main__':
    print(json.dumps(validate_pages(), ensure_ascii=False))
