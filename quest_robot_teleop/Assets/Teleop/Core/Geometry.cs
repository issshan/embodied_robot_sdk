using System;
namespace QuestRobotTeleop {
[Serializable] public struct V3 {
 public double x,y,z;
 public V3(double x,double y,double z){this.x=x;this.y=y;this.z=z;}
 public static V3 operator+(V3 a,V3 b)=>new V3(a.x+b.x,a.y+b.y,a.z+b.z);
 public static V3 operator-(V3 a,V3 b)=>new V3(a.x-b.x,a.y-b.y,a.z-b.z);
 public static V3 operator*(V3 a,double s)=>new V3(a.x*s,a.y*s,a.z*s);
 public double Length=>Math.Sqrt(x*x+y*y+z*z);
 public bool Finite=>!(double.IsNaN(x)||double.IsNaN(y)||double.IsNaN(z)||double.IsInfinity(x)||double.IsInfinity(y)||double.IsInfinity(z));
 public static V3 Clamp(V3 p,V3 lo,V3 hi)=>new V3(Math.Max(lo.x,Math.Min(hi.x,p.x)),Math.Max(lo.y,Math.Min(hi.y,p.y)),Math.Max(lo.z,Math.Min(hi.z,p.z)));
}
[Serializable] public struct Q4 {
 public double x,y,z,w;
 public Q4(double x,double y,double z,double w){this.x=x;this.y=y;this.z=z;this.w=w;}
 public static Q4 Identity=>new Q4(0,0,0,1);
 public Q4 Normalized {get{double n=Math.Sqrt(x*x+y*y+z*z+w*w);if(double.IsNaN(n)||double.IsInfinity(n)||n<1e-9)throw new ArgumentException("invalid quaternion");return new Q4(x/n,y/n,z/n,w/n);}}
 public Q4 Inverse {get{var q=Normalized;return new Q4(-q.x,-q.y,-q.z,q.w);}}
 public static Q4 operator*(Q4 a,Q4 b)=>new Q4(a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w,a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z);
 public V3 Rotate(V3 p){var q=Normalized;var v=q*new Q4(p.x,p.y,p.z,0)*q.Inverse;return new V3(v.x,v.y,v.z);}
}
[Serializable] public struct PoseSample {
 public V3 position;public Q4 rotation;
 public PoseSample(V3 p,Q4 q){position=p;rotation=q.Normalized;}
}
public static class Frames {
 // Unity: right/up/forward. Robot: forward/left/up. S has determinant -1.
 public static V3 UnityToRobot(V3 p)=>new V3(p.z,-p.x,p.y);
 public static V3 RobotToUnity(V3 p)=>new V3(-p.y,p.z,p.x);
 // Axial vector transforms with det(S)*S, unlike position.
 public static Q4 UnityToRobot(Q4 q){q=q.Normalized;return new Q4(-q.z,q.x,-q.y,q.w);}
}
public sealed class RelativeMapper {
 public bool Calibrated{get;private set;}
 PoseSample neutral;V3 home;Q4 homeRotation;
 public void Calibrate(PoseSample controller,V3 robotHome,Q4 robotRotation){
  if(!controller.position.Finite||!robotHome.Finite)throw new ArgumentException("invalid calibration");
  neutral=new PoseSample(controller.position,controller.rotation);home=robotHome;homeRotation=robotRotation.Normalized;Calibrated=true;
 }
 public PoseSample Map(PoseSample current,double scale){
  if(!Calibrated||!current.position.Finite||double.IsNaN(scale)||scale<=0||scale>5)throw new ArgumentException("invalid mapping");
  // Input deltas are expressed in the calibrated controller frame.
  V3 delta=neutral.rotation.Inverse.Rotate(current.position-neutral.position);
  Q4 deltaRotation=neutral.rotation.Inverse*current.rotation.Normalized;
  return new PoseSample(home+homeRotation.Rotate(Frames.UnityToRobot(delta)*scale),homeRotation*Frames.UnityToRobot(deltaRotation));
 }
}
// Position-only 3DoF yaw + two-link arm. NOT a 6DoF industrial arm solver.
public static class ArmKinematics {
 public static bool Solve(V3 target,double l1,double l2,out V3 joints){
  joints=new V3();if(!target.Finite||l1<=0||l2<=0)return false;
  double radial=Math.Sqrt(target.x*target.x+target.y*target.y);
  double d=Math.Sqrt(radial*radial+target.z*target.z);
  if(d>l1+l2+1e-9||d<Math.Abs(l1-l2)-1e-9||d<1e-6)return false;
  double c=Math.Max(-1,Math.Min(1,(d*d-l1*l1-l2*l2)/(2*l1*l2)));
  double elbow=Math.Acos(c),shoulder=Math.Atan2(target.z,radial)-Math.Atan2(l2*Math.Sin(elbow),l1+l2*Math.Cos(elbow));
  joints=new V3(Math.Atan2(target.y,target.x),shoulder,elbow);return true;
 }
 public static V3 Forward(V3 j,double l1,double l2){
  double radial=l1*Math.Cos(j.y)+l2*Math.Cos(j.y+j.z);
  return new V3(radial*Math.Cos(j.x),radial*Math.Sin(j.x),l1*Math.Sin(j.y)+l2*Math.Sin(j.y+j.z));
 }
}
}
