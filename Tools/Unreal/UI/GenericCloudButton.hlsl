// 只处理表现颜色：两次采样，零时间/Tick依赖。ColorRGB已由sRGB采样转换至线性。
// 绿/蓝内芯参与换色；暖金边R最大，不受FillColor影响。RecolorAmount=0时RGB直通。
float3 rgb = max(ColorRGB, 0.0);
float coreMask = smoothstep(0.006, 0.04, max(rgb.g, rgb.b) - rgb.r);
float value = max(rgb.g, rgb.b);
float3 tinted = value * max(FillColor, 0.0);
rgb = lerp(rgb, tinted, saturate(RecolorAmount) * coreMask);
float gray = dot(rgb, float3(0.2126, 0.7152, 0.0722));
rgb = lerp(gray.xxx, rgb, saturate(Saturation));
return float4(rgb * max(Brightness, 0.0), saturate(Alpha * Opacity));

