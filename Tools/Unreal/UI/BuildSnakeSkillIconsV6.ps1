# SSLM 巳蛇技能 V6 的独立技能图标源稿生成器。
# 职责：离线创建 8 张 1024×1024 美术审查源稿与 8 张 48×48 PNG 纹理。
# 归属：第三层巳蛇英雄内容包的源美术；不得在服务器、GAS逻辑或运行时调用。
# 安全边界：只写V6独立命名的PNG文件；不会生成伪造uasset，不修改UI Profile或已批准名称。
# 质量状态：采用矢量渐变构图制作可审核初稿，不能代替最终3A手绘美术验收。
# 输入：输出目录。失败：抛出错误并保留可追踪的已产生文件；恢复时可重新运行相同输出路径。
param([string]$OutputRoot='Game/Plugins/DivineBeasts/ContentPacks/Heroes/DBAHeroPack_Snake/SourceArt/UI/SkillIcons/V6')
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Drawing -ErrorAction Stop
$cs=@'
using System;
using System.IO;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;

public static class SnakeSkillV6 {
 static Color Ink=Color.FromArgb(13,22,40), Indigo=Color.FromArgb(40,30,71), Jade=Color.FromArgb(102,200,182),
              Teal=Color.FromArgb(48,131,137), Silver=Color.FromArgb(226,244,232),
              Violet=Color.FromArgb(116,92,172), Gold=Color.FromArgb(230,185,103);
 static PointF P(float x,float y){return new PointF(x,y);}
 static Color Alpha(Color c,int a){return Color.FromArgb(Math.Max(0,Math.Min(255,a)),c);}
 static void Circle(Graphics g,float x,float y,float d,Color c,int a){
  using(GraphicsPath p=new GraphicsPath()){p.AddEllipse(x-d/2,y-d/2,d,d);
   using(PathGradientBrush b=new PathGradientBrush(p)){b.CenterColor=Alpha(c,a);b.SurroundColors=new[]{Alpha(c,0)};g.FillPath(b,p);}
  }
 }
 static void E(Graphics g,float x,float y,float w,float h,Color c){using(SolidBrush b=new SolidBrush(c)){g.FillEllipse(b,x-w/2,y-h/2,w,h);}}
 static void Poly(Graphics g,Color c,params PointF[] q){using(SolidBrush b=new SolidBrush(c))g.FillPolygon(b,q);}
 static void C(Graphics g,Color c,float w,params PointF[] q){
   using(Pen p=new Pen(c,w)){p.LineJoin=LineJoin.Round;p.StartCap=LineCap.Round;p.EndCap=LineCap.Round;
   if(q.Length==2)g.DrawLine(p,q[0],q[1]);else g.DrawCurve(p,q,0.36f);}
 }
 static void Arc(Graphics g,Color c,float w,float x,float y,float d,float a,float sweep){
  using(Pen p=new Pen(c,w)){p.StartCap=LineCap.Round;p.EndCap=LineCap.Round;g.DrawArc(p,x,y,d,d,a,sweep);}
 }
 static void Path(Graphics g,float w,PointF[] pts){
   C(g,Alpha(Teal,70),w*2.3f,pts);C(g,Alpha(Ink,255),w*1.35f,pts);
   C(g,Indigo,w*1.17f,pts);C(g,Teal,w*1.06f,pts);
   C(g,Jade,w*.77f,pts);C(g,Alpha(Silver,220),w*.18f,pts);
 }
 static void Spark(Graphics g,float x,float y,float r,Color c){
   Circle(g,x,y,r*4,c,105);
   Poly(g,c,P(x-r,y),P(x-r*.16f,y-r*.16f),P(x,y-r*1.3f),P(x+r*.16f,y-r*.16f),
    P(x+r,y),P(x+r*.16f,y+r*.16f),P(x,y+r*1.3f),P(x-r*.16f,y+r*.16f));
 }
 // 主体鳞针由明确的两大片彩色宝石切面构成，目标显示尺寸48像素。
 static void Scale(Graphics g,float cx,float cy,float rot,float sz,Color primary){
  GraphicsState st=g.Save();g.TranslateTransform(cx,cy);g.RotateTransform(rot);g.ScaleTransform(sz,sz);
  Poly(g,Ink,P(-220,-60),P(-90,-129),P(175,-48),P(271,0),P(165,54),P(-99,125));
  Poly(g,primary,P(-190,-59),P(-79,-103),P(223,-4),P(-47,-11));
  Poly(g,Silver,P(-182,-64),P(-68,-102),P(169,-18),P(-67,-32));
  Poly(g,Violet,P(-183,58),P(-47,10),P(219,7),P(-80,103));
  Poly(g,Jade,P(-115,-7),P(223,0),P(-106,18));C(g,Silver,8,P(-95,-9),P(169,-1),P(227,0));
  g.Restore(st);
 }
 static void SnakeHead(Graphics g,float x,float y,float rotation,float scale){
  GraphicsState st=g.Save();g.TranslateTransform(x,y);g.RotateTransform(rotation);g.ScaleTransform(scale,scale);
  Poly(g,Ink,P(-108,-81),P(-48,-128),P(34,-132),P(118,-68),P(164,-28),P(107,43),P(9,88),P(-81,61));
  Poly(g,Teal,P(-91,-73),P(-39,-109),P(28,-107),P(137,-23),P(89,31),P(-53,30));
  Poly(g,Violet,P(-55,-64),P(8,-96),P(94,-25),P(21,24));
  Poly(g,Silver,P(-71,-77),P(-37,-102),P(30,-95),P(108,-37),P(38,-57));
  E(g,33,-20,24,31,Gold);E(g,36,-21,10,23,Ink);
  C(g,Ink,9,P(47,33),P(104,25),P(141,-13));
  C(g,Jade,7,P(142,-13),P(169,-21),P(189,-43));
  g.Restore(st);
 }
 static void Fang(Graphics g,float x,float y,float mirror){
  GraphicsState st=g.Save();g.TranslateTransform(x,y);g.ScaleTransform(mirror,1f);
  Poly(g,Ink,P(-148,-146),P(-49,-184),P(76,-77),P(111,129),P(38,242),P(20,119),P(-58,-27));
  Poly(g,Silver,P(-130,-140),P(-66,-167),P(41,-68),P(51,105),P(42,190),P(-4,72));
  Poly(g,Teal,P(38,-62),P(97,78),P(60,181),P(31,84));
  C(g,Alpha(Silver,180),11,P(-113,-139),P(-32,-103),P(30,-11),P(47,97));g.Restore(st);
 }
 static void Drop(Graphics g,float x,float y,float size){
   using(GraphicsPath path=new GraphicsPath()){
    path.AddBezier(x,y-size,x-size*.56f,y-size*.25f,x-size*.6f,y+size*.42f,x,y+size*.75f);
    path.AddBezier(x,y+size*.75f,x+size*.6f,y+size*.42f,x+size*.56f,y-size*.25f,x,y-size);
    using(LinearGradientBrush b=new LinearGradientBrush(P(x-size,y-size),P(x+size,y+size),Silver,Teal)){g.FillPath(b,path);}
    using(Pen p=new Pen(Ink,8)){g.DrawPath(p,path);}
   }E(g,x-size*.17f,y-size*.2f,size*.2f,size*.32f,Silver);
 }
 static void Back(Graphics g,int i){
  using(LinearGradientBrush b=new LinearGradientBrush(new Rectangle(0,0,1024,1024),Ink,Indigo,45f)){g.FillRectangle(b,0,0,1024,1024);}
  Circle(g,503,507,980,Teal,66);Circle(g,633,400,800,Violet,46);
  Circle(g,149,240,525,Teal,74);Circle(g,925,910,810,Ink,235);
  Arc(g,Alpha(Jade,37),23,175,185,695,205,119);
 }
 static void Draw(Graphics g,int i){
  Back(g,i);
  switch(i){
   case 0: // 幽鳞刺：单一斜贯大型鳞针
    Path(g,42,new[]{P(208,812),P(401,634),P(573,430),P(810,204)});
    Circle(g,554,494,560,Jade,92);Scale(g,547,502,-45,1.17f,Teal);
    SnakeHead(g,310,723,-39,.51f);Spark(g,826,204,42,Silver);break;
   case 1: // 毒牙：双弯牙与双毒滴
    Circle(g,525,551,773,Teal,122);
    C(g,Indigo,88,P(239,203),P(504,109),P(793,212));
    Fang(g,391,452,1.16f);Fang(g,660,452,-1.16f);
    Drop(g,458,794,119);Drop(g,624,835,82);
    Spark(g,513,669,31,Silver);break;
   case 2: // 双鳞弹：两枚独立交错弹体
    C(g,Alpha(Jade,90),109,P(179,792),P(359,628),P(644,364),P(841,238));
    C(g,Alpha(Violet,105),105,P(166,209),P(347,426),P(653,641),P(850,760));
    C(g,Jade,32,P(177,791),P(359,629),P(641,360),P(849,240));
    C(g,Silver,28,P(171,210),P(345,421),P(660,640),P(844,757));
    Scale(g,631,373,-42,.77f,Teal);Scale(g,635,646,42,.77f,Violet);
    Spark(g,839,228,28,Silver);Spark(g,857,766,28,Jade);break;
   case 3: // 万毒归鳞：三圈毒浪，中心聚合爆发
    Circle(g,509,509,874,Gold,57);
    for(int n=0;n<3;n++){float x=171+n*67,d=682-n*134;
      Arc(g,Alpha(Teal,65),72-n*14,x,x,d,195,291);
      Arc(g,n==2?Gold:Jade,42-n*7,x,x,d,195,296);
      Arc(g,Alpha(Silver,176),11,x+13,x+13,d-26,199,280);
    }
    Path(g,85,new[]{P(274,672),P(294,370),P(524,244),P(726,400),P(633,627)});
    SnakeHead(g,628,603,52,.78f);
    Circle(g,513,520,400,Jade,165);Spark(g,505,510,58,Silver);Spark(g,213,777,29,Gold);break;
   case 4: // 蛇行：地面S形运动，前冲蛇首
    C(g,Alpha(Jade,70),192,P(154,838),P(483,823),P(686,600),P(383,393),P(528,197),P(820,236));
    Path(g,99,new[]{P(156,813),P(466,814),P(667,596),P(390,399),P(536,215),P(784,233)});
    SnakeHead(g,774,227,-10,.75f);
    C(g,Silver,19,P(151,928),P(442,918),P(705,704));
    C(g,Violet,18,P(222,699),P(539,659),P(567,565));Spark(g,827,168,30,Jade);break;
   case 5: // 蜕鳞：空鳞壳剥离与新生躯体分离
    Circle(g,529,520,832,Jade,69);
    Arc(g,Alpha(Silver,85),109,207,165,670,128,297);
    Arc(g,Teal,84,221,177,640,126,299);
    Arc(g,Silver,24,235,191,610,132,284);
    Poly(g,Alpha(Violet,230),P(302,296),P(406,329),P(433,561),P(309,719),P(338,530));
    Poly(g,Alpha(Jade,190),P(730,240),P(655,328),P(688,570),P(796,697),P(723,513));
    Path(g,59,new[]{P(362,793),P(476,722),P(440,542),P(558,397),P(643,276)});
    SnakeHead(g,647,269,-38,.72f);Spark(g,811,685,26,Silver);break;
   case 6: // 缠纹：毒纹陷阱环束缚目标脚步
    Circle(g,519,523,823,Violet,85);
    Arc(g,Alpha(Teal,177),102,170,178,702,0,360);
    Arc(g,Silver,27,189,193,664,17,305);
    Arc(g,Violet,35,245,257,552,181,219);
    Poly(g,Ink,P(466,207),P(575,207),P(612,562),P(707,736),P(357,752),P(430,545));
    Poly(g,Violet,P(482,219),P(554,215),P(588,581),P(666,706),P(401,709),P(465,550));
    Path(g,73,new[]{P(265,658),P(414,520),P(630,530),P(747,670),P(669,781)});
    SnakeHead(g,684,741,163,.55f);Spark(g,523,442,33,Jade);break;
   case 7: // 封途：可绕行的S形地面毒径
    Circle(g,531,524,898,Teal,83);
    PointF[] pts={P(113,741),P(386,792),P(719,576),P(346,415),P(526,218),P(867,260)};
    C(g,Ink,235,pts);C(g,Violet,206,pts);C(g,Teal,159,pts);C(g,Jade,91,pts);
    C(g,Alpha(Silver,180),15,P(151,725),P(393,752),P(695,578),P(388,434),P(538,246),P(857,278));
    Drop(g,220,591,71);Drop(g,718,741,66);Spark(g,819,225,38,Silver);break;
  }
  Circle(g,528,492,420,Jade,i==3?32:21);
  Spark(g,141,269,15,Alpha(Jade,185));Spark(g,883,758,13,Alpha(Silver,155));
 }
 public static void Build(string root,string[] names){
  string masters=System.IO.Path.Combine(root,"Masters"), icons=System.IO.Path.Combine(root,"48");
  Directory.CreateDirectory(masters);Directory.CreateDirectory(icons);
  if(names.Length!=8)throw new InvalidOperationException("技能数必须等于8");
  for(int i=0;i<8;i++){
   using(Bitmap b=new Bitmap(1024,1024,PixelFormat.Format32bppArgb))
   using(Graphics g=Graphics.FromImage(b)){
    g.SmoothingMode=SmoothingMode.AntiAlias;g.CompositingQuality=CompositingQuality.HighQuality;
    g.InterpolationMode=InterpolationMode.HighQualityBicubic;g.PixelOffsetMode=PixelOffsetMode.HighQuality;
    Draw(g,i);
    b.Save(System.IO.Path.Combine(masters,names[i]+"_Master_1024.png"),ImageFormat.Png);
    using(Bitmap tiny=new Bitmap(48,48,PixelFormat.Format32bppArgb))
    using(Graphics gt=Graphics.FromImage(tiny)){
     gt.SmoothingMode=SmoothingMode.HighQuality;gt.InterpolationMode=InterpolationMode.HighQualityBicubic;
     gt.CompositingQuality=CompositingQuality.HighQuality;gt.PixelOffsetMode=PixelOffsetMode.HighQuality;
     gt.DrawImage(b,new Rectangle(0,0,48,48),new Rectangle(0,0,1024,1024),GraphicsUnit.Pixel);
     tiny.Save(System.IO.Path.Combine(icons,names[i]+".png"),ImageFormat.Png);
    }
    // 原生 PNG 交由外层脚本按文件数和散列验证，不依赖控制台输出。
   }
  }
 }
}
'@
Add-Type -TypeDefinition $cs -ReferencedAssemblies @('System.Drawing.Common','System.Drawing.Primitives','System.Private.Windows.GdiPlus','System.Private.Windows.Core') -ErrorAction Stop
$names=@('T_DBA_Snake_S01_ScalePierce_V6','T_DBA_Snake_S02_VenomFang_V6',
'T_DBA_Snake_S03_TwinScaleBolt_V6','T_DBA_Snake_S04_VenomTide_V6',
'T_DBA_Snake_S05_SerpentDash_V6','T_DBA_Snake_S06_ShedScales_V6',
'T_DBA_Snake_S07_CoilSnare_V6','T_DBA_Snake_S08_PoisonTrail_V6')
[SnakeSkillV6]::Build($OutputRoot,[string[]]$names)
Write-Output '八张独立1024美术初稿及八张48像素PNG生成完成；待人工美术评审。'
