# -*- coding: utf-8 -*-
"""SSLM三资源HUD源码与UE原生资产存在性静态门禁；不等于真实编辑器编译或联机。"""
import json
import sys
from pathlib import Path
root = Path(__file__).resolve().parents[2]
plugins = root / "Game/Plugins"
combat = plugins / "GamePlatform/Gameplay/GamePlatformCombat/Source/GamePlatformCombat"
ui = plugins / "DivineBeasts/DBAClient/Source/DivineBeastsUIClient"
ui_pack = plugins / "DivineBeasts/ContentPacks/Presentation/DBAUIPack_Core/Content/UI"
base_ui = plugins / "GamePlatform/Presentation/GamePlatformUI/Source/GamePlatformUIClient"
issues = []
def read(p):
    if not p.is_file():
        issues.append("缺少文件：" + str(p))
        return ""
    return p.read_text(encoding="utf-8-sig")
def check(ok, msg):
    if not ok:
        issues.append(msg)
h = read(combat / "Public/Attributes/GamePlatformCombatAttributeSet.h")
c = read(combat / "Private/Attributes/GamePlatformCombatAttributeSet.cpp")
reset = read(combat / "Private/Components/GamePlatformCombatComponent.cpp")
view = read(ui / "Private/ViewModels/Combat/DivineBeastsPlayerStatusViewModel.cpp")
panel = read(ui / "Private/Panels/Combat/DivineBeastsPlayerStatusPanel.cpp")
panel_h = read(ui / "Public/Panels/Combat/DivineBeastsPlayerStatusPanel.h")
contract = read(ui / "Public/Contracts/DivineBeastsPlayerStatusUIContracts.h")
base_widget = read(base_ui / "Private/Components/GamePlatformResourceBarWidget.cpp")
native_test = read(ui / "Private/Tests/DivineBeastsPlayerStatusViewModelTests.cpp")
check("FGameplayAttributeData Energy;" in h and "FGameplayAttributeData MaxEnergy;" in h,
      "Energy/MaxEnergy未作为独立GAS数值声明")
check("InitMaxEnergy(100.0f)" in c and "InitEnergy(100.0f)" in c,
      "能量默认100/100缺失")
for f in ("Energy", "MaxEnergy"):
    check(f"UGamePlatformCombatAttributeSet, {f}, COND_OwnerOnly, REPNOTIFY_Always" in c,
          f"{f}未按拥有者复制")
    check(f"NewStatus.{f} = Combat->Get{f}();" in view,
          f"UI必须从真实Combat读取{f}")
    check(f"Get{f}Attribute()" in view,
          f"没有{f}独立变化委托")
check("GetEnergyAttribute()" in reset and "GetMaxEnergy()" in reset,
      "新化身重置没有恢复独立能量")
for f in ("Health", "MaxHealth", "Energy", "MaxEnergy", "Momentum", "MaxMomentum"):
    check(f"double {f} =" in contract, "只读状态契约缺少" + f)
check("NewStatus.Momentum = Momentum->GetMomentum();" in view,
      "项目气势必须由自身属性集读取")
for name in ("HealthBar", "EnergyBar", "MomentumBar"):
    check(f"{name} = nullptr;" in panel_h, "面板缺少" + name)
check('TEXT("Energy"), Status.Energy, Status.MaxEnergy' in panel and
      'TEXT("Momentum"),' in panel,
      "UI未将能量与气势分为两条独立ResourceId")
check("SetResourceFillColor(FLinearColor(0.14f, 0.43f, 0.96f" in panel and
      "SetResourceFillColor(FLinearColor(0.97f, 0.69f, 0.22f" in panel,
      "没有明确蓝色能量与金色气势的不同填充色")
check("SetFillColorAndOpacity(InColor.GetClamped())" in base_widget,
      "通用资源条颜色设置未接线")
check('Status.Energy, 65.0' in native_test and 'Status.Momentum, 40.0' in native_test,
      "原生视图测试没有校验65能量与40气势独立值")
for name in ("WBP_DBA_UI_PlayerStatus", "WBP_DBA_UI_PlayerFrameStatus",
             "WBP_DBA_UI_PlayerFrameEnergyBar", "WBP_DBA_UI_EnergyBar"):
    check((ui_pack / "Components" / (name + ".uasset")).is_file(),
          "缺少原生UI资产 " + name)
check((ui_pack / "Combat/WBP_DBA_UI_CombatHUD.uasset").is_file(),
      "缺少真实战斗HUD蓝图")
result = {"status": "PASS" if not issues else "FAIL",
          "attribute_sets": 2, "attribute_fields": 10,
          "scope": "静态源码及资产路径检查，不含蓝图结构、C++编译、双端复制或视觉验收",
          "issues": issues}
print(json.dumps(result, ensure_ascii=False, indent=2))
sys.exit(0 if not issues else 1)
