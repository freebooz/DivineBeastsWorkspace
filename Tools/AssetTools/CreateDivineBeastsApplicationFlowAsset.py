#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
创建或验证《神兽联盟》正式 Application Flow（应用流程）DataAsset（数据资产）。

约束：
- 仅操作 /DBAClient/Definitions/DA_DivineBeastsApplicationFlow。
- 已存在资产默认只校验；经明确迁移调用只增加竞技入场恢复边。
- 必须使用真实 UGamePlatformFlowDefinition 反射类，不生成伪 .uasset。
- 资产不保存账号凭据、Endpoint、TransferTicket 等敏感数据。
"""

import json

ASSET_PACKAGE = "/DBAClient/Definitions/DA_DivineBeastsApplicationFlow"
CLASS_PATH = "/Script/GamePlatformApplicationFlow.GamePlatformFlowDefinition"
LOGICAL_NAMESPACE = "divinebeasts.application"
LOGICAL_NAME = "main"
LOGICAL_VERSION = 1
ARENA_ENTRY_RECOVERY_NODES = {
    "DBA.Flow.TransferWorld": ("DBA.Flow.Executor.TransferWorld", "DBA.Flow.WorldReady"),
    "DBA.Flow.WorldReady": ("DBA.Flow.Executor.WorldReady", "DBA.Flow.InWorld"),
}
RECOVERY_NODE = "DBA.Flow.Recovering"


def require(value, message):
    """失败即终止，禁止带着半成品继续保存。"""
    if not value:
        raise RuntimeError(message)


def node(node_id, executor_id, timeout, next_node="None", routes=None):
    """生成与FGamePlatformFlowNodeDefinition字段一致的值描述。"""
    return {
        "node_id": node_id,
        "executor_id": executor_id,
        "input_definition_id": "",
        "timeout_seconds": float(timeout),
        "next_node_id": next_node,
        "routes": {} if routes is None else dict(routes),
    }


def expected_values():
    """项目主应用流程。Party/Matchmaking由Arena领域负责，不进入本图。"""
    nodes = [
        node("DBA.Flow.Boot", "DBA.Flow.Executor.Boot", 5, "DBA.Flow.Initialize"),
        node("DBA.Flow.Initialize", "DBA.Flow.Executor.Initialize", 5, "DBA.Flow.Authentication"),
        node("DBA.Flow.Authentication", "DBA.Flow.Executor.Authentication", 1800, "DBA.Flow.LoadProfile"),
        node("DBA.Flow.LoadProfile", "DBA.Flow.Executor.LoadProfile", 30, "DBA.Flow.LoadRoster"),
        node("DBA.Flow.LoadRoster", "DBA.Flow.Executor.LoadRoster", 30, "DBA.Flow.CharacterEntry"),
        node(
            "DBA.Flow.CharacterEntry",
            "DBA.Flow.Executor.CharacterEntry",
            3600,
            "None",
            {
                "CreateCharacter": "DBA.Flow.CreateCharacter",
                "SelectCharacter": "DBA.Flow.ValidateSelection",
            },
        ),
        # 创建成功先重新读取后端档案并回到角色入口等待人工选择；禁止由创建动作直接进入世界。
        node("DBA.Flow.CreateCharacter", "DBA.Flow.Executor.CreateCharacter", 30, "DBA.Flow.LoadRoster"),
        node("DBA.Flow.ValidateSelection", "DBA.Flow.Executor.ValidateSelection", 30, "DBA.Flow.ResolveExperience"),
        node("DBA.Flow.ResolveExperience", "DBA.Flow.Executor.ResolveExperience", 5, "DBA.Flow.RequestWorld"),
        node("DBA.Flow.RequestWorld", "DBA.Flow.Executor.RequestWorld", 45, "DBA.Flow.TransferWorld"),
        node("DBA.Flow.TransferWorld", "DBA.Flow.Executor.TransferWorld", 60, "DBA.Flow.WorldReady", {"Recover": RECOVERY_NODE}),
        node("DBA.Flow.WorldReady", "DBA.Flow.Executor.WorldReady", 180, "DBA.Flow.InWorld", {"Recover": RECOVERY_NODE}),
        # InWorld完全事件驱动；24小时仅作为单次客户端会话失控保护，不用于业务轮询。
        node(
            "DBA.Flow.InWorld",
            "DBA.Flow.Executor.InWorld",
            86400,
            "None",
            {
                "RequestWorld": "DBA.Flow.RequestWorld",
                "Recover": "DBA.Flow.Recovering",
            },
        ),
        node("DBA.Flow.Recovering", "DBA.Flow.Executor.Recovering", 30, "DBA.Flow.RequestWorld"),
    ]
    return {
        "entry_node_id": "DBA.Flow.Boot",
        "allow_cycles": True,
        "max_immediate_cycle_transitions": 16,
        "nodes": nodes,
    }


def read_asset_values(asset, include_all_node_fields=False):
    """读取磁盘资产当前值，只用于校验，不进行隐式规范化。"""
    result = {
        "entry_node_id": str(asset.get_editor_property("entry_node_id")),
        "allow_cycles": bool(asset.get_editor_property("allow_cycles")),
        "max_immediate_cycle_transitions": int(
            asset.get_editor_property("max_immediate_cycle_transitions")
        ),
        "nodes": [],
    }
    for item in asset.get_editor_property("nodes"):
        node_values = {
            "node_id": str(item.get_editor_property("node_id")),
            "executor_id": str(item.get_editor_property("executor_id")),
            "input_definition_id": "",
            "timeout_seconds": float(item.get_editor_property("timeout_seconds")),
            "next_node_id": str(item.get_editor_property("next_node_id")),
            "routes": {
                str(key): str(value)
                for key, value in item.get_editor_property("routes").items()
            },
        }
        if include_all_node_fields:
            identity = item.get_editor_property("input_definition_id")
            asset_type = identity.get_editor_property("primary_asset_type")
            node_values["input_definition_id"] = (
                str(asset_type.get_editor_property("name")), str(identity.get_editor_property("primary_asset_name")))
            node_values["wait_for_user_input"] = bool(item.get_editor_property("wait_for_user_input"))
        result["nodes"].append(node_values)
    return result


def validate_identity(asset):
    """验证逻辑身份和结构版本，确保Data服务可以按正式PrimaryAssetId发现资产。"""
    logical_id = asset.get_editor_property("logical_id")
    require(logical_id.get_editor_property("namespace") == LOGICAL_NAMESPACE,
            "Flow LogicalId.Namespace不匹配")
    require(logical_id.get_editor_property("name") == LOGICAL_NAME,
            "Flow LogicalId.Name不匹配")
    require(logical_id.get_editor_property("logical_version") == LOGICAL_VERSION,
            "Flow LogicalId.LogicalVersion不匹配")

    version = asset.get_editor_property("data_version")
    require(int(version.get_editor_property("schema_version")) == 1,
            "Flow schema_version必须为1")
    require(int(version.get_editor_property("content_revision")) >= 1,
            "Flow content_revision必须为正整数")


def write_new_asset(unreal, asset):
    """只对新建资产写值；已有资产绝不自动覆盖。"""
    logical_id = asset.get_editor_property("logical_id")
    logical_id.set_editor_property("namespace", LOGICAL_NAMESPACE)
    logical_id.set_editor_property("name", LOGICAL_NAME)
    logical_id.set_editor_property("logical_version", LOGICAL_VERSION)
    asset.set_editor_property("logical_id", logical_id)

    version = asset.get_editor_property("data_version")
    version.set_editor_property("schema_version", 1)
    version.set_editor_property("content_revision", 1)
    asset.set_editor_property("data_version", version)
    asset.set_editor_property("required_definitions", [])

    values = expected_values()
    node_type = getattr(unreal, "GamePlatformFlowNodeDefinition", None)
    require(node_type is not None, "GamePlatformFlowNodeDefinition反射类型未加载")

    reflected_nodes = []
    for item in values["nodes"]:
        # 节点字段为EditDefaultsOnly；独立Python结构体实例不能通过编辑接口修改。
        # UE结构体构造参数直接初始化值，再整体写入DataAsset的默认Nodes数组，
        # 保留引擎类型转换与资产保存校验，不放宽运行期字段的编辑权限。
        reflected = node_type(
            node_id=item["node_id"],
            executor_id=item["executor_id"],
            timeout_seconds=item["timeout_seconds"],
            next_node_id=item["next_node_id"],
            routes=item["routes"],
        )
        reflected_nodes.append(reflected)

    asset.set_editor_property("entry_node_id", values["entry_node_id"])
    asset.set_editor_property("allow_cycles", values["allow_cycles"])
    asset.set_editor_property("max_immediate_cycle_transitions",
                              values["max_immediate_cycle_transitions"])
    asset.set_editor_property("nodes", reflected_nodes)


def plan_arena_entry_recovery(values):
    """核对既有权威图，只计算须补的两条边。"""
    nodes = values["nodes"]
    by_name = {item["node_id"]: item for item in nodes}
    require(len(by_name) == len(nodes), "流程存在重复节点")
    recovery = by_name.get(RECOVERY_NODE)
    require(recovery and recovery["executor_id"] == "DBA.Flow.Executor.Recovering"
            and recovery["next_node_id"] == "DBA.Flow.RequestWorld", "现有Recovering合同已变化")
    missing = []
    for name, (executor_id, next_node_id) in ARENA_ENTRY_RECOVERY_NODES.items():
        source = by_name.get(name)
        require(source and source["executor_id"] == executor_id and source["next_node_id"] == next_node_id,
                f"{name} 的现有执行器或默认分支已变化")
        current = source["routes"].get("Recover")
        require(current in (None, RECOVERY_NODE), f"{name} 已有不同的 Recover 分支，保留现有值")
        if current is None:
            missing.append(name)
    return missing


def _flow_asset(unreal):
    flow_class = unreal.load_class(None, CLASS_PATH)
    require(flow_class is not None, "UGamePlatformFlowDefinition反射类未加载")
    asset = unreal.EditorAssetLibrary.load_asset(ASSET_PACKAGE)
    require(asset is not None and asset.get_class() == flow_class, "现有正式流程资产缺失或类型不符")
    validate_identity(asset)
    return asset


def verify_arena_entry_recovery():
    """可从新编辑器或冷重载后调用，确认真实资产中的恢复边。"""
    import unreal

    values = read_asset_values(_flow_asset(unreal))
    require(not plan_arena_entry_recovery(values), "正式流程资产缺少竞技入场恢复边")
    return {name: RECOVERY_NODE for name in ARENA_ENTRY_RECOVERY_NODES}


def migrate_arena_entry_recovery():
    """仅更新已受审查的缺失分支，保留既有字段和所有其他节点。"""
    import unreal

    asset = _flow_asset(unreal)
    expected = read_asset_values(asset, include_all_node_fields=True)
    missing = set(plan_arena_entry_recovery(expected))
    if not missing:
        return {"asset": ASSET_PACKAGE, "changed": [], "routes": verify_arena_entry_recovery()}

    nodes = list(asset.get_editor_property("nodes"))
    node_type = unreal.GamePlatformFlowNodeDefinition
    for index, previous in enumerate(nodes):
        name = str(previous.get_editor_property("node_id"))
        if name not in missing:
            continue
        routes = dict(previous.get_editor_property("routes").items())
        routes[unreal.Name("Recover")] = unreal.Name(RECOVERY_NODE)
        nodes[index] = node_type(
            node_id=previous.get_editor_property("node_id"),
            executor_id=previous.get_editor_property("executor_id"),
            input_definition_id=previous.get_editor_property("input_definition_id"),
            timeout_seconds=previous.get_editor_property("timeout_seconds"),
            next_node_id=previous.get_editor_property("next_node_id"),
            routes=routes,
            wait_for_user_input=previous.get_editor_property("wait_for_user_input"),
        )
        expected["nodes"][index]["routes"]["Recover"] = RECOVERY_NODE
    asset.set_editor_property("nodes", nodes)
    require(read_asset_values(asset, include_all_node_fields=True) == expected,
            "迁移改变了恢复边之外的流程设置，禁止保存")
    require(unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False), "保存竞技入场恢复边失败")
    return {"asset": ASSET_PACKAGE, "changed": sorted(missing), "routes": verify_arena_entry_recovery()}


def main():
    import unreal

    assets = unreal.EditorAssetLibrary
    flow_class = unreal.load_class(None, CLASS_PATH)
    require(flow_class is not None,
            "UGamePlatformFlowDefinition反射类未加载，请先完成UE模块编译")

    # 首次生成先查询存在性，避免以预期的LoadAsset失败污染Commandlet退出码。
    # 已登记但无法加载的资产必须报错，不能把损坏资产误判为首次创建后覆盖。
    created = not assets.does_asset_exist(ASSET_PACKAGE)
    asset = None if created else assets.load_asset(ASSET_PACKAGE)
    if not created:
        require(asset is not None, "已有流程资产无法加载；禁止自动覆盖")

    if created:
        tools = unreal.AssetToolsHelpers.get_asset_tools()
        require(tools is not None, "AssetTools不可用")
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", flow_class)
        folder, name = ASSET_PACKAGE.rsplit("/", 1)
        asset = tools.create_asset(name, folder, flow_class, factory,
                                   overwrite_existing=False)
        require(asset is not None, "创建神兽联盟Application Flow资产失败")
        write_new_asset(unreal, asset)
        require(assets.save_loaded_asset(asset, only_if_is_dirty=False),
                "保存神兽联盟Application Flow资产失败")
    else:
        require(asset.get_class() == flow_class,
                "已有同路径资产类型不是UGamePlatformFlowDefinition")

    validate_identity(asset)
    require(read_asset_values(asset) == expected_values(),
            "已有Flow资产与正式流程基线不一致；禁止脚本自动覆盖，请人工审查后升级版本")

    print(
        "DIVINE_BEASTS_APPLICATION_FLOW_REPORT "
        + json.dumps(
            {
                "asset": ASSET_PACKAGE,
                "logical_id": "divinebeasts.application.main@1",
                "created": created,
                "node_count": len(expected_values()["nodes"]),
                "allow_cycles": True,
                "status": "ok",
            },
            ensure_ascii=False,
            sort_keys=True,
        ),
        flush=True,
    )


if __name__ == "__main__":
    main()
