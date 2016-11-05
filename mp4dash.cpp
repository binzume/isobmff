#include "isobmff.h"
#include <iostream>
#include <fstream>

using namespace std;
using namespace isobmff;

struct Sample {
    uint64_t timestamp;
    uint32_t time_scale;
    uint32_t time_offset;
    bool has_time_offset;
    bool sync_point;
    std::vector<uint8_t> data;
};

class Mp4SampleReader {
    Box *track;
    BoxSTSC *stsc;
    BoxSTSD *stsd;
    BoxSTSS *stss;
    BoxSTSZ *stsz;
    BoxSTCO *stco;
    BoxSTTS *stts;
    BoxCTTS *ctts;
    uint32_t pos;
    uint32_t read_offset;
    uint32_t current_chunk;
    uint32_t time_scale;
public:
    Mp4SampleReader(isobmff::Box *track) : track(track) {
        stsc = (BoxSTSC*)track->findByType(BOX_STSC);
        stss = (BoxSTSS*)track->findByType(BOX_STSS);
        stsd = (BoxSTSD*)track->findByType(BOX_STSD);
        stsz = (BoxSTSZ*)track->findByType(BOX_STSZ);
        stco = (BoxSTCO*)track->findByType(BOX_STCO);
        stts = (BoxSTTS*)track->findByType(BOX_STTS);
        ctts = (BoxCTTS*)track->findByType(BOX_CTTS);
        auto mdhd = (BoxMDHD*)track->findByType(BOX_MDHD);
        time_scale = mdhd->time_scale;
        pos = 0;
        read_offset = 0;
        current_chunk = -1;
    }
    bool eos() { return pos >= stsz->count(); }
    bool syncPoint() { return (stss == nullptr) || stss->include(pos + 1);}
    uint32_t timeScale() { return  time_scale;}
    uint32_t position() { return pos; }
    void seek(uint32_t sample) {
        current_chunk = stsc->sampleToChunk(sample);
        pos = sample;
        read_offset = 0;
        while (sample > 0) {
            sample--;
            if (stsc->sampleToChunk(sample) != current_chunk) break;
            read_offset += stsz->size(sample);
        }
    }
    Sample read(std::istream &is) {
        auto chunk = stsc->sampleToChunk(pos);
        if (current_chunk != chunk) {
            current_chunk = chunk;
            read_offset = 0;
        }
        Sample s;
        s.timestamp = stts->sampleToTime(pos);
        s.time_scale = time_scale;
        s.time_offset = 0;
        s.has_time_offset = false;
        s.sync_point = syncPoint();
        if (ctts != nullptr) {
            s.time_offset = ctts->sampleToOffset(pos);
            s.has_time_offset = true;
        }

        // read
        s.data.resize(stsz->size(pos));
        is.seekg(stco->offset(chunk) + read_offset);
        is.read((char*)&s.data[0], s.data.size());

        read_offset += stsz->size(pos);
        pos ++;
        return s;
    }
};


