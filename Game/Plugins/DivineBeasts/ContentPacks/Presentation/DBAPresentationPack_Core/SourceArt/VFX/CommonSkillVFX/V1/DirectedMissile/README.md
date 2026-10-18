# 定向飞弹源与审核合同

`Reference_DirectedMissile.png`为用户参考原图。`DirectedMissileV1.blend`含三条可编辑Bezier与有体积的Core/Petals/AirWave三槽网格；FBX由该源导出，三槽面朝外，尖端保留聚能小核。参考相机、起止点和单位厘米见`ReferenceGuide.json`。

两份`*TexturePrompt.txt`为内置image_gen的完整纹理提示词；Head与Trail RGBA源像素未经后期修改，`TextureManifest.json`记录SHA和UE导入合同。图像只供真实材质采样，不能作为原生VFX画面。

最新要求：修订translucent_energy_layers_03保持弹头最大截面宽约0.19倍轴长；内核半径0.056L、瓣厚半轴0.019L，填实内部并保留曲面暗部。尖端辉光/细芒共用Core槽位，VertexColor.R携带分层遮罩，导入必须Replace顶点色。三条尾迹宽26厘米，相位0°/60°/120°，按真实历史位置消退，出生渐隐并在飞行末段收束。原始纹理Alpha不用于破碎轮廓，能量由解析低频流动与弯曲叠层金边构成。有界空气材质默认强度0.35。Native、源验证、冷加载与原生媒体归工作空间`Saved/Validation/CommonSkillVFX/DirectedMissileV1`；完整执行说明归`Docs/Implementation/VFX/SSLM定向飞弹V1拆解与完整Niagara制作提示词_20261018.md`。

当前是人工待审核候选；没有修改正式生肖映射，也没有把此授权当作凝锋已通过人工审核。四档、GPU和Cook仍未完成。`Revisions`保留本项已替换源版本，不作最终素材使用。
