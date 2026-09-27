#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
创建或验证《神兽联盟》正式 Application Flow（应用流程）DataAsset（数据资产）。

约束：
- 仅操作 /DBAClient/Definitions/DA_DivineBeastsApplicationFlow。
- 已存在资产只校验，不自动覆盖人工修改。
- 必须使用真实 UGamePlatformFlowDefinition 反射类，不生成伪 .uasset。
- 资产不保存账号凭据、Endpoint、TransferTicket 等敏感数据。
"""

import json

ASSET_PACKAGE = "/DBAClient/Definitions/DA_DivineBeastsApplicationFlow"
CLASS_PATH = "/Script/GamePlatformApplicationFlow.GamePlatformFlowDefinition"
LOGICAL_NAMESPACE = "divinebeasts.application"
LOGICAL_NAME = "main"
LOGICAL_VERSION = 1


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
        node("DBA.Flow.CreateCharacter", "DBA.Flow.Executor.CreateCharacter", 30, "DBA.Flow.ValidateSelection"),
        node("DBA.Flow.ValidateSelection", "DBA.Flow.Executor.ValidateSelection", 30, "DBA.Flow.ResolveExperience"),
        node("DBA.Flow.ResolveExperience", "DBA.Flow.Executor.ResolveExperience", 5, "DBA.Flow.RequestWorld"),
        node("DBA.Flow.RequestWorld", "DBA.Flow.Executor.RequestWorld", 45, "DBA.Flow.TransferWorld"),
        node("DBA.Flow.TransferWorld", "DBA.Flow.Executor.TransferWorld", 60, "DBA.Flow.WorldReady"),
        node("DBA.Flow.WorldReady", "DBA.Flow.Executor.WorldReady", 180, "DBA.Flow.InWorld"),
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


def read_asset_values(asset):
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
        result["nodes"].append(
            {
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
        )
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
        reflected = node_type()
        reflected.set_editor_property("node_id", item["node_id"])
        reflected.set_editor_property("executor_id", item["executor_id"])
        reflected.set_editor_property("timeout_seconds", item["timeout_seconds"])
        reflected.set_editor_property("next_node_id", item["next_node_id"])
        reflected.set_editor_property("routes", item["routes"])
        reflected_nodes.append(reflected)

    asset.set_editor_property("entry_node_id", values["entry_node_id"])
    asset.set_editor_property("allow_cycles", values["allow_cycles"])
    asset.set_editor_property("max_immediate_cycle_transitions",
                              values["max_immediate_cycle_transitions"])
    asset.set_editor_property("nodes", reflected_nodes)


def main():
    import unreal

    assets = unreal.EditorAssetLibrary
    flow_class = unreal.load_class(None, CLASS_PATH)
    require(flow_class is not None,
            "UGamePlatformFlowDefinition反射类未加载，请先完成UE模块编译")

    asset = assets.load_asset(ASSET_PACKAGE)
    created = asset is None

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