int convert(istream &ifs, Box *track, int track_idx) {

    auto mdhd = (BoxMDHD*)track->findByType(BOX_MDHD);
    cout << "duration: " << mdhd->duration / mdhd->time_scale
         << "sec. (" << mdhd->duration << "/" <<  mdhd->time_scale << endl;

    auto tkhd = (BoxTKHD*)track->findByType(BOX_TKHD);
    auto hdlr = (BoxHDLR*)track->findByType(BOX_HDLR);
    auto stsd = (BoxSTSD*)track->findByType(BOX_STSD);
    cout << "resoluion: " << tkhd->width/65536 << "x" <<  tkhd->width/65536 << endl;
    cout << "type:" << hdlr->typeAsString() << " (" << hdlr->name() << ")" << endl;

    cout << "type: " << stsd->typeAsString() << "  config_size:" << stsd->desc().size() << endl;

    ifs.clear();
    Mp4SampleReader reader(track);

    uint32_t timeScale = reader.timeScale();

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
        m4s.add(omoov);

        BoxMVHD *omvhd = new BoxMVHD();
        omoov->add(omvhd);
        omvhd->init();
        omvhd->duration = 0;
        omvhd->timeScale = timeScale;

        BoxSimpleList *otrack = new BoxSimpleList(BOX_TRAK);
        BoxTKHD *otkhd = new BoxTKHD();
        otkhd->init();
        otkhd->volume = tkhd->volume;
        otkhd->width = tkhd->width;
        otkhd->height = tkhd->height;
        otrack->add(otkhd);
        // otrack->add(track->findByType("edts")); // TODO
        omoov->add(otrack);

        BoxSimpleList *omdia = new BoxSimpleList(BOX_MDIA);
        BoxMDHD *omdhd = new BoxMDHD();
        omdhd->time_scale = timeScale;
        omdia->add(omdhd);
        omdia->add(hdlr); // TODO
        otrack->add(omdia);

        BoxSimpleList *ominf = new BoxSimpleList(BOX_MINF);
        omdia->add(ominf);
        //if (track->findByType("vmhd") != nullptr) {
        //    ominf->add(track->findByType("vmhd"));
        //}
        //ominf->add(track->findByType("dinf"));

        auto ostbl = new BoxSimpleList(BOX_STBL);
        ominf->add(ostbl);

        ostbl->add(stsd);

        ostbl->add(new BoxSTTS());
        ostbl->add(new BoxSTSC());
        ostbl->add(new BoxSTSZ());
        ostbl->add(new BoxSTCO());

        BoxSimpleList *omvex = new BoxSimpleList("mvex");
        omoov->add(omvex);
        omvex->add(new BoxTREX());

        char fname[256];
        sprintf(fname, "dash/init-stream%d.m4s", track_idx);
        m4s.write(ofstream(fname, ios::binary));
    }

    // write segments
    uint32_t seg_duration = 5 * reader.timeScale();
    uint64_t last_timestamp = 0;
    for(int frag = 1; !reader.eos(); frag++) {

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
        osidx->pts = last_timestamp * timeScale / reader.timeScale();

        auto moof = new BoxSimpleList(BOX_MOOF);
        m4s.add(moof);

        auto mfhd = new BoxMFHD();
        mfhd->fragments = frag;
        moof->add(mfhd);

        auto traf = new BoxSimpleList(BOX_TRAF);
        moof->add(traf);

        auto tfhd = new BoxTFHD();
        tfhd->flags |= BoxTFHD::FLAG_DEFAULT_SIZE | BoxTFHD::FLAG_DEFAULT_FLAGS; // ffmpeg compat
        tfhd->default_size = 0;
        tfhd->default_flags = SAMPLE_FLAGS_NO_SYNC;
        traf->add(tfhd);

        auto tfdt = new BoxTFDT();
        tfdt->flag_start = osidx->pts;
        traf->add(tfdt);

        auto trun = new BoxTRUN();
        trun->flags = BoxTRUN::FLAG_SAMPLE_SIZE | BoxTRUN::FLAG_SAMPLE_FLAGS
            | BoxTRUN::FLAG_SAMPLE_CTS | BoxTRUN::FLAG_DATA_OFFSET;
        traf->add(trun);

        auto mdat = new UnknownBox(BOX_MDAT, 8); // TODO
        m4s.add(mdat);

        int64_t start_timestamp = -1;
        uint32_t samples = 0;
        while (!reader.eos()) {
            Sample sample = reader.read(ifs);
            if (false) {
                cout << "  size: " << sample.data.size() << endl;
                cout << "  timestamp: " << sample.timestamp << endl;
                if (sample.has_time_offset) {
                    cout << "  time offset: " << sample.time_offset << endl;
                }
            }

            trun->add(sample.data.size());
            trun->add(sample.sync_point ? SAMPLE_FLAGS_SYNC : SAMPLE_FLAGS_NO_SYNC );
            trun->add(sample.time_offset * timeScale / sample.time_scale);
            mdat->buf.insert(mdat->buf.end(), sample.data.begin(), sample.data.end());

            if (start_timestamp < 0) start_timestamp = sample.timestamp;
            last_timestamp = sample.timestamp;
            samples++;
            if (samples > 1 && sample.timestamp > seg_duration * frag && reader.syncPoint()) break;
        }
        last_timestamp += (last_timestamp - start_timestamp) / (samples - 1);
        tfhd->default_duration = (last_timestamp - start_timestamp) / (samples - 1);

        moof->calcSize();
        osidx->add(moof->size + mdat->calcSize(), last_timestamp, 1<<31); // (1<<31) = start with SAP
        trun->data_offset = moof->size + 8; // pos(mdat.data) - pos(moof)

        char fname[256];
        sprintf(fname, "dash/chunk-stream%d-%05d.m4s", track_idx, frag);;
        m4s.write(ofstream(fname, ios::binary));
        cout << "output:" << fname  <<  " t:" << last_timestamp << endl;
    }

    return 0;
}


int main() {

    ifstream ifs("test2.mp4", ios::binary); // AVC+AAC mp4

    Mp4Root mp4;
    mp4.parse(ifs);
    cout << mp4;

    // get tracks
    vector<Box*> tracks;
    mp4.findAllByType(tracks, BOX_TRAK);

    int track_idx = 0; // != track_id
    for (auto track : tracks) {
        convert(ifs, track, track_idx);
        track_idx ++;
    }

    return 0;
}
