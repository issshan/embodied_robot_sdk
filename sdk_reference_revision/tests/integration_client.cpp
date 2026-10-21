#include "robot_sdk.hpp"
#include <atomic>
#include <iostream>
#include <stdexcept>
using namespace embodied_robot;
void Require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(int argc,char** argv){
 try{
    Require(argc==3,"port and mode required");
    const std::string mode=argv[2];
    ClientOptions options;
    options.heartbeat_interval_ms=80; options.heartbeat_timeout_ms=350;
    options.reconnect_initial_ms=40; options.reconnect_max_ms=160;
    std::atomic<unsigned> states{0}; std::atomic<bool> bad{false};
    RobotSDK sdk("127.0.0.1",std::stoi(argv[1]),options);
    sdk.RegisterSensorCallback([&](uint16_t type,const uint8_t* p,size_t size){
       ChassisVelocity v{};
       if(type!=kRobotState||!DecodeVelocity(p,size,v)||v.linear_x!=0.25f)bad=true;
       ++states;
       if(mode=="callback_error")throw std::runtime_error("test callback");
    });
    Require(!sdk.SetChassisVelocity({0.25f,0,0}),"disconnected send accepted");
    Require(sdk.Init(),"start failed");
    if(mode!="absent")Require(sdk.WaitUntilConnected(2000),"connect timeout");
    const uint64_t started=platform::NowMs();
    if(mode=="absent")platform::SleepMs(150);
    else if(mode=="silent")platform::SleepMs(1100);
    else{
       uint64_t last_send=0;
       while(platform::NowMs()-started<1800){
          if(sdk.IsConnected()&&platform::NowMs()-last_send>=40){sdk.SetChassisVelocity({0.25f,0,0});last_send=platform::NowMs();}
          platform::SleepMs(5);
       }
    }
    const auto stop_start=platform::NowMs();sdk.Shutdown();
    const auto stop_ms=platform::NowMs()-stop_start;
    Require(stop_ms<1000,"shutdown too slow");Require(!sdk.IsConnected(),"connected after shutdown");
    auto s=sdk.Stats();
    if(mode=="normal"||mode=="callback_error"||mode=="faults"||mode=="reconnect"||mode=="delayed")Require(states>0&&!bad,"state echo missing/invalid");
    if(mode=="faults")Require(s.crc_errors>0&&s.invalid_lengths>0,"fault counters missing");
    if(mode=="reconnect")Require(s.connections>=2,"reconnect absent");
    if(mode=="silent")Require(s.heartbeat_timeouts>0&&s.connections>=2,"heartbeat recovery absent");
    if(mode=="callback_error")Require(s.callback_errors>0,"callback error not contained");
    if(mode=="absent")Require(s.connections==0,"unexpected server");
    Require(sdk.Init(),"restart failed");platform::SleepMs(30);sdk.Shutdown();sdk.Shutdown();
    std::cout<<"PASS mode="<<mode<<" states="<<states<<" connections="<<s.connections<<" crc="<<s.crc_errors<<" lengths="<<s.invalid_lengths<<" heartbeat_timeouts="<<s.heartbeat_timeouts<<" stop_ms="<<stop_ms<<'\n';return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
