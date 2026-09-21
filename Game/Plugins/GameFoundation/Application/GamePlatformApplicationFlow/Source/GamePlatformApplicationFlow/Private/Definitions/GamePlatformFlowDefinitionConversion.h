#pragma once

#include "Definitions/GamePlatformFlowDefinition.h"
#include "Execution/ApplicationFlowExecutor.h"

namespace GamePlatform::ApplicationFlow
{
/** 只转换图值，不制造节点；同一图供资产预检及真实工厂适配消费。 */
FDefinition ConvertAssetGraph(const UGamePlatformFlowDefinition& Definition);
}
