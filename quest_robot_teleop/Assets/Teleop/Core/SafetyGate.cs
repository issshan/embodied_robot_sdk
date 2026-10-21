using System;
namespace QuestRobotTeleop {
public enum ControlState { Uncalibrated, Idle, Active, TrackingLost, Disconnected, EmergencyStop }
public sealed class SafetyGate {
 public ControlState State{get;private set;}=ControlState.Uncalibrated;
 public bool Latched{get;private set;}
 public V3 Output{get;private set;}=new V3(0.35,0,0.2);
 public V3 Min=new V3(0.15,-0.25,0.05),Max=new V3(0.5,0.25,0.3);
 public double MaxSpeed=0.3;
 bool releaseRequired=true;
 public void Trip(){Latched=true;releaseRequired=true;State=ControlState.EmergencyStop;}
 public void Reset(bool triggerHeld){if(!triggerHeld){Latched=false;releaseRequired=true;State=ControlState.Idle;}}
 public V3 Step(V3 desired,bool calibrated,bool tracking,bool connected,bool trigger,double dt){
  if(!desired.Finite||double.IsNaN(dt)||double.IsInfinity(dt)||dt<=0||dt>0.25){Trip();return Output;}
  if(!trigger)releaseRequired=false;
  if(Latched){State=ControlState.EmergencyStop;return Output;}
  if(!calibrated){State=ControlState.Uncalibrated;releaseRequired=true;return Output;}
  if(!tracking){State=ControlState.TrackingLost;releaseRequired=true;return Output;}
  if(!connected){State=ControlState.Disconnected;releaseRequired=true;return Output;}
  if(!trigger||releaseRequired){State=ControlState.Idle;return Output;}
  State=ControlState.Active;
  V3 target=V3.Clamp(desired,Min,Max),delta=target-Output;
  double step=MaxSpeed*dt;
  Output=delta.Length>step?Output+delta*(step/delta.Length):target;
  return Output;
 }
}
// Reject old UDP acks and future/unsent sequence values; use receiver-local time.
public sealed class AckMonitor {
 long last=-1;double receivedAt=double.NegativeInfinity;
 public bool Accept(long seq,long highestSent,double now){if(seq<=last||seq>highestSent||seq<0||double.IsNaN(now)||double.IsInfinity(now))return false;last=seq;receivedAt=now;return true;}
 public bool Alive(double now,double timeout=0.3)=>now>=receivedAt&&now-receivedAt<timeout;
}
[Serializable] public sealed class CommandPacket {
 public int version=1;public string session="";public long seq;public bool enabled;public V3 target;public Q4 orientation=Q4.Identity;public string state="Idle";
}
[Serializable] public sealed class StatePacket {
 public int version;public string session="";public long ack;public bool enabled;public V3 target;public string status="";
}
[Serializable] public sealed class RecordFrame {
 public double time;public PoseSample controller;public V3 target;public Q4 orientation;public string state="";
}
}
