#requires -Version 7
<#
.SYNOPSIS
  SSLM Dragon V6 八技能 x 三版高分辨率程序化概念样稿，只用于人工审核。
.DESCRIPTION
  读取V6.1既有技能提案以固定八AbilityId和槽位，生成24张独立1024方形候选
  及64/48尺寸实测样本。程序化概念只能证明构图/可读性，不是AI电影级终稿，
  不能冒充美术优选或真实UE Texture2D。禁止修改五份旧V1及唯一正式UIProfile。
  操作前确认本任务Review目录不存在/新生成文件绝不覆盖他人；有独立人工签批
  才能将8张终稿转入正式资产装配。保留SHA与生成记录。
#>
[CmdletBinding()]
param([switch]$VerifyOnly)
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '../../..')).Path
$hero='Game/Plugins/DivineBeasts/ContentPacks/Heroes/DBAHeroPack_Dragon'
$source=Join-Path $root 'Docs/Design/ZodiacSkillsV6_1_DetailedProposal_20261018.json'
$out=Join-Path $root ($hero+'/SourceArt/UI/SkillIcons/V6/Review')
$index=Join-Path $out 'DragonV6CandidateReviewManifest.json'
$sheet=Join-Path $out 'DragonV6_24CandidateContactSheet.png'
if(!(Test-Path $source)){throw '项目V6.1源合同不见，拒绝生成'}
$v6=Get-Content $source -Raw -Encoding utf8 |ConvertFrom-Json
$entry=@($v6.heroes|Where-Object{$_.heroId -eq 'Hero.Zodiac.Dragon'})
if($v6.V5NamesStillProductionAuthority -ne $true -or $entry.Count -ne 1 -or $entry[0].skills.Count -ne 8){throw 'V6.1源合同仍必须保留现行V5正式授权名字和八项技能'}
$skills=@($entry[0].skills)
$keys=@('dragon_claw','cloud_scale','azure_breath','dragon_ascension','cloud_stride','dragon_scale_guard','thunder_cloud_lock','cloud_blessing')
for($i=0;$i -lt 8;$i++){
  if($skills[$i].abilityId -notmatch ('dragon_eight_'+$keys[$i]+'@1$')){throw ('龙技能稳定身份不符:'+($skills[$i].abilityId))}
}
$old=@('T_DBA_Dragon_Active01_DragonSpiral_V1','T_DBA_Dragon_Active02_CloudBeam_V1','T_DBA_Dragon_BasicAttack_DragonClaw_V1','T_DBA_Dragon_Passive_CloudHeart_V1','T_DBA_Dragon_Ultimate_DragonAscension_V1')
foreach($item in $old){
  if(!(Test-Path (Join-Path $root ($hero+'/SourceArt/UI/SkillIcons/'+$item+'.png')))){throw ('旧V1资产已被其他任务挪动，拒绝继续:'+ $item)}
}
if($VerifyOnly){
  if(!(Test-Path $index)){throw 'Review候选manifest不存在'}
  $data=Get-Content $index -Raw -Encoding UTF8|ConvertFrom-Json
  if($data.candidates.Count -ne 24){throw '候选数量不是24'}
  foreach($c in $data.candidates){
    foreach($f in @($c.master,$c.small64,$c.small48)){
      $full=Join-Path $root $f.path
      if(!(Test-Path $full) -or (Get-FileHash $full -Algorithm SHA256).Hash.ToLowerInvariant() -ne $f.sha256){throw ('候选源Hash改变:'+ $f.path)}
    }
    if($c.humanApproval -ne 'PENDING' -or $c.assetStatus -ne 'PROCEDURAL_CONCEPT_NOT_FINAL_AI_ART'){throw '不能伪造已优选或正式导入'}
  }
  Write-Output 'SSLM_DRAGON_V6_REVIEW_24_SOURCES_48_64_SHA_VERIFIED human_selection_pending=true formal_icons_imported=false'
  return
}
if(Test-Path $out){throw 'Dragon V6 Review目标目录已存在，禁止覆盖或重复生成'}
New-Item -ItemType Directory -Path $out -Force|Out-Null
New-Item -ItemType Directory -Path (Join-Path $out 'Candidates1024')|Out-Null
New-Item -ItemType Directory -Path (Join-Path $out 'Previews48')|Out-Null
New-Item -ItemType Directory -Path (Join-Path $out 'Previews64')|Out-Null
Add-Type -AssemblyName System.Drawing.Common
$types=@'
using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;

