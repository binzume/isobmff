#include "isobmff.h"
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

    // add 'free' box.
    BoxFREE *fbox = new BoxFREE(32);
    strcpy((char*)&fbox->body[0], "Hello!");
    mp4.children.push_back(fbox);

    ofstream of("out.mp4", ios::binary);
    mp4.write(of); // skip mdat, if large.

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
    cout << "type: " << stsd->typeAsString() << endl;
    uint32_t lastChunk = -1;
    uint32_t offset = 0;
    for (int i=0; i<stsz->count() && i<10; i++) {
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
        if (ctts != nullptr) {
            cout << "  time offset: " << ctts->sampleToOffset(i) << endl;
        }
        offset += stsz->size(i);
    }

    return 0;
}
