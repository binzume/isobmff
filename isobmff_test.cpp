#define BOX_READ_SIZE_LIMIT 1024*1024*100
#include <iostream>
#include "isobmff.h"
#include <fstream>
#include <cassert>

using namespace std;
using namespace isobmff;

int main() {

    ifstream ifs("test.mp4", ios::binary);

    Mp4Root mp4;
    mp4.parse(ifs);
    cout << mp4;

    auto mvhd = (BoxMVHD*)mp4.findByType(BOX_MVHD);
    assert(mvhd != nullptr);
    cout << *mvhd << endl;
    cout << "duration: " << (double)mvhd->duration / mvhd->timeScale << "sec. (" << mvhd->duration << "/" <<  mvhd->timeScale << endl;

    // get tracks
    vector<Box*> tracks;
    mp4.findAllByType(tracks, BOX_TRAK);
    for (auto track : tracks) {

        auto tkhd = (BoxTKHD*)track->findByType(BOX_TKHD);
        assert(tkhd != nullptr);
        cout << "track:" << tkhd->track_id << endl;
        cout << "  resoluion(video): " << tkhd->width/65536 << "x" <<  tkhd->height/65536 << endl;
        cout << "  volume(audio): " << tkhd->volume << endl;

        auto mdhd = (BoxMDHD*)track->findByType(BOX_MDHD);
        assert(tkhd != nullptr);
        cout << "  duration: " << (double)mdhd->duration / mdhd->time_scale << "sec. (" << mdhd->duration << "/" <<  mdhd->time_scale << endl;
    }

    // add 'free' box.
    BoxFREE *fbox = new BoxFREE(32);
    strcpy((char*)&fbox->body[0], "Hello!");
    mp4.children.push_back(fbox);

    // TODO: if mdat moved.
    //vector<BoxSTCO*> stcos;
    //for (auto &stco : mp4.findAllByType(stcos, BOX_STCO)) {
    //    stco->moveAll(fbox->size);
    //}

    ofstream ofs("out.mp4", ios::binary);
    mp4.write(ofs); // skip mdat, if large.

    return 0;
}