public static class SSLMDragonV6ReviewPainter {
  static Color Ink=Color.FromArgb(16,40,45);
  static Color Cyan=Color.FromArgb(104,215,180);
  static Color Blue=Color.FromArgb(64,156,229);
  static Color Silver=Color.FromArgb(181,202,220);
  static Color White=Color.FromArgb(232,240,231);
  static Color Gold=Color.FromArgb(217,183,115);
  static Color With(Color c,int a){return Color.FromArgb(Math.Max(0,Math.Min(255,a)),c);}
  static float Dist(float x,float y){return (float)Math.Sqrt(x*x+y*y);}
  static void Stroke(Graphics g,GraphicsPath path,Color hue,float width){
    float[] scales={7.0f,4f,2.1f,1.18f,.55f};
    int[] alpha={6,12,29,92,230};
    for(int i=0;i<scales.Length;i++){
      using(var pen=new Pen(With(i==4?White:hue,alpha[i]),width*scales[i])){
        pen.LineJoin=LineJoin.Round;pen.StartCap=LineCap.Round;pen.EndCap=LineCap.Round;g.DrawPath(pen,path);
      }
    }
  }
  static void Arc(Graphics g,RectangleF b,float from,float sweep,Color hue,float w){
    using(var path=new GraphicsPath()){path.AddArc(b,from,sweep);Stroke(g,path,hue,w);}
  }
  static void Bezier(Graphics g,PointF a,PointF b,PointF c,PointF d,Color hue,float w){
    using(var path=new GraphicsPath()){path.AddBezier(a,b,c,d);Stroke(g,path,hue,w);}
  }
  static void Poly(Graphics g,PointF[] points,Color hue,float width) {
    using(var path=new GraphicsPath()){path.AddLines(points);Stroke(g,path,hue,width);}
  }
  static void CircleGlow(Graphics g,float x,float y,float r,Color hue,float opacity){
    for(int i=12;i>0;i--){
      float rr=r*(.18f+.90f*i/12);
      int a=(int)(opacity*(1.0f-i/13.0f)*20);
      using(var brush=new SolidBrush(With(hue,a))){g.FillEllipse(brush,x-rr,y-rr,rr*2,rr*2);}
    }
  }
  static void Background(Graphics g,Random r,int k,int v){
    using(var gradient=new LinearGradientBrush(new Rectangle(0,0,1024,1024),
       Color.FromArgb(2,14,29),Color.FromArgb(7,48,67),35f))g.FillRectangle(gradient,0,0,1024,1024);
    // 冷灰云层、轻微明暗纹，严格避免胶片颗粒/沙噪点。
    CircleGlow(g,500,510,480,Blue,1.0f);
    CircleGlow(g,400,390,260,Cyan,.7f);
    int cloudLayers=32;
    for(int i=0;i<cloudLayers;i++){
      float xx=r.Next(-150,1180),yy=r.Next(40,1040);
      float w=r.Next(170,540),h=r.Next(20,90);
      using(var b=new SolidBrush(With((i%3==0?Silver:Blue),r.Next(3,10)))){
        g.FillEllipse(b,xx-w/2,yy-h/2,w,h);
      }
    }
    // 稀疏亮星与施法能量，不使用可读文字或边框。
    for(int i=0;i<25;i++){
      float x=r.Next(50,970),y=r.Next(40,985),rr=r.Next(2,5);
      using(var brush=new SolidBrush(With(i%4==0?Gold:White,r.Next(35,100))))
        g.FillEllipse(brush,x-rr,y-rr,rr*2,rr*2);
    }
  }
  static void Lightning(Graphics g,Random r,PointF start,PointF end,Color hue,float width){
    int count=9;
    var arr=new PointF[count+1];
    float dx=end.X-start.X,dy=end.Y-start.Y;
    float len=Dist(dx,dy);
    float nx=-dy/len,ny=dx/len;
    for(int i=0;i<=count;i++){
      float t=i/(float)count;
      float jitter=(i==0||i==count)?0f:(float)(r.NextDouble()-.5)*len*.18f;
      arr[i]=new PointF(start.X+dx*t+nx*jitter,start.Y+dy*t+ny*jitter);
    }
    Poly(g,arr,hue,width);
    for(int j=0;j<2;j++){
      int a=r.Next(2,7);
      PointF p=arr[a];
      PointF e=new PointF(p.X+(float)(r.NextDouble()-.5)*200,p.Y+r.Next(65,140));
      Poly(g,new PointF[]{p,new PointF((p.X+e.X)/2+15,(p.Y+e.Y)/2),e},hue,width*.30f);
    }
  }
  static void Ring(Graphics g,float x,float y,float rx,float ry,Color hue,float width,float angle){
    GraphicsState s=g.Save();
    g.TranslateTransform(x,y);g.RotateTransform(angle);
    Arc(g,new RectangleF(-rx,-ry,rx*2,ry*2),15f,316f,hue,width);
    g.Restore(s);
  }
  static void DragonHead(Graphics g,float x,float y,float scale,float rotation,Color hue){
    GraphicsState old=g.Save();
    g.TranslateTransform(x,y);g.RotateTransform(rotation);g.ScaleTransform(scale,scale);
    using(var silhouette=new GraphicsPath()){
      silhouette.AddPolygon(new PointF[]{
        new PointF(-170,-45),new PointF(-135,-130),new PointF(-75,-170),
        new PointF(4,-122),new PointF(74,-101),new PointF(160,-43),
        new PointF(225,4),new PointF(152,29),new PointF(123,62),
        new PointF(66,55),new PointF(-5,104),new PointF(-90,70),new PointF(-150,13)
      });
      using(var b=new LinearGradientBrush(new PointF(-160,-170),new PointF(190,110),
                Color.FromArgb(22,105,156),Color.FromArgb(2,25,62)))g.FillPath(b,silhouette);
      Stroke(g,silhouette,hue,5.5f);
    }
    // 额前双犄角与银白鼻梁，龙首辨识度优先。
    Poly(g,new PointF[]{new PointF(-80,-132),new PointF(-150,-260),new PointF(-25,-152)},Gold,6.0f);
    Poly(g,new PointF[]{new PointF(10,-123),new PointF(45,-245),new PointF(73,-100)},Silver,5.2f);
    Bezier(g,new PointF(-112,-57),new PointF(5,-100),new PointF(70,-55),new PointF(198,6),Silver,3.6f);
    Bezier(g,new PointF(110,35),new PointF(195,120),new PointF(110,145),new PointF(23,119),Cyan,2.6f);
    CircleGlow(g,40,-42,31,Gold,4f);
    using(var eye=new SolidBrush(White))g.FillEllipse(eye,23,-52,39,13);
    g.Restore(old);
  }
  static void DragonBody(Graphics g,float x,float y,float size,float rotation,Color hue){
    GraphicsState old=g.Save();
    g.TranslateTransform(x,y);g.RotateTransform(rotation);g.ScaleTransform(size,size);
    Bezier(g,new PointF(-280,195),new PointF(-150,270),new PointF(-150,-170),new PointF(75,-45),hue,10f);
    Bezier(g,new PointF(-270,210),new PointF(-60,335),new PointF(-90,-80),new PointF(75,-55),Silver,4.0f);
    DragonHead(g,130,-70,.55f,5f,hue);
    g.Restore(old);
  }
  static void Shield(Graphics g,int variant){
    GraphicsState old=g.Save();g.TranslateTransform(512,517);g.RotateTransform((variant-1)*7f);
    using(var path=new GraphicsPath()){
      path.AddPolygon(new PointF[]{
        new PointF(0,-345),new PointF(285,-222),new PointF(240,114),
        new PointF(0,348),new PointF(-240,114),new PointF(-285,-222)
      });
      using(var b=new LinearGradientBrush(new Rectangle(-290,-345,580,700),
       Color.FromArgb(80,36,128,184),Color.FromArgb(60,9,42,83),45f))g.FillPath(b,path);
      Stroke(g,path,Silver,7f);
    }
    Arc(g,new RectangleF(-220,-260,440,470),225,288,Cyan,3.0f);
    g.Restore(old);
    DragonHead(g,500,510,.76f,variant%2==0?6:-12,Blue);
  }
  static void Claws(Graphics g,int variant,Random r){
    float angle=variant==1?20f:variant==2?-31f:-13f;
    GraphicsState st=g.Save();g.TranslateTransform(512,535);g.RotateTransform(angle);
    for(int i=0;i<3;i++){
      float off=(i-1)*112;
      Bezier(g,new PointF(-350,-220+off),new PointF(-220,-90+off),
       new PointF(70,75+off),new PointF(320,305+off),variant==1?Silver:Cyan,15-i*2);
      Bezier(g,new PointF(-340,-240+off),new PointF(-150,-120+off),
       new PointF(80,15+off),new PointF(340,240+off),Blue,4.3f);
    }
    g.Restore(st);
    if(variant!=2)DragonHead(g,470,280,.55f,-20f,Silver);
  }
  static void Thunder(Graphics g,int variant,Random r){
    float off=(variant-1)*108;
    CircleGlow(g,500,765,245,Blue,1f);
    Ring(g,520+off*.15f,745,300,76,Silver,7f,0);
    Ring(g,520+off*.15f,745,215,49,Cyan,3f,0);
    Lightning(g,r,new PointF(435+off,70),new PointF(530+off*.15f,742),Blue,variant==1?14:11);
    Lightning(g,r,new PointF(590-off*.5f,95),new PointF(545,740),Silver,4.6f);
    CircleGlow(g,518,733,80,White,2.1f);
  }
  static void Breath(Graphics g,int variant,Random r){
    float angle=variant==0?8f:variant==1?-13f:15f;
    DragonHead(g,300,360,1.0f,angle,Silver);
    for(int i=0;i<7;i++){
      float yy=480+(i-3)*37;
      Bezier(g,new PointF(440,458),new PointF(550,460+(i-3)*6),
       new PointF(670,yy),new PointF(875,600+(i-3)*47),
       i%3==0?White:(i%2==0?Blue:Cyan),6.5f-i*.3f);
    }
    for(int i=0;i<3;i++)Lightning(g,r,new PointF(460,475),new PointF(790+i*20,610+i*55),Blue,2.4f);
  }
  static void Ultimate(Graphics g,int variant,Random r){
    Ring(g,512,590,337,266,Silver,8.5f,variant*10);
    Ring(g,512,590,282,228,Blue,8f,variant*15-20);
    Ring(g,512,590,204,180,Cyan,3.2f,variant*12);
    DragonBody(g,487,495,1.05f,variant==2?42f:variant==1?-15f:12f,Blue);
    Lightning(g,r,new PointF(585,120),new PointF(525,810),variant==2?Gold:Silver,7f);
    CircleGlow(g,510,580,180,Blue,1.0f);
  }
  static void Movement(Graphics g,int variant,Random r){
    float off=variant==0?-20f:variant==1?12f:42f;
    GraphicsState st=g.Save();g.TranslateTransform(512,512);g.RotateTransform(off);
    for(int i=0;i<5;i++){
      float y=(i-2)*80;
      Bezier(g,new PointF(-360,210+y),new PointF(-190,140+y),
             new PointF(-60,-130+y*.4f),new PointF(345,-220+y*.15f),
             i%2==0?Blue:Cyan,7.5f-i*.4f);
    }
    g.Restore(st);
    DragonHead(g,730,345,.70f,-21f,Silver);
    Ring(g,320,690,200,80,Blue,4f,15);
  }
  static void Control(Graphics g,int variant,Random r){
    Ring(g,512,535,360,260,Blue,10f,variant*9);
    Ring(g,512,535,210,165,Silver,10f,-variant*8);
    Ring(g,512,535,115,89,Cyan,6f,variant*15);
    for(int i=0;i<6;i++){
      double rad=(i*60+variant*15)*Math.PI/180.0;
      float x=512+(float)Math.Cos(rad)*305;
      float y=535+(float)Math.Sin(rad)*225;
      float dx=512-x,dy=535-y;
      Bezier(g,new PointF(x,y),new PointF(x+dx*.23f,y+dy*.23f),
       new PointF(x+dx*.51f,y+dy*.51f),new PointF(x+dx*.66f,y+dy*.66f),
       i%2==0?Cyan:Silver,5f);
    }
    CircleGlow(g,510,535,70,Blue,2f);
  }
  static void Domain(Graphics g,int variant,Random r){
    CircleGlow(g,510,490,350,Blue,.6f);
    Ring(g,500,530,350,325,Silver,8f,-17+variant*13);
    Ring(g,500,530,286,273,Cyan,5.5f,17-variant*15);
    Ring(g,500,530,197,190,Blue,5f,variant*9);
    DragonBody(g,510,488,.93f,variant==2?35:variant==1?-14:16,Silver);
    Lightning(g,r,new PointF(710,185),new PointF(625,790),Blue,7f);
    Arc(g,new RectangleF(162,335,680,380),5,145,Gold,2.5f);
  }
  public static void Render(string file,int skill,int variant) {
    using(var bitmap=new Bitmap(1024,1024,PixelFormat.Format32bppArgb))
    using(var g=Graphics.FromImage(bitmap)){
      g.SmoothingMode=SmoothingMode.AntiAlias;
      g.InterpolationMode=InterpolationMode.HighQualityBicubic;
      g.CompositingQuality=CompositingQuality.HighQuality;
      var r=new Random(180901+skill*8177+variant*103);
      Background(g,r,skill,variant);
      switch(skill) {
        case 0:Claws(g,variant,r);break;
        case 1:Thunder(g,variant,r);break;
        case 2:Breath(g,variant,r);break;
        case 3:Ultimate(g,variant,r);break;
        case 4:Movement(g,variant,r);break;
        case 5:Shield(g,variant);break;
        case 6:Control(g,variant,r);break;
        case 7:Domain(g,variant,r);break;
      }
      bitmap.Save(file,ImageFormat.Png);
    }
  }
  public static void Resize(string source,string dest,int size){
    using(var original=Image.FromFile(source))
    using(var bmp=new Bitmap(size,size,PixelFormat.Format32bppArgb))
    using(var g=Graphics.FromImage(bmp)){
      g.Clear(Color.Transparent);
      g.SmoothingMode=SmoothingMode.AntiAlias;
      g.InterpolationMode=InterpolationMode.HighQualityBicubic;
      g.PixelOffsetMode=PixelOffsetMode.HighQuality;
      g.CompositingQuality=CompositingQuality.HighQuality;
      g.DrawImage(original,new Rectangle(0,0,size,size),
        0,0,original.Width,original.Height,GraphicsUnit.Pixel);
      bmp.Save(dest,ImageFormat.Png);
    }
  }
  public static void ContactSheet(string[] pngs,string[] skillNames,string output){
    int width=8*160+64,height=3*199+99;
    using(var bmp=new Bitmap(width,height,PixelFormat.Format32bppArgb))
    using(var g=Graphics.FromImage(bmp)){
      g.Clear(Color.FromArgb(9,22,37));
      g.InterpolationMode=InterpolationMode.HighQualityBicubic;
      using(var titleFont=new Font("Microsoft YaHei",19,FontStyle.Bold))
      using(var subFont=new Font("Microsoft YaHei",13,FontStyle.Regular))
      using(var white=new SolidBrush(Color.FromArgb(230,242,249)))
      using(var gold=new SolidBrush(Color.FromArgb(217,183,115))){
        g.DrawString("SSLM 龙V6 · 24份独立构图审核草案（程序化概念｜待AI美术精修）",
             titleFont,white,new PointF(30,15));
        for(int i=0;i<8;i++)g.DrawString(skillNames[i],subFont,gold,new PointF(40+i*160,58));
        for(int skill=0;skill<8;skill++)for(int variant=0;variant<3;variant++){
          int idx=skill*3+variant;
          using(var src=Image.FromFile(pngs[idx])){
            int xx=36+skill*160,yy=99+variant*199;
            g.DrawImage(src,new Rectangle(xx,yy,145,145));
            g.DrawString((char)('A'+variant)+" | "+(skill+1),
                subFont,white,new PointF(xx+8,yy+150));
          }
        }
      }
      bmp.Save(output,ImageFormat.Png);
    }
  }
}
'@
Add-Type -TypeDefinition $types -ReferencedAssemblies @('System.Drawing.Common','System.Drawing.Primitives','System.Runtime','System.Private.Windows.GdiPlus','System.Private.Windows.Core') -ErrorAction Stop
$names=@('CangLongZhua','LeiCai','LeiXi','CangLongDingTian','YuFeng','YunHu','LongYa','LongYu')
$candidates=@()
$files=New-Object 'System.Collections.Generic.List[string]'
for($i=0;$i -lt 8;$i++){
  for($v=0;$v -lt 3;$v++){
    $variant=@('A','B','C')[$v]
    $stem=('T_DBA_Dragon_S{0:00}_{1}_V6_{2}_Review' -f ($i+1),$names[$i],$variant)
    $a=Join-Path $out ('Candidates1024/'+$stem+'.png')
    $b=Join-Path $out ('Previews64/'+$stem+'_64.png')
    $c=Join-Path $out ('Previews48/'+$stem+'_48.png')
    [SSLMDragonV6ReviewPainter]::Render($a,$i,$v)
    [SSLMDragonV6ReviewPainter]::Resize($a,$b,64)
    [SSLMDragonV6ReviewPainter]::Resize($a,$c,48)
    $files.Add($a)
    $shaObj={
      param($full)
      return [ordered]@{
        path=([System.IO.Path]::GetRelativePath($root,$full)).Replace('\','/')
        sha256=(Get-FileHash $full -Algorithm SHA256).Hash.ToLowerInvariant()
        bytes=(Get-Item $full).Length
      }
    }
    $candidates+=[ordered]@{
      skillOrder=$i+1
      nameV6=$skills[$i].proposedNameV61
      nameV5=$skills[$i].approvedDisplayNameV5
      abilityId=$skills[$i].abilityId
      slotId=$skills[$i].slotId
      variant=$variant
      master=(& $shaObj $a)
      small64=(& $shaObj $b)
      small48=(& $shaObj $c)
      humanApproval='PENDING'
      assetStatus='PROCEDURAL_CONCEPT_NOT_FINAL_AI_ART'
    }
  }
}
[SSLMDragonV6ReviewPainter]::ContactSheet($files.ToArray(),@($skills|ForEach-Object{$_.proposedNameV61}),$sheet)
[ordered]@{
  schemaVersion=1
  date='2026-10-18'
  titleZh='辰龙V6八技能每技能三版概念草案'
  currentProductionDisplayNameSource='V5'
  candidateCount=24
  sourceQualityStatusZh='程序化构图审核图，非人工精修3A终版或AI图像母版'
  approvedCount=0
  productionAbilitySetChanged=$false
  productionUIProfileChanged=$false
  nativeUETexture2DImportedCount=0
  contactSheetPath=([System.IO.Path]::GetRelativePath($root,$sheet)).Replace('\','/')
  candidates=$candidates
}|ConvertTo-Json -Depth 9|Set-Content -LiteralPath $index -Encoding UTF8
Write-Output ('SSLM_DRAGON_V6_24_PROCEDURAL_ICON_REVIEW_RENDERED count='+$candidates.Count+' sheet='+$sheet+' human_pending=true UE_native=false')
