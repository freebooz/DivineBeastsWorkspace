#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""只读主题接入静态门禁；失败指出缺失层，不代替真实UE编译、自动化与资产验收。"""
from pathlib import Path
import sys
ROOT = Path(__file__).resolve().parents[2]
BASE = ROOT / 'Game/Plugins/GamePlatform/Presentation/GamePlatformUI/Source/GamePlatformUIClient'

def read(path: Path) -> str:
    return path.read_text(encoding='utf-8-sig') if path.is_file() else ''

def main() -> int:
    failures = 0
    def check(name: str, ok: bool) -> None:
        nonlocal failures
        print(('PASS' if ok else 'FAIL') + ' | ' + name)
        failures += not ok
    definition = read(BASE / 'Public/Definitions/GamePlatformUIThemeDefinition.h')
    service = read(BASE / 'Private/Styling/GamePlatformUIThemeService.cpp')
    bindings = read(BASE / 'Private/Styling/GamePlatformUIThemeBinding.cpp')
    manager = read(BASE / 'Private/Manager/GamePlatformUIManagerSubsystem.cpp')
    check('已有平台定义基类承载主题身份', 'UGamePlatformDefinitionBase' in definition)
    check('按钮、文字、边框使用类型明确的原生样式', all(x in definition for x in ['Buttons', 'Texts', 'Borders']))
    check('主题只经中央数据租约加载释放', 'AcquireDefinition(' in service and 'ReleaseDefinition(' in service)
    check('主题服务无独立加载器与同步加载', bool(service) and all(x not in service for x in ['LoadSynchronous(', 'FStreamableManager', 'UnloadPrimaryAsset(']))
    check('主题属于原有本地玩家UI管理器', 'ThemeService = NewObject<UGamePlatformUIThemeService>(this)' in manager and 'ThemeService->Shutdown()' in manager)
    check('兼容原生控件而不重建树', all(x in bindings for x in ['UButton', 'UTextBlock', 'UBorder']) and 'WidgetTree->FindWidget' in bindings)
    check('不把神兽联盟资源写入平台主题实现', all(x not in service + bindings + definition for x in ['/DBAUIPack_Core/', 'Hero.Zodiac.', 'DivineBeasts']))
    check('变更通知代替逐帧主题轮询', bool(service) and 'OnThemeChanged' in bindings and 'Tick(' not in service + bindings)
    check('CommonText显式脱离旧样式以保持当前字号快照', 'CommonText->SetStyle(nullptr)' in bindings and 'if (Item.Binding.bPreserveFontSize)' in bindings)
    check('主题文字同步阴影及删除线，不残留上个主题视觉值',
          'Style->bUsesDropShadow' in bindings and
          'Text->SetShadowOffset(ShadowOffset)' in bindings and
          'Text->SetShadowColorAndOpacity(ShadowColor)' in bindings and
          'Text->SetStrikeBrush(StrikeBrush)' in bindings)
    check('CommonBorder记录新样式类避免同步时恢复旧背景', 'CommonBorder->SetStyle(TSubclassOf<UCommonBorderStyle>(Item.StyleClass))' in bindings)
    check('显式主题按钮清除历史背景乘色且不修改业务启用状态',
          'Button->SetBackgroundColor(FLinearColor::White)' in bindings and
          'Button->SetIsEnabled(' not in bindings)
    check('主题资产明确排除专用服务器加载', 'NeedsLoadForServer() const override { return false; }' in definition)
    project = read(ROOT / 'Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsUIClient/Private/DivineBeastsUIClientSubsystem.cpp')
    check('项目主题选择来自配置而非平台硬编码', 'DefaultThemeDefinitionId' in project and 'PlatformUI->RequestThemeAsync(ThemeId, Context)' in project)
    ordinary = read(BASE / 'Private/Core/GamePlatformWidgetBase.cpp')
    active = read(BASE / 'Private/Core/GamePlatformActivatableWidgetBase.cpp')
    check('两类平台控件均成对绑定和注销主题', all('ThemeBinding->Initialize(this, ThemeBindings)' in text and 'ThemeBinding->Unbind()' in text for text in (ordinary, active)))
    check('主题终态不在请求句柄返回前同步广播失败', 'FailPending(RequestId' not in service.split('FGuid UGamePlatformUIThemeService::RequestThemeAsync',1)[-1].split('void UGamePlatformUIThemeService::FailPending',1)[0])
    check('主题回调中移除控件仍有强引用和树身份复核', 'TArray<TStrongObjectPtr<UWidget>> PinnedWidgets' in bindings and 'FindWidget(Item.Binding.WidgetName) != Item.Widget' in bindings)
    check('主题批量应用完整结束后才释放旧样式引用',
          bindings.find('AppliedStyleClasses.AddUnique(IncomingClass)') < bindings.find('ApplyStyle(Item);') <
          bindings.find('AppliedStyleClasses = MoveTemp(IncomingClasses);'))
    print('RESULT | FAILED=' + str(failures))
    print('LIMITS | 仅源码合同检查，不证明引擎资源或完整程序已可用。')
    return int(failures != 0)

if __name__ == '__main__':
    sys.exit(main())
