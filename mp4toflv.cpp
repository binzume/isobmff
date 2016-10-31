#include "isobmff.h"
#include "flv.h"
#include <iostream>
#include <fstream>

using namespace std;
using namespace isobmff;

int main() {

    ifstream ifs("test.mp4", ios::binary); // AVC+AAC mp4

    Mp4Root mp4;
    mp4.parse(ifs);
    cout << mp4;

    // get tracks
    vector<Box*> tracks;
    mp4.findAllByType(tracks, BOX_TRAK);

    // first track.  maybe video.
    auto track = tracks[0];

    auto tkhd = (BoxTKHD*)track->findByType(BOX_TKHD);
    auto mdhd = (BoxMDHD*)track->findByType(BOX_MDHD);
    auto hdlr = (BoxHDLR*)track->findByType(BOX_HDLR);
    cout << "resoluion: " << tkhd->width()/65536 << "x" <<  tkhd->width()/65536 << endl;
    cout << "duration: " << mdhd->duration() / mdhd->timeScale()
         << "sec. (" << mdhd->duration() << "/" <<  mdhd->timeScale() << endl;
    cout << "type:" << hdlr->typeAsString() << " (" << hdlr->name() << ")" << endl;

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

    flv::FLVHeader fh = {{'F','L','V'}, 1, flv::TYPE_FLAG_VIDEO | flv::TYPE_FLAG_AUDIO, 9}; // 9: sizeof(flv::FLVHeader)
    of << fh;
    flv::write32(of, 0);

    flv::FLVTagHeader th;
    th.type = flv::TAG_TYPE_VIDEO;
    th.stream_id = 0;
    th.timestamp = 0;
    uint8_t codecId = flv::VCODEC_AVC;
    if (stsd->typeAsString() == "mp4a") {
        th.type = flv::TAG_TYPE_AUDIO;
        codecId = flv::ACODEC_AAC; // TODO esds box.
    }

    // write header
    string desc = stsd->desc();
    if (codecId == flv::VCODEC_AVC) {
        int p = desc.find("avcC");
        string config = desc.substr(p + 4);
        flv::write_video(of, th, config, codecId, 0, true, true);

        flv::write32(of, (uint32_t)of.tellp() - prev);
        prev = of.tellp();
    } else if (codecId == flv::ACODEC_AAC) {
        int p = desc.find("esds");
        string config = desc.substr(p+30, desc[p+29]); // TODO esds box.
        flv::write_audio(of, th, config, flv::audio_format(codecId, 2, flv::SOUND_RATE_44K), true);

        flv::write32(of, (uint32_t)of.tellp() - prev);
        prev = of.tellp();
    }

    vector<uint8_t> buf;
    ifs.clear();
    for (int i=0; i<stsz->count(); i++) {
        cout << " " << i << endl;

        // read sample
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
        ifs.seekg(stco->offset(chunk) + offset);
        ifs.read((char*)&buf[0], buf.size());
        offset += stsz->size(i); // update offset in chunk.

        // check idr
        bool rap = false;
        if (codecId == flv::VCODEC_AVC) {
            uint32_t p = 0;
            while (p+5 < buf.size()) {
                uint32_t sz = (buf[p] << 24) | (buf[p+1] << 16) | (buf[p+2] << 8) | buf[p+3];
                cout << "  NAL" << sz << " typ" <<  (buf[p+4]&0x1f) <<  endl;
                if ((buf[p+4]&0x1f) == 5) rap = true;
                p+= sz + 4;
            }
        }

        // write flv tag.
        th.timestamp = stts->sampleToTime(i) * 1000 / mdhd->timeScale();
        if (th.type == flv::TAG_TYPE_VIDEO) {
            flv::write_video(of, th, buf, codecId, timeOffset * 1000 / mdhd->timeScale(), rap);
        } else {
            flv::write_audio(of, th, buf, flv::audio_format(codecId, 2, flv::SOUND_RATE_44K));
        }

        flv::write32(of, (uint32_t)of.tellp() - prev);
        prev = of.tellp();
    }
    return 0;
}
