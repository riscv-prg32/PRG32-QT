#include "Audio.h"
namespace prg32 {
static uint16_t r16(const uint8_t*p){return uint16_t(p[0])|uint16_t(p[1])<<8;} static uint32_t r32(const uint8_t*p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
std::optional<AudioBlock> AudioBlock::parse(std::span<const uint8_t>d,std::string&e){
 if(d.size()<40||std::string((const char*)d.data(),4)!="AUD0"){e="invalid AUD0 audio block";return std::nullopt;} if(r16(d.data()+4)!=1){e="unsupported AUD0 version";return std::nullopt;}
 const size_t hs=r16(d.data()+6),sc=r16(d.data()+8),ic=r16(d.data()+10),tc=r16(d.data()+12),so=r32(d.data()+16),io=r32(d.data()+20),to=r32(d.data()+24),eo=r32(d.data()+28),po=r32(d.data()+32),bs=r32(d.data()+36);
 if(hs<40||bs<hs||bs>d.size()){e="invalid AUD0 block size";return std::nullopt;} AudioBlock out;
 struct SD{uint32_t o,l,ls,le;uint16_t note;uint8_t flags;}; std::vector<SD> sd;
 for(size_t n=0;n<sc;n++){size_t o=so+n*20;if(o+20>bs){e="AUD0 sample descriptor overflow";return std::nullopt;}sd.push_back({r32(d.data()+o),r32(d.data()+o+4),r32(d.data()+o+8),r32(d.data()+o+12),r16(d.data()+o+16),d[o+18]});}
 for(size_t n=0;n<ic;n++){size_t o=io+n*8;if(o+8>bs){e="AUD0 instrument descriptor overflow";return std::nullopt;}out.instruments.push_back({r16(d.data()+o),d[o+2],int8_t(d[o+3]),d[o+4],d[o+5],d[o+6],d[o+7]});}
 struct TD{uint32_t idx,count;};std::vector<TD> td;for(size_t n=0;n<tc;n++){size_t o=to+n*8;if(o+8>bs){e="AUD0 track descriptor overflow";return std::nullopt;}td.push_back({r32(d.data()+o),r32(d.data()+o+4)});}
 for(auto&s:sd){if(uint64_t(po)+s.o+s.l>bs){e="AUD0 sample payload overflow";return std::nullopt;}AudioSample a;a.bytes.assign(d.begin()+po+s.o,d.begin()+po+s.o+s.l);a.loopStart=int(std::min<uint32_t>(s.l,s.ls));a.loopEnd=int(std::min<uint32_t>(s.l,s.le));a.baseNote=s.note;a.loop=(s.flags&1)!=0;out.samples.push_back(std::move(a));}
 for(auto&t:td){AudioTrack tr;for(uint32_t n=0;n<t.count;n++){size_t o=eo+(uint64_t(t.idx)+n)*4;if(o+4>bs){e="AUD0 event overflow";return std::nullopt;}tr.events.push_back({d[o],d[o+1],d[o+2],d[o+3]});}out.tracks.push_back(std::move(tr));}
 return out;
}
}
