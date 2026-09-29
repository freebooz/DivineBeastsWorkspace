-- 000008：玩家长期背包、容器、快捷栏与OperationId幂等结果。
-- 背包属于PlayerDataService长期数据；不保存货币余额、装备运行状态或实时战斗状态。

CREATE TABLE player_inventories (
    player_id TEXT PRIMARY KEY REFERENCES player_profiles(player_id) ON DELETE CASCADE,
    inventory_revision BIGINT NOT NULL DEFAULT 1 CHECK (inventory_revision >= 1),
    updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE TABLE player_inventory_containers (
    player_id TEXT NOT NULL REFERENCES player_inventories(player_id) ON DELETE CASCADE,
    container_id TEXT NOT NULL CHECK (char_length(container_id) BETWEEN 1 AND 256),
    capacity INTEGER NOT NULL CHECK (capacity BETWEEN 1 AND 10000),
    PRIMARY KEY (player_id, container_id)
);

CREATE TABLE player_inventory_items (
    player_id TEXT NOT NULL REFERENCES player_inventories(player_id) ON DELETE CASCADE,
    item_instance_id TEXT NOT NULL CHECK (char_length(item_instance_id) BETWEEN 1 AND 256),
    item_definition_id TEXT NOT NULL CHECK (char_length(item_definition_id) BETWEEN 1 AND 256),
    quantity INTEGER NOT NULL CHECK (quantity > 0),
    container_id TEXT NOT NULL,
    slot_index INTEGER NOT NULL CHECK (slot_index >= 0),
    revision BIGINT NOT NULL DEFAULT 1 CHECK (revision >= 1),
    instance_state TEXT NOT NULL DEFAULT 'active' CHECK (char_length(instance_state) BETWEEN 1 AND 64),
    max_stack_size INTEGER NOT NULL DEFAULT 1 CHECK (max_stack_size >= 1),
    PRIMARY KEY (player_id, item_instance_id),
    CONSTRAINT fk_inventory_item_container FOREIGN KEY (player_id, container_id)
        REFERENCES player_inventory_containers(player_id, container_id) ON DELETE RESTRICT,
    CONSTRAINT uq_inventory_container_slot UNIQUE (player_id, container_id, slot_index)
        DEFERRABLE INITIALLY DEFERRED
);

CREATE TABLE player_inventory_quickbar (
    player_id TEXT NOT NULL REFERENCES player_inventories(player_id) ON DELETE CASCADE,
    slot_index INTEGER NOT NULL CHECK (slot_index BETWEEN 0 AND 11),
    item_instance_id TEXT NOT NULL,
    revision BIGINT NOT NULL DEFAULT 1 CHECK (revision >= 1),
    PRIMARY KEY (player_id, slot_index),
    CONSTRAINT fk_inventory_quickbar_item FOREIGN KEY (player_id, item_instance_id)
        REFERENCES player_inventory_items(player_id, item_instance_id) ON DELETE CASCADE
);

CREATE TABLE player_inventory_operations (
    player_id TEXT NOT NULL REFERENCES player_inventories(player_id) ON DELETE CASCADE,
    operation_id TEXT NOT NULL CHECK (char_length(operation_id) BETWEEN 1 AND 128),
    operation_type TEXT NOT NULL CHECK (operation_type IN ('Move','Split','Merge','SetQuickbar','ClearQuickbar')),
    canonical_request BYTEA NOT NULL CHECK (octet_length(canonical_request) > 0),
    response_snapshot JSONB NOT NULL CHECK (jsonb_typeof(response_snapshot) = 'object'),
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    PRIMARY KEY (player_id, operation_id)
);

CREATE INDEX idx_inventory_items_player_container
    ON player_inventory_items(player_id, container_id, slot_index);
CREATE INDEX idx_inventory_operations_created
    ON player_inventory_operations(player_id, created_at);

COMMENT ON TABLE player_inventories IS 'PlayerDataService长期背包聚合根；revision用于乐观并发';
COMMENT ON TABLE player_inventory_operations IS '背包写操作持久幂等结果；同OperationId同请求重放，异请求拒绝';
COMMENT ON COLUMN player_inventory_items.max_stack_size IS '首次权威授予时冻结的堆叠上限；客户端Definition不是权威规则';
