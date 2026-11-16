// 项目UI窄金属圈。Diameter、FrameWidth为逻辑单位；UV为控件归一化坐标。
// FillCenter=0保留透明圆心，FillCenter=1绘制按钮底；Body/Trim为线性RGB。
float2 p = (UV - 0.5) * max(Diameter, 2.0);
float radius = length(p);
float outer = max(Diameter, 2.0) * 0.5 - 0.65;
float width = clamp(FrameWidth, 1.0, outer);
float inner = outer - width;
float aa = max(fwidth(radius), 0.65);
float outsideMask = 1.0 - smoothstep(outer-aa, outer+aa, radius);
float hole = 1.0 - smoothstep(inner-aa, inner+aa, radius);
float t = saturate((radius-inner) / width);
float2 normal = p / max(radius, 0.001);
float key = saturate(dot(normal, normalize(float2(-0.6,-0.8))) * 0.5 + 0.5);
float lips = saturate((1.0-smoothstep(0.12,0.36,t)) + smoothstep(0.67,0.94,t));
float3 metal = lerp(Body, Trim, lips * 0.72) * (0.58 + key * 0.72);
metal += Trim * pow(key, 7.0) * 0.13;
metal *= 0.76 + 0.24 * smoothstep(0.0, 0.22, t);
float3 center = Body * (0.32 + 0.16*key);
float fill = saturate(FillCenter);
float alpha = outsideMask * (1.0-hole + hole*fill);
float3 color = lerp(metal, center, hole*fill) * max(Brightness, 0.0);
return float4(color, alpha);
