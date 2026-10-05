#include "spacetrace/NativePackage.h"
#include "spacetrace/RealtimeFIRRenderer.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <vector>

using namespace spacetrace;

static void writeWavFloat32(const std::string& path, const std::vector<float>& l,
                            const std::vector<float>& r, uint32_t sr) {
    std::ofstream f(path, std::ios::binary);
    uint32_t dataBytes = static_cast<uint32_t>(l.size()*2*sizeof(float));
    uint32_t riffSize = 36 + dataBytes;
    uint16_t fmt=3, ch=2, bits=32, block=8; uint32_t byteRate=sr*block;
    f.write("RIFF",4); f.write(reinterpret_cast<char*>(&riffSize),4); f.write("WAVE",4);
    f.write("fmt ",4); uint32_t fmtSize=16; f.write(reinterpret_cast<char*>(&fmtSize),4);
    f.write(reinterpret_cast<char*>(&fmt),2); f.write(reinterpret_cast<char*>(&ch),2);
    f.write(reinterpret_cast<char*>(&sr),4); f.write(reinterpret_cast<char*>(&byteRate),4);
    f.write(reinterpret_cast<char*>(&block),2); f.write(reinterpret_cast<char*>(&bits),2);
    f.write("data",4); f.write(reinterpret_cast<char*>(&dataBytes),4);
    for (size_t i=0;i<l.size();++i){f.write(reinterpret_cast<const char*>(&l[i]),4);f.write(reinterpret_cast<const char*>(&r[i]),4);}
}

static float pinkSample(std::mt19937& rng) {
    static std::normal_distribution<float> nd(0.0f, 1.0f);
    static double b0=0,b1=0,b2=0,b3=0,b4=0,b5=0,b6=0;
    double w=nd(rng);
    b0 = 0.99886*b0 + w*0.0555179;
    b1 = 0.99332*b1 + w*0.0750759;
    b2 = 0.96900*b2 + w*0.1538520;
    b3 = 0.86650*b3 + w*0.3104856;
    b4 = 0.55000*b4 + w*0.5329522;
    b5 = -0.7616*b5 - w*0.0168980;
    double pink = b0+b1+b2+b3+b4+b5+b6+w*0.5362;
    b6 = w*0.115926;
    return static_cast<float>(pink*0.05);
}

int main(int argc,char**argv){
    if(argc<3){std::cerr<<"usage: sweep_measure dataset.sthrtf out.wav\n"; return 2;}
    auto pr=NativePackage::loadFile(argv[1]); if(!pr){std::cerr<<pr.error<<"\n";return 1;}
    auto& set=pr.package->hrtf;
    const uint32_t sr=44100; const size_t total=sr*20u, block=64;
    size_t maxIR=2; for(auto&m:set.measurements()) maxIR=std::max(maxIR,std::max(m.left.size(),m.right.size())+64u);
    RealtimeFIRRenderer rr; rr.prepare(maxIR,sr,20.0); rr.setHRTF(&set);
    std::vector<float>L(total),R(total),in(block),ol(block),orr(block);
    std::mt19937 rng(0x53504143u);
    for(size_t off=0;off<total;off+=block){
        size_t n=std::min(block,total-off);
        for(size_t i=0;i<n;++i) in[i]=pinkSample(rng);
        double t=(double)off/(double)(total-1);
        rr.setPosition({360.0*t,0.0,1.0});
        rr.process(in.data(),ol.data(),orr.data(),n);
        std::copy_n(ol.data(),n,L.data()+off); std::copy_n(orr.data(),n,R.data()+off);
    }
    writeWavFloat32(argv[2],L,R,sr);
    std::cerr<<"rendered "<<set.metadata().name<<" measurements="<<set.measurements().size()<<"\n";
    return 0;
}
