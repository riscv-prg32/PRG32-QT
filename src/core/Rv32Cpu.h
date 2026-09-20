#pragma once
#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>
namespace prg32 {
class Rv32Cpu {
public:
 using HostCall=std::function<void(uint32_t)>;
 void reset(uint32_t base,std::vector<uint8_t>*memory); void setHostCall(HostCall c){host_=std::move(c);} void setHostRange(uint32_t base,uint32_t count){hostBase_=base;hostCount_=count;}
 bool call(uint32_t entry,uint32_t a0,uint64_t budget,std::string &error); bool step(std::string &error);
 uint32_t reg(unsigned i)const{return i?x_[i]:0;} void setReg(unsigned i,uint32_t v){if(i)x_[i]=v;} uint32_t pc()const{return pc_;}
 uint8_t load8(uint32_t a,bool &ok)const; uint16_t load16(uint32_t a,bool &ok)const; uint32_t load32(uint32_t a,bool &ok)const;
 void store8(uint32_t a,uint8_t v,bool &ok); void store16(uint32_t a,uint16_t v,bool &ok); void store32(uint32_t a,uint32_t v,bool &ok);
 uint32_t base()const{return base_;}
private:
 uint32_t sext(uint32_t v,unsigned bits)const; bool exec32(uint32_t ins,std::string&e); bool exec16(uint16_t ins,std::string&e);
 std::array<uint32_t,32>x_{}; uint32_t pc_=0,base_=0,hostBase_=0,hostCount_=0; uint64_t retired_=0; uint32_t reservation_=0; bool hasReservation_=false; std::vector<uint8_t>*mem_=nullptr; HostCall host_; static constexpr uint32_t ReturnSentinel=0xfffffffcu;
};
}
