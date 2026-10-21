using QuestRobotTeleop;
using System.Text.Json;
class Program {
 static int checks;
 static void Check(bool value,string name){checks++;if(!value)throw new Exception(name);}
 static void Near(V3 a,V3 b,string name){Check((a-b).Length<1e-8,name);}
 static void Main(string[] args){
  var v=new V3(1,2,3);Near(Frames.RobotToUnity(Frames.UnityToRobot(v)),v,"frame inverse");
  Near(Frames.UnityToRobot(new V3(0,0,1)),new V3(1,0,0),"forward axis");
  Near(Frames.UnityToRobot(new V3(1,0,0)),new V3(0,-1,0),"right to left sign");
  for(int i=0;i<40;i++){
   var q=new Q4(i+1,2,-3,4).Normalized;var p=new V3(i*.1,-.2,.7);
   Near(Frames.UnityToRobot(q.Rotate(p)),Frames.UnityToRobot(q).Rotate(Frames.UnityToRobot(p)),"quaternion reflection invariant");
  }
  var mapper=new RelativeMapper();bool caught=false;
  try{mapper.Map(new PoseSample(v,Q4.Identity),1);}catch(ArgumentException){caught=true;}
  Check(caught,"mapping requires calibration");
  var home=new V3(.35,0,.2);mapper.Calibrate(new PoseSample(v,Q4.Identity),home,Q4.Identity);
  Near(mapper.Map(new PoseSample(v,Q4.Identity),1).position,home,"calibration zero jump");
  Near(mapper.Map(new PoseSample(v+new V3(0,0,.2),Q4.Identity),.5).position,home+new V3(.1,0,0),"relative scale");
  var gate=new SafetyGate();gate.Step(home,false,true,true,false,.02);Check(gate.State==ControlState.Uncalibrated,"uncalibrated");
  gate.Step(home,true,true,true,true,.02);Check(gate.State==ControlState.Idle,"must release after calibration");
  gate.Step(home,true,true,true,false,.02);var a=gate.Output;
  var b=gate.Step(new V3(10,10,10),true,true,true,true,.02);
  Check(gate.State==ControlState.Active,"deadman activates");Check((b-a).Length<=.00600001,"speed limit");
  gate.Step(home,true,false,true,true,.02);Check(gate.State==ControlState.TrackingLost,"tracking stop");
  gate.Step(home,true,true,true,true,.02);Check(gate.State==ControlState.Idle,"no automatic rearm");
  gate.Step(home,true,true,true,false,.02);gate.Step(home,true,true,true,true,.02);Check(gate.State==ControlState.Active,"manual rearm");
  gate.Step(home,true,true,false,true,.02);Check(gate.State==ControlState.Disconnected,"network stop");
  gate.Trip();gate.Reset(true);Check(gate.Latched,"held reset rejected");gate.Reset(false);Check(!gate.Latched,"reset released");
  gate.Step(new V3(double.NaN,0,0),true,true,true,false,.02);Check(gate.Latched,"nonfinite fail closed");
  var safe=new SafetyGate();safe.Step(home,true,true,true,false,.02);
  for(int i=0;i<500;i++){var old=safe.Output;var next=safe.Step(new V3(Math.Sin(i)*10,Math.Cos(i)*10,i%2),true,true,true,true,.02);Check((next-old).Length<=.00600001,"speed property");Check(next.x>=safe.Min.x&&next.x<=safe.Max.x&&next.y>=safe.Min.y&&next.y<=safe.Max.y&&next.z>=safe.Min.z&&next.z<=safe.Max.z,"workspace property");}
  V3 joints;Check(ArmKinematics.Solve(home,.35,.3,out joints),"IK home");Near(ArmKinematics.Forward(joints,.35,.3),home,"FK IK roundtrip");Check(!ArmKinematics.Solve(new V3(2,0,0),.35,.3,out joints),"unreachable");
  for(int i=0;i<30;i++){var target=new V3(.2+i*.005,.1,.2);Check(ArmKinematics.Solve(target,.35,.3,out joints),"IK workspace");Near(ArmKinematics.Forward(joints,.35,.3),target,"FK IK property");}
  var monitor=new AckMonitor();Check(!monitor.Alive(1),"no ack disconnected");Check(monitor.Accept(2,2,1),"ack accepted");Check(!monitor.Accept(2,3,1.1),"duplicate ignored");Check(!monitor.Accept(1,3,1.1),"old ignored");Check(!monitor.Accept(9,3,1.1),"future ignored");Check(monitor.Alive(1.2),"ack alive");Check(!monitor.Alive(1.4),"ack expires");
  var options=new JsonSerializerOptions{IncludeFields=true,IgnoreReadOnlyProperties=true};var packet=new CommandPacket{session="test",seq=1,target=home};var json=JsonSerializer.Serialize(packet,options);var copy=JsonSerializer.Deserialize<CommandPacket>(json,options);Near(copy.target,home,"wire json roundtrip");Check(!copy.enabled,"default disabled");
  Console.WriteLine($"PASS core checks={checks}");
  if(args.Length==2){
   using var udp=new System.Net.Sockets.UdpClient();udp.Connect(args[0],int.Parse(args[1]));udp.Client.ReceiveTimeout=1000;
   var network=new CommandPacket{session=Guid.NewGuid().ToString("N"),seq=0,enabled=false,target=home};
   StatePacket Exchange(){byte[] bytes=System.Text.Encoding.UTF8.GetBytes(JsonSerializer.Serialize(network,options));udp.Send(bytes,bytes.Length);System.Net.IPEndPoint peer=null;return JsonSerializer.Deserialize<StatePacket>(System.Text.Encoding.UTF8.GetString(udp.Receive(ref peer)),options);}
   var initial=Exchange();Check(initial.session==network.session&&initial.ack==0&&!initial.enabled,"network handshake");
   System.Threading.Thread.Sleep(50);network.seq=1;network.enabled=true;network.target=new V3(.5,0,.2);var response=Exchange();
   Check(response.ack==1&&response.enabled&&response.target.x>home.x&&response.target.x<=home.x+.031,"network limited motion");
   network.seq=2;network.enabled=false;Check(!Exchange().enabled,"network disable");
   Console.WriteLine("PASS UDP CSharp-to-Python handshake + motion + disable");
  }
 }
}
