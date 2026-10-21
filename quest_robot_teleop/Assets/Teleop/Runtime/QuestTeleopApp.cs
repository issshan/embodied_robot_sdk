using System;
using System.IO;
using System.Collections.Generic;
using System.Net;
using System.Net.Sockets;
using System.Text;
using UnityEngine;
using UnityEngine.XR;
#if ENABLE_INPUT_SYSTEM
using UnityEngine.InputSystem;
#endif
namespace QuestRobotTeleop {
public sealed class QuestTeleopApp : MonoBehaviour {
 [Header("Simulator only; no hardware driver")]
 public string serverIp="127.0.0.1";public int serverPort=8765;public bool desktopSimulation=true;
 public float positionScale=0.5f;
 RelativeMapper mapper=new RelativeMapper();SafetyGate gate=new SafetyGate();AckMonitor ack=new AckMonitor();
 UdpClient udp;string session=Guid.NewGuid().ToString("N");long seq=-1;double lastSend;PoseSample input=new PoseSample(new V3(0,1,0),Q4.Identity);
 Camera view;TextMesh label;LineRenderer upper,lower;Transform marker;V3 joints;V3 simulated=new V3(0,1,0);
 bool prevCal,prevStop,prevReset,prevRecord,prevReplay,recording,replaying;StreamWriter writer;
 List<RecordFrame> replay=new List<RecordFrame>();int replayIndex;double replayStart,lastRecord;string recordingPath="",notice="";float fps=60;
 static Vector3 U(V3 p)=>new Vector3((float)p.x,(float)p.y,(float)p.z);
 static V3 V(Vector3 p)=>new V3(p.x,p.y,p.z);
 static Q4 Q(Quaternion q)=>new Q4(q.x,q.y,q.z,q.w);
 void Start(){
  view=Camera.main;if(view==null){var c=new GameObject("XR Camera");view=c.AddComponent<Camera>();c.tag="MainCamera";c.AddComponent<AudioListener>();}
  if(desktopSimulation){view.transform.position=new Vector3(0,0.7f,-1.1f);view.transform.LookAt(new Vector3(0,0.25f,0.4f));}
  upper=MakeLine("upper");lower=MakeLine("lower");marker=GameObject.CreatePrimitive(PrimitiveType.Sphere).transform;marker.name="target";marker.localScale=Vector3.one*0.04f;
  var board=new GameObject("Instructions");label=board.AddComponent<TextMesh>();label.fontSize=35;label.characterSize=0.008f;label.anchor=TextAnchor.UpperLeft;board.transform.position=new Vector3(-0.65f,0.85f,1.1f);
  try{IPAddress ip;if(!IPAddress.TryParse(serverIp,out ip)||ip.AddressFamily!=AddressFamily.InterNetwork)throw new ArgumentException("IPv4 required");udp=new UdpClient();udp.Connect(ip,serverPort);udp.Client.Blocking=false;}catch(Exception e){notice=e.Message;udp?.Close();udp=null;}
 }
 LineRenderer MakeLine(string name){var line=new GameObject(name).AddComponent<LineRenderer>();line.positionCount=2;line.startWidth=line.endWidth=0.035f;line.material=new Material(Shader.Find("Sprites/Default"));line.startColor=line.endColor=Color.cyan;return line;}
 bool ReadDevice(XRNode node,out PoseSample pose,out InputDevice device){
  device=InputDevices.GetDeviceAtXRNode(node);pose=new PoseSample(new V3(),Q4.Identity);
  bool tracked;InputTrackingState state;Vector3 p;Quaternion q;
  if(!device.isValid||!device.TryGetFeatureValue(CommonUsages.isTracked,out tracked)||!tracked||!device.TryGetFeatureValue(CommonUsages.trackingState,out state))return false;
  if((state&(InputTrackingState.Position|InputTrackingState.Rotation))!=(InputTrackingState.Position|InputTrackingState.Rotation))return false;
  if(!device.TryGetFeatureValue(CommonUsages.devicePosition,out p)||!device.TryGetFeatureValue(CommonUsages.deviceRotation,out q))return false;
  try{pose=new PoseSample(V(p),Q(q));return pose.position.Finite;}catch(ArgumentException){return false;}
 }
 bool Button(InputDevice d,InputFeatureUsage<bool> usage){bool value;return d.TryGetFeatureValue(usage,out value)&&value;}
 void Update(){
  double now=Time.realtimeSinceStartupAsDouble;double dt=Time.unscaledDeltaTime;
  bool tracked=false,held=false,cal=false,stop=false,reset=false,rec=false,play=false;
  if(desktopSimulation){
#if ENABLE_INPUT_SYSTEM
   var k=Keyboard.current;if(k!=null){float speed=0.25f*Time.unscaledDeltaTime;simulated+=new V3((k.rightArrowKey.isPressed?speed:0)-(k.leftArrowKey.isPressed?speed:0),(k.pageUpKey.isPressed?speed:0)-(k.pageDownKey.isPressed?speed:0),(k.upArrowKey.isPressed?speed:0)-(k.downArrowKey.isPressed?speed:0));held=k.spaceKey.isPressed;cal=k.cKey.isPressed;stop=k.eKey.isPressed;reset=k.rKey.isPressed;rec=k.fKey.isPressed;play=k.pKey.isPressed;tracked=!k.tKey.isPressed;}
#endif
   input=new PoseSample(simulated,Q4.Identity);
  }else{
   InputDevice right,left,head;PoseSample headPose,leftPose;
   bool handOk=ReadDevice(XRNode.RightHand,out input,out right),headOk=ReadDevice(XRNode.Head,out headPose,out head);
   ReadDevice(XRNode.LeftHand,out leftPose,out left);tracked=handOk&&headOk;
   if(headOk){view.transform.localPosition=U(headPose.position);view.transform.localRotation=new Quaternion((float)headPose.rotation.x,(float)headPose.rotation.y,(float)headPose.rotation.z,(float)headPose.rotation.w);}
   held=Button(right,CommonUsages.triggerButton);cal=Button(right,CommonUsages.gripButton);stop=Button(right,CommonUsages.primaryButton);reset=Button(right,CommonUsages.secondaryButton);rec=Button(left,CommonUsages.primaryButton);play=Button(left,CommonUsages.secondaryButton);
  }
  Receive(now);
  if(stop&&!prevStop)gate.Trip();if(reset&&!prevReset)gate.Reset(held);
  if(cal&&!prevCal&&!held&&tracked&&!replaying){mapper.Calibrate(input,gate.Output,Q4.Identity);notice="Calibrated; release then hold trigger";}
  PoseSample mapped=new PoseSample(gate.Output,Q4.Identity);
  try{if(mapper.Calibrated&&tracked)mapped=mapper.Map(input,positionScale);}catch(ArgumentException){gate.Trip();}
  V3 target=gate.Step(mapped.position,mapper.Calibrated,tracked&&!replaying,ack.Alive(now),held,dt);
  if(rec&&!prevRecord){if(!recording&&!replaying)BeginRecording();else EndRecording();}
  if(play&&!prevReplay){if(replaying)replaying=false;else BeginReplay(now);}
  if(recording&&now-lastRecord>=1.0/30){try{writer.WriteLine(JsonUtility.ToJson(new RecordFrame{time=now,controller=input,target=target,orientation=mapped.rotation,state=gate.State.ToString()}));lastRecord=now;}catch(Exception e){notice=e.Message;EndRecording();}}
  if(now-lastSend>=1.0/30){Send(target,mapped.rotation,gate.State==ControlState.Active&&!replaying);lastSend=now;}
  V3 visual=target;
  if(replaying&&replay.Count>0){double elapsed=now-replayStart;while(replayIndex+1<replay.Count&&replay[replayIndex+1].time-replay[0].time<=elapsed)replayIndex++;visual=replay[replayIndex].target;if(replayIndex==replay.Count-1)replaying=false;}
  DrawArm(visual);fps=Mathf.Lerp(fps,1f/Mathf.Max(Time.unscaledDeltaTime,0.001f),0.04f);
  label.text=$"Quest Robot Teleop - SIMULATOR ONLY\n{gate.State} | peer={ack.Alive(now)} | fps~{fps:F0}\nCalibrate: grip / C   Enable: trigger / Space\nE-stop: A / E   Reset: B / R (trigger released)\nRecord: X / F   Replay visualization: Y / P\nDesktop arrows + PgUp/PgDn; T tracking loss\nRecording={recording} Replay={replaying}\n{notice}";
  prevCal=cal;prevStop=stop;prevReset=reset;prevRecord=rec;prevReplay=play;
 }
 void Receive(double now){if(udp==null)return;for(int n=0;n<32;n++){try{if(udp.Available==0)break;IPEndPoint peer=null;byte[] bytes=udp.Receive(ref peer);if(bytes.Length>2048)continue;var p=JsonUtility.FromJson<StatePacket>(Encoding.UTF8.GetString(bytes));if(p!=null&&p.version==1&&p.session==session&&p.target.Finite)ack.Accept(p.ack,seq,now);}catch(SocketException){break;}catch(ArgumentException){notice="Invalid simulator packet ignored";}}}
 void Send(V3 target,Q4 orientation,bool enabled){if(udp==null)return;var p=new CommandPacket{session=session,seq=++seq,enabled=enabled,target=target,orientation=orientation,state=gate.State.ToString()};byte[] bytes=Encoding.UTF8.GetBytes(JsonUtility.ToJson(p));try{udp.Send(bytes,bytes.Length);}catch(SocketException){notice="Simulator unavailable";}}
 void DrawArm(V3 target){marker.position=U(Frames.RobotToUnity(target));if(!ArmKinematics.Solve(target,0.35,0.30,out joints)){notice="Target unreachable";return;}double r=0.35*Math.Cos(joints.y);V3 elbow=new V3(r*Math.Cos(joints.x),r*Math.Sin(joints.x),0.35*Math.Sin(joints.y));upper.SetPosition(0,Vector3.zero);upper.SetPosition(1,U(Frames.RobotToUnity(elbow)));lower.SetPosition(0,U(Frames.RobotToUnity(elbow)));lower.SetPosition(1,U(Frames.RobotToUnity(target)));}
 void BeginRecording(){try{string dir=Path.Combine(Application.persistentDataPath,"recordings");Directory.CreateDirectory(dir);recordingPath=Path.Combine(dir,DateTime.UtcNow.ToString("yyyyMMdd_HHmmss_fff")+".jsonl");writer=new StreamWriter(new FileStream(recordingPath,FileMode.CreateNew,FileAccess.Write,FileShare.Read));recording=true;notice="Recording to local app storage";}catch(Exception e){notice=e.Message;}}
 void EndRecording(){recording=false;writer?.Dispose();writer=null;}
 void BeginReplay(double now){EndRecording();if(string.IsNullOrEmpty(recordingPath)||!File.Exists(recordingPath)){notice="Record a session first";return;}replay.Clear();try{foreach(string line in File.ReadLines(recordingPath)){if(replay.Count>=18000)break;var f=JsonUtility.FromJson<RecordFrame>(line);if(f==null||!f.target.Finite||double.IsNaN(f.time)||double.IsInfinity(f.time)||(replay.Count>0&&f.time<replay[replay.Count-1].time))throw new InvalidDataException("invalid replay");replay.Add(f);}if(replay.Count>0){gate.Trip();replaying=true;replayStart=now;replayIndex=0;}}catch(Exception e){notice=e.Message;replaying=false;}}
 void OnApplicationPause(bool paused){if(paused){gate.Trip();EndRecording();}}
 void OnDisable(){gate.Trip();EndRecording();udp?.Close();udp=null;}
}
}
