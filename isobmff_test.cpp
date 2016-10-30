#include "isobmff.h"
#include <iostream>
#include <fstream>
#include <cassert>

using namespace std;
using namespace isobmff;

int main() {

    ifstream ifs("test.mp4", ios::binary);

    Mp4Root mp4;
    mp4.parse(ifs);
    cout << mp4;

    cout << *mp4.findByType(BOX_MVHD) << endl;
    auto mvhd = (BoxMVHD*)mp4.findByType(BOX_MVHD);
    assert(mvhd != nullptr);
    cout << "duration: " << (double)mvhd->duration / mvhd->timeScale << "sec. (" << mvhd->duration << "/" <<  mvhd->timeScale << endl;

    // get tracks
    vector<Box*> tracks;
    mp4.findAllByType(tracks, BOX_TRAK);
    for (auto track : tracks) {

        auto tkhd = (BoxTKHD*)track->findByType(BOX_TKHD);
        assert(tkhd != nullptr);
        cout << "track:" << tkhd->trackId() << endl;
        cout << "  resoluion(video): " << tkhd->width()/65536 << "x" <<  tkhd->height()/65536 << endl;
        cout << "  volume(audio): " << tkhd->volume() << endl;

        auto mdhd = (BoxMDHD*)track->findByType(BOX_MDHD);
        assert(tkhd != nullptr);
        cout << "  duration: " << (double)mdhd->duration() / mdhd->timeScale() << "sec. (" << mdhd->duration() << "/" <<  mdhd->timeScale() << endl;
    }

    // add 'free' box.
    BoxFREE *fbox = new BoxFREE(32);
    strcpy((char*)&fbox->body[0], "Hello!");
    mp4.children.push_back(fbox);

    ofstream ofs("out.mp4", ios::binary);
    mp4.write(ofs); // skip mdat, if large.

    return 0;
}
