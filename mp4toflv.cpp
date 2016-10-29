#include "isobmff.h"
#include "flv.h"
#include <iostream>
#include <fstream>

using namespace std;
using namespace isobmff;

int main() {

    ifstream f("test.mp4", ios::binary);

    Mp4Root mp4;
    mp4.parse(f);
    cout << mp4;

    cout << *mp4.findByType(BOX_MVHD) << endl;


    // first track.  maybe video.
    auto track = mp4.findByType(BOX_TRAK);

    auto tkhd = (BoxTKHD*)track->findByType(BOX_TKHD);
    cout << "resoluion: " << tkhd->width()/65536 << "x" <<  tkhd->width()/65536 << endl;

    auto mdhd = (BoxMDHD*)track->findByType(BOX_MDHD);
    cout << "duration: " << mdhd->duration() / mdhd->timeScale() << "sec. (" << mdhd->duration() << "/" <<  mdhd->timeScale() << endl;

    auto stsc = (BoxSTSC*)track->findByType(BOX_STSC);
    auto stsd = (BoxSTSD*)track->findByType(BOX_STSD);
    auto stsz = (BoxSTSZ*)track->findByType(BOX_STSZ);
    auto stco = (BoxSTCO*)track->findByType(BOX_STCO);
    auto stts = (BoxSTTS*)track->findByType(BOX_STTS);
    auto ctts = (BoxCTTS*)track->findByType(BOX_CTTS);
    cout << "samples: " << stsz->count() << endl;
    cout << "type: " << stsd->typeAsString() << "  config_size:" << stsd->desc().size() << endl;
    uint32_t lastChunk = -1;
    uint32_t offset = 0;

    ofstream of("out.flv", ios::binary);
    size_t prev = 0;

    flv::FLVHeader fh = {{'F','L','V'}, 1, 0x05, 9}; // 9: sizeof(flv::FLVHeader)
    of << fh;
    write32(of, 0);

    string desc = stsd->desc();
    int p = desc.find("avcC");
    string config = desc.substr(p + 4);

    flv::FLVTagHeader th;
    th.type = flv::TAG_TYPE_VIDEO;
    th.stream_id = 0;
    th.timestamp = 0;
    th.size = config.size() + 5;
    of << th;
    write8(of, 0x17); // AVC
    write8(of, 0x00); // AVC Header
    write24(of, 0); // timeoffset
    of << config;
    write32(of, (uint32_t)of.tellp() - prev);
    prev = of.tellp();
    vector<uint8_t> buf;

    f.clear();
    for (int i=0; i<stsz->count(); i++) {
        cout << " " << i << endl;
        auto chunk = stsc->sampleToChunk(i);
        if (lastChunk != chunk) {
            lastChunk = chunk;
            offset = 0;
        }
        cout << "  size:" << stsz->size(i) << endl;
        cout << "  chunk:" << chunk << endl;
        cout << "  offset: " << stco->offset(chunk) + offset << endl;
        cout << "  timestamp: " << stts->sampleToTime(i) << endl;
        uint32_t timeOffset = 0;
        if (ctts != nullptr) {
            timeOffset = ctts->sampleToOffset(i);
            cout << "  time offset: " << timeOffset << endl;
        }

        buf.resize(stsz->size(i));
        f.seekg(stco->offset(chunk) + offset);
        f.read((char*)&buf[0], buf.size());
        uint32_t p = 0;
        bool rap = false;
        while (p+5 < buf.size()) {
            uint32_t sz = (buf[p] << 24) | (buf[p+1] << 16) | (buf[p+2] << 8) | buf[p+3];
            cout << "  NAL" << sz << " typ" <<  (buf[p+4]&0x1f) <<  endl;
            if ((buf[p+4]&0x1f) == 5) rap = true;
            p+= sz + 4;
        }

        // write flv tag.
        th.timestamp = stts->sampleToTime(i) * 1000 / mdhd->timeScale();
        th.size = buf.size() + 2 + 3;
        of << th;
        write8(of, rap? 0x17 : 0x07); // AVC
        write8(of, 0x01); // AVC NAL
        write24(of, timeOffset * 1000 / mdhd->timeScale()); // TODO

        of.write((char*)&buf[0], buf.size());

        write32(of, (uint32_t)of.tellp() - prev);
        prev = of.tellp();

        // update file offset.
        offset += stsz->size(i);
    }


    return 0;
}
