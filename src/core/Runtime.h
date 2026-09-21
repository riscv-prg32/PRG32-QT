#pragma once
#include "Audio.h"
#include "Cartridge.h"
#include "Framebuffer.h"
#include "Rv32Cpu.h"
#include <array>
#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
namespace prg32 {
class Runtime {
public:
 struct PerfCase { uint32_t index=0, mode=0, first=0, frames=0, min=0, mean=0, p50=0, p95=0, p99=0, max=0, missed=0, update=0, draw=0, present=0; std::string name, goal; std::vector<uint32_t> samples; uint64_t updateTotal=0, drawTotal=0, presentTotal=0, frameTotal=0; };
 struct PerfSnapshot { uint32_t state=0, sequence=0, suiteVersion=0; uint64_t started=0, ended=0; std::string name; std::vector<PerfCase> cases; };
 struct Score{std::string game,player;uint32_t score;};
 static constexpr uint32_t CurrentAbiHash=0x260f6136u,CompatHash0=0x006427c2u,CompatHash1=0x6be6e8d0u,HostBase=0xf0000000u;
 static constexpr uint32_t FeatureAudio=1u<<0,FeatureWifi=1u<<1,FeatureMultiplayer=1u<<2,FeatureMetrics=1u<<3,FeatureAudioPlus=1u<<4,FeatureKeyboard=1u<<5,FeatureTilemap=1u<<6,FeaturePlatformer=1u<<7,FeatureSprites=1u<<8;
 static constexpr uint32_t ProvidedFeatures=FeatureAudio|FeatureMetrics|FeatureAudioPlus|FeatureTilemap|FeaturePlatformer|FeatureSprites;
 bool load(const Cartridge&,std::string&);bool init(std::string&);bool frame(uint32_t,std::string&);void stop();void pause(){lastAudioUs_=0;if(audio_)audio_->stopAll();}
 Framebuffer&framebuffer(){return fb_;}const Framebuffer&framebuffer()const{return fb_;}uint32_t input()const{return input_;}bool fullscreen()const{return fullscreen_;}std::string lastLog()const{return log_;}RGBState rgbLED()const{return led_;}
 const Cartridge& cartridge()const{return cart_;} bool loaded()const{return loaded_;}const PerfSnapshot& performance()const{return perf_;}
 const std::vector<Score>& scores()const{return scores_;}void submitScore(std::string game,std::string player,uint32_t score){scores_.push_back({std::move(game),std::move(player),score});}
 void setPresentCallback(std::function<void()>cb){present_=std::move(cb);}void setLEDCallback(std::function<void(RGBState)>cb){ledCallback_=std::move(cb);}void setAudioSink(AudioSink*s){audio_=s;}
private:
 void hostCall(uint32_t);uint32_t performanceCall(uint32_t,uint32_t,uint32_t);uint32_t guestPtr(uint32_t off)const{return base_+off;}std::string guestString(uint32_t,size_t=1024)const;bool writeCString(uint32_t,const std::string&,size_t);void writeAbiTable();uint64_t nowUs()const;void pulseLED(int=69,uint8_t=180);
 void playSample(int,uint8_t,uint16_t,int8_t);void noteOn(int,int,int,uint8_t,int8_t);void playTrack(int);void advanceAudio(double);void processTrack();
 struct TileDef{std::array<uint8_t,8>bits{};uint16_t fg=0xffff,bg=0;};std::unordered_map<uint16_t,TileDef>tiles_;std::array<uint16_t,40*25>tileScreen_{};std::array<std::array<uint16_t,64*32>,2>playfield_{};std::array<uint32_t,65536>tileFlags_{};std::array<int,2>scrollX_{},scrollY_{},parallaxX_{256,256},parallaxY_{256,256};int cameraX_=0,cameraY_=0;
 void drawTile(int,int,uint16_t,bool=false);void drawPlayfield(int);uint16_t tileAt(int,int,int)const;bool solidAt(int,int,int,uint32_t)const;uint16_t actorMove(uint32_t,int,int);uint16_t actorStep(uint32_t,uint32_t,int,int,int,int);void cameraFollow(uint32_t,int,int);void drawRGBSprite(int,int,int,int,uint32_t,std::optional<uint16_t>);void drawIndexedSprite(int,int,uint32_t,uint32_t,bool);
 std::vector<Score>scores_;std::string currentPlayer_="PLAYER";int scoreGet(const std::string&,int,uint32_t);
 Cartridge cart_;bool loaded_=false,fullscreen_=false;uint32_t base_=0,input_=0,abiOffset_=0,scratchOffset_=0;std::vector<uint8_t>mem_;Rv32Cpu cpu_;Framebuffer fb_;std::function<void()>present_;std::function<void(RGBState)>ledCallback_;std::string log_;std::chrono::steady_clock::time_point started_;uint32_t rng_=0x12345678u;AudioSink*audio_=nullptr;RGBState led_{};
 uint8_t masterVolume_=220;std::array<uint8_t,8>channelVolumes_{};std::array<int8_t,8>channelPans_{};int tempo_=120;struct TrackState{int id=0,index=0,ticks=0;};std::optional<TrackState>track_;double trackElapsedMs_=0;uint64_t lastAudioUs_=0,perfStart_=0;
 PerfSnapshot perf_;int activePerfCase_=-1;
};
}
