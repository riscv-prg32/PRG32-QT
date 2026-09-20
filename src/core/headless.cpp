#include "Cartridge.h"
#include "Runtime.h"
#include <fstream>
#include <iostream>
#include <iterator>
int main(int argc,char**argv){if(argc<2){std::cerr<<"usage: prg32qt-headless file.prg32 [frames]\n";return 2;}std::ifstream f(argv[1],std::ios::binary);std::vector<uint8_t>b((std::istreambuf_iterator<char>(f)),{});std::string e;auto c=prg32::Cartridge::parse(b,e);if(!c){std::cerr<<e<<"\n";return 3;}prg32::Runtime r;if(!r.load(*c,e)||!r.init(e)){std::cerr<<e<<"\n";return 4;}int frames=argc>2?std::stoi(argv[2]):300;for(int i=0;i<frames;i++)if(!r.frame(0,e)){std::cerr<<"frame "<<i<<": "<<e<<"\n";return 5;}std::cout<<c->name()<<": OK ("<<frames<<" frames)\n";return 0;}
