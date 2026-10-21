#include "robot_sdk.hpp"
#include "send_all.hpp"
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace embodied_robot;
int checks = 0;
void Check(bool ok, const char* name) { ++checks; if (!ok) throw std::runtime_error(name); }
int main() {
    try {
        const uint8_t data[] = {1,2,3,4,5};
        Check(ProtocolParser::CalculateCRC16(reinterpret_cast<const uint8_t*>("123456789"),9)==0x4B37,"CRC known vector");
        const auto packet = ProtocolParser::Pack(0x1234, data,5);
        Check(packet[2]==0x34 && packet[3]==0x12 && packet[4]==5,"wire little endian");
        for (size_t split=0; split<=packet.size(); ++split) {
            ProtocolParser parser; Frame frame;
            Check(parser.Feed(packet.data(),split),"split feed first");
            const bool early=parser.Next(frame);
            Check(early == (split==packet.size()),"no premature frame");
            Check(parser.Feed(packet.data()+split,packet.size()-split),"split feed rest");
            if (!early) Check(parser.Next(frame),"split reconstructed");
            Check(frame.type==0x1234 && frame.payload==std::vector<uint8_t>(data,data+5),"payload exact");
        }
        ProtocolParser parser; Frame frame;
        for (size_t i=0;i<packet.size();++i) {
            Check(parser.Feed(&packet[i],1),"byte feed");
            Check(parser.Next(frame)==(i+1==packet.size()),"byte reconstruction");
        }
        auto joined=packet; joined.insert(joined.end(),packet.begin(),packet.end());
        parser.Reset(); Check(parser.Feed(joined.data(),joined.size()),"coalesced feed");
        Check(parser.Next(frame)&&parser.Next(frame)&&!parser.Next(frame),"coalesced frames");
        auto corrupt=packet; corrupt[8]^=0x80;
        corrupt.insert(corrupt.end(),packet.begin(),packet.end());
        parser.Reset(); parser.Feed(corrupt.data(),corrupt.size());
        Check(parser.Next(frame)&&frame.payload.size()==5&&!parser.Next(frame),"CRC reject and resync");
        Check(parser.Stats().crc_errors==1,"CRC counter");
        std::vector<uint8_t> invalid{0x5a,0x5a,1,0,0xff,0xff,0xff,0xff};
        invalid.insert(invalid.end(),packet.begin(),packet.end());
        parser.Reset(); parser.Feed(invalid.data(),invalid.size());
        Check(parser.Next(frame)&&parser.Stats().invalid_lengths==1,"length reject resync");
        std::vector<uint8_t> maximum(kMaxPayload,0x42);
        auto largest=ProtocolParser::Pack(5,maximum.data(),maximum.size());
        parser.Reset(); parser.Feed(largest.data(),largest.size());
        Check(parser.Next(frame)&&frame.payload==maximum,"max length");
        std::vector<uint8_t> overflow(10000,0);
        Check(!parser.Feed(overflow.data(),overflow.size()),"overflow rejected");
        bool caught=false;
        try { ProtocolParser::Pack(1,nullptr,1); } catch(const std::invalid_argument&) { caught=true; }
        Check(caught,"null payload rejected");
        caught=false;
        try { ProtocolParser::Pack(1,maximum.data(),kMaxPayload+1); } catch(const std::invalid_argument&) { caught=true; }
        Check(caught,"oversize pack rejected");
        auto empty=ProtocolParser::Pack(9,nullptr,0);
        parser.Reset(); parser.Feed(empty.data(),empty.size()); Check(parser.Next(frame)&&frame.payload.empty(),"empty payload");
        RingBuffer ring(8); uint8_t out[8]{};
        Check(ring.Write(data,5)==5,"ring write"); ring.Consume(4);
        Check(ring.Write(data,5)==5,"ring wrap write");
        Check(ring.Peek(out,6)==6&&out[0]==5&&std::memcmp(out+1,data,5)==0,"ring wrap read");
        Check(ring.Write(data,5)==0&&ring.Size()==6,"ring overflow atomic");
        ring.Consume(99); Check(ring.Size()==0&&ring.FreeSpace()==8,"ring consume clear");
        caught=false; try { RingBuffer zero(0); } catch(const std::invalid_argument&) { caught=true; }
        Check(caught,"zero capacity rejected");
        auto velocity=EncodeVelocity({1.0f,-2.0f,0.5f}); ChassisVelocity decoded{};
        Check(velocity[2]==0x80 && velocity[3]==0x3f,"float endian");
        Check(DecodeVelocity(velocity.data(),12,decoded)&&decoded.linear_y==-2.0f,"velocity roundtrip");
        Check(!DecodeVelocity(velocity.data(),11,decoded),"velocity bad length");
        caught=false;try { EncodeVelocity({std::numeric_limits<float>::infinity(),0,0}); }catch(const std::invalid_argument&){caught=true;}
        Check(caught,"nonfinite rejected");
        size_t sent=0;int calls=0;
        Check(SendAll(data,5,[&](const uint8_t* p,size_t n){++calls;if(calls==2)return 0;Check(p==data+sent,"short write pointer");int k=static_cast<int>(std::min<size_t>(2,n));sent+=k;return k;},[]{return true;})&&sent==5&&calls==4,"short write retry");
        Check(!SendAll(data,5,[](const uint8_t*,size_t){return -1;},[]{return true;}),"fatal send");
        Check(!SendAll(data,5,[](const uint8_t*,size_t){return 0;},[]{return false;}),"send deadline");
        caught=false;try { TcpClient invalid_client("bad",8888); }catch(const std::invalid_argument&){caught=true;}
        Check(caught,"IP validated");
        std::cout<<"PASS checks="<<checks<<'\n';return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
