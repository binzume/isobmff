#include "isobmff.h"
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

    int track_idx = 0; // != track_id

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
    auto stss = (BoxSTSS*)track->findByType(BOX_STSS);
    auto stsd = (BoxSTSD*)track->findByType(BOX_STSD);
    auto stsz = (BoxSTSZ*)track->findByType(BOX_STSZ);
    auto stco = (BoxSTCO*)track->findByType(BOX_STCO);
    auto stts = (BoxSTTS*)track->findByType(BOX_STTS);
    auto ctts = (BoxCTTS*)track->findByType(BOX_CTTS);
    cout << "samples: " << stsz->count() << endl;
    cout << "type: " << stsd->typeAsString() << "  config_size:" << stsd->desc().size() << endl;

    uint32_t timeScale = mdhd->timeScale()/2; // TODO

    // write init
    {
        Mp4Root m4s;
        BoxFTYP *oftyp = new BoxFTYP(0);
        m4s.add(oftyp);
        memcpy(oftyp->major, "iso5", 4);
        oftyp->minor = 512;
        oftyp->compat.push_back(0x366f7369); // iso6
        oftyp->compat.push_back(0x3134706d); // mp41

        BoxSimpleList *omoov = new BoxSimpleList(BOX_MOOV);
        omoov->add(mp4.findByType(BOX_MVHD));
        ((BoxMVHD*)mp4.findByType(BOX_MVHD))->duration = 0;
        m4s.add(omoov);

        BoxSimpleList *otrack = new BoxSimpleList(BOX_TRAK);
        tkhd->duration(0);
        tkhd->trackId(1);
        otrack->add(tkhd);
        otrack->add(track->findByType("edts"));
        omoov->add(otrack);

        BoxSimpleList *omdia = new BoxSimpleList(BOX_MDIA);
        omdia->add(mdhd);
        omdia->add(hdlr);
        hdlr->name("VideoHandler");
        otrack->add(omdia);

        BoxSimpleList *ominf = new BoxSimpleList(BOX_MINF);
        omdia->add(ominf);
        ominf->add(track->findByType("vmhd"));
        ominf->add(track->findByType("dinf"));

        auto ostbl = new BoxSimpleList(BOX_STBL);
        ominf->add(ostbl);

        ostbl->add(track->findByType("stsd"));

        ostbl->add(new BoxSTTS());
        ostbl->add(new BoxSTSC());
        ostbl->add(new BoxSTSZ());
        ostbl->add(new BoxSTCO());

        BoxSimpleList *omvex = new BoxSimpleList("mvex");
        omoov->add(omvex);
        omvex->add(new BoxTREX());

        omoov->add(mp4.findByType("udta"));

        char fname[256];
        sprintf(fname, "out/init-stream%d.m4s", track_idx);;
        m4s.write(ofstream(fname, ios::binary));
    }


    ifs.clear();
    uint32_t lastChunk = -1;
    uint32_t offset = 0;
    uint32_t samples = 0;

    // write segment
 for(int frag = 1; frag<=10; frag++)
    {
        // uint32_t frag = 1;
        uint32_t limit_sample = 0x80; // TODO
        uint32_t start_sample = samples; // limit_sample*(frag-1);


        Mp4Root m4s;
        m4s.clear();
        auto oftyp = new BoxSTYP(0);
        m4s.add(oftyp);
        memcpy(oftyp->major, "msdh", 4);
        oftyp->minor = 0;
        oftyp->compat.push_back(0x6864736d); // msdh
        oftyp->compat.push_back(0x7869736d); // msix

        auto osidx = new BoxSIDX();
        m4s.add(osidx);
        osidx->time_scale = timeScale;
        osidx->pts = stts->sampleToTime(start_sample) * timeScale / mdhd->timeScale();

        auto moof = new BoxSimpleList(BOX_MOOF);
        m4s.add(moof);

        auto mfhd = new BoxMFHD();
        mfhd->fragments = frag;
        moof->add(mfhd);

        auto traf = new BoxSimpleList(BOX_TRAF);
        moof->add(traf);

        const int SAMPLE_FLAGS_NO_SYNC = 0x01010000;
        const int SAMPLE_FLAGS_SYNC = 0x02000000;

        auto tfhd = new BoxTFHD();
        tfhd->flags |= BoxTFHD::FLAG_DEFAULT_SIZE | BoxTFHD::FLAG_DEFAULT_FLAGS; // ffmpeg compat
        tfhd->default_duration = 1001;
        tfhd->default_size = stsz->size(start_sample);
        tfhd->default_flags = SAMPLE_FLAGS_NO_SYNC;
        traf->add(tfhd);

        auto tfdt = new BoxTFDT();
        tfdt->flag_start = osidx->pts; // TODO
        traf->add(tfdt);

        auto trun = new BoxTRUN();
        trun->flags = BoxTRUN::FLAG_SAMPLE_SIZE | BoxTRUN::FLAG_SAMPLE_FLAGS
            | BoxTRUN::FLAG_SAMPLE_CTS | BoxTRUN::FLAG_DATA_OFFSET;

        auto mdat = new UnknownBox(BOX_MDAT, 8); // TODO
        m4s.add(mdat);

        uint32_t datasize = 0;

        for (int i=samples; i<stsz->count() && i < start_sample; i++) {
            auto chunk = stsc->sampleToChunk(i);
            if (lastChunk != chunk) {
                lastChunk = chunk;
                offset = 0;
            }
            samples++;
            offset += stsz->size(i); // update offset in chunk.
        }

        for (int i=start_sample; i<stsz->count(); i++) {
            cout << " " << i << endl;
            if (i >= start_sample+limit_sample && stss->include(i+1)) break;

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

            trun->add(stsz->size(i));
            trun->add(stss->include(i+1) ? SAMPLE_FLAGS_SYNC : SAMPLE_FLAGS_NO_SYNC );
            trun->add(timeOffset * timeScale / mdhd->timeScale());

            mdat->buf.resize(datasize + stsz->size(i));
            ifs.seekg(stco->offset(chunk) + offset);
            ifs.read((char*)&mdat->buf[datasize], stsz->size(i));
            datasize += stsz->size(i);
            samples++;

            offset += stsz->size(i); // update offset in chunk.
        }
        traf->add(trun);
        uint32_t duration = stts->sampleToTime(samples) * timeScale / mdhd->timeScale();

        moof->calcSize();
        osidx->add(moof->size + mdat->calcSize(), duration, 1<<31); // TODO
        trun->data_offset = moof->size + 8; // pos(mdat.data) - pos(moof)


        char fname[256];
        sprintf(fname, "out/chunk-stream%d-%05d.m4s", track_idx, frag);;
        m4s.write(ofstream(fname, ios::binary));
    }


    return 0;
}
