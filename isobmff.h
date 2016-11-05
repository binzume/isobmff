// ISO base media file format (isobmff)

#ifndef FLV_ISOBMFF_
#define FLV_ISOBMFF_

#ifndef BOX_READ_SIZE_LIMIT
# define BOX_READ_SIZE_LIMIT (1024 * 1024 * 10) // skip large box.
#endif

#include <vector>
#include <istream>
#include <ostream>
#include <string>
#include <stdint.h>

namespace isobmff {

static inline uint32_t read32(std::istream &is) {
    uint8_t buf[4];
    is.read((char*)&buf[0],sizeof(buf));
    return (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
}

static inline uint32_t read24(std::istream &is) {
    uint8_t buf[3];
    is.read((char*)&buf[0],sizeof(buf));
    return (buf[0] << 16) | (buf[1] << 8) | buf[2];
}
static inline uint16_t read16(std::istream &is) {
    uint8_t buf[2];
    is.read((char*)&buf[0],sizeof(buf));
    return (buf[1] << 8) | buf[0];
}
static inline uint8_t read8(std::istream &is) {
    uint8_t buf[1];
    is.read((char*)&buf[0],sizeof(buf));
    return buf[0];
}
static inline uint64_t read64(std::istream &is) {
    uint64_t h = read32(is);
    return (h << 32) | read32(is);
}

static inline void write8(std::ostream &is, uint8_t d) {
    char buf[] = {d};
    is.write(buf, sizeof(buf));
}
static inline void write16(std::ostream &is, uint16_t d) {
    char buf[] = {d >> 8, d};
    is.write(buf, sizeof(buf));
}
static inline void write24(std::ostream &is, uint32_t d) {
    char buf[] = {d >> 16, d >> 8, d};
    is.write(buf, sizeof(buf));
}
static inline void write32(std::ostream &is, uint32_t d) {
    char buf[] = {d >> 24, d >> 16, d >> 8, d};
    is.write(buf, sizeof(buf));
}
static inline void write64(std::ostream &os, uint64_t d) {
    write32(os, d >> 32);
    write32(os, d);
}


class Box{
public:
    size_t size;
    char type[5];
    std::vector<Box*> children;
    int ref_count;

    Box(const char boxtype[4], size_t sz){
        memcpy(type, boxtype, 4);
        type[4] = '\0';
        size = sz;
        ref_count = 1;
    }
    virtual bool is_full_box() const {return false;}

    Box* findByType(const char n[4]) {
        if (memcmp(type, n, 4)==0) return this;
        for (int i=0; i<children.size(); i++) {
            Box *b = children[i]->findByType(n);
            if (b != nullptr) return b;
        }
        return nullptr;
    }

    template<typename T>
    std::vector<T*>& findAllByType(std::vector<T*> &out, const char n[4]) {
        if (memcmp(type, n, 4)==0)
            out.push_back((T*)this);
        for (int i=0; i<children.size(); i++)
            children[i]->findAllByType(out, n);
        return out;
    }

    void dump(std::ostream &os, const std::string &prefix) const {
        os << prefix << type;
        os << " size: " << size << std::endl;
        dump_attr(os, prefix);
        for (int i=0; i<children.size(); i++) {
            children[i]->dump(os, prefix + ". ");
        }
    }
    virtual void dump_attr(std::ostream &os, const std::string &prefix) const {}

    virtual void parse(std::istream &is) {}

    virtual size_t calcSize() {return size;}

    virtual void write(std::ostream &os) const {
        write32(os, size);
        os.write(type, 4);
        for (int i=0; i<children.size(); i++) {
            children[i]->write(os);
        }
    }

    virtual ~Box() {
        for (auto &b :children) {
            b->ref_count--;
            if (b->ref_count <= 0) delete b;
        }
    }
};

class FullBox : public Box {
public:
    FullBox(const char boxtype[4], size_t sz) : Box(boxtype, sz), version(0), flags(0) {};
    uint8_t version;
    uint32_t flags;
    bool is_full_box() const {return true;}

    void parse(std::istream &is) {
        version = read8(is);
        flags = read24(is);
    }
    virtual void write(std::ostream &os) const {
        Box::write(os);
        write8(os, version);
        write24(os, flags);
    }
    virtual void dump_attr(std::ostream &os, const std::string &prefix) const {
        os << prefix << " v" << version << " flags:" << flags << std::endl;
    }

protected:
    static const int HEADER_SIZE = 12;
};


class FullBufBox : public FullBox{
protected:
    std::vector<uint8_t> buf;
    uint8_t ui8(int pos) const {
        return buf[pos];
    }
    uint16_t ui16(int pos) const {
        return (buf[pos] << 8)|buf[pos+1];
    }
    uint32_t ui32(int pos) const {
        return (buf[pos] << 24)|(buf[pos+1] << 16)|(buf[pos+2] << 8)|buf[pos+3];
    }
    uint64_t ui64(int pos) const {
        uint64_t h = ui32(pos);
        return h | ui32(pos+4);
    }
    void ui16(int pos, uint16_t v) {
        buf[pos+0] = v >> 8;
        buf[pos+1] = v;
    }
    void ui32(int pos, uint32_t v) {
        buf[pos+0] = v >> 24;
        buf[pos+1] = v >> 16;
        buf[pos+2] = v >> 8;
        buf[pos+3] = v;
    }
public:
    FullBufBox(const char type[4], size_t sz) : FullBox(type, sz) {
        buf.resize(sz - HEADER_SIZE);
    }

    virtual void dump_attr(std::ostream &os, const std::string &prefix) const {
        os << prefix << " : [";
        for (int i=0; i<10 && i < buf.size(); i++) {
            os << (uint32_t)buf[i] << ",";
        }
        os << "...] " << buf.size() << std::endl;
    }

    void parse(std::istream &is) {
        FullBox::parse(is);
        is.read((char*)&buf[0], size - HEADER_SIZE);
    }
    virtual void write(std::ostream &os) const {
        FullBox::write(os);
        if (size > 8) {
            os.write((char*)&buf[0], buf.size());
        }
    }

    virtual size_t calcSize() {size = buf.size()+HEADER_SIZE; return size;}
};

class UnknownBox : public Box {
public:
    std::vector<uint8_t> buf;
    UnknownBox(const char boxtype[4], size_t sz) : Box(boxtype, sz) { buf.resize(sz-8); }
    virtual void dump_attr(std::ostream &os, const std::string &prefix) const {
        os << prefix << " unknown_body: [";
        for (int i=0; i<10 && i < buf.size(); i++) {
            os << (uint32_t)buf[i] << ",";
        }
        os << "...] " << buf.size() << std::endl;
    }

    void parse(std::istream &is) {
        is.read((char*)&buf[0], size - 8);
    }
    virtual void write(std::ostream &os) const {
        Box::write(os);
        if (buf.size() > 0) {
            os.write((char*)&buf[0], buf.size());
        }
    }

    virtual size_t calcSize() {size = buf.size()+8; return size;}
};

class UnknownBoxRef : public Box {
public:
    UnknownBoxRef(const char boxtype[4], size_t sz) : Box(boxtype, sz) {}
    long long offset;

    virtual void dump_attr(std::ostream &os, const std::string &prefix) const {
        os << prefix << " unknown_ref: " << offset << std::endl;
    }

    void parse(std::istream &is) {
        offset = is.tellg();
        size_t pos = offset;
        is.seekg(pos + size - 8, std::ios_base::beg);
    }

    virtual void write(std::ostream &os) const {
        // TODO:
    }

};


// mp4
static const char *BOX_FTYP = "ftyp";
static const char *BOX_FREE = "free";
static const char *BOX_MOOV = "moov";
static const char *BOX_MVHD = "mvhd";
static const char *BOX_MDIA = "mdia";
static const char *BOX_MDHD = "mdhd";
static const char *BOX_MINF = "minf";
static const char *BOX_MDAT = "mdat";
static const char *BOX_HDLR = "hdlr";
static const char *BOX_STCO = "stco";
static const char *BOX_STSC = "stsc";
static const char *BOX_STSD = "stsd";
static const char *BOX_STTS = "stts";
static const char *BOX_STSZ = "stsz";
static const char *BOX_STSS = "stss";
static const char *BOX_STBL = "stbl";
static const char *BOX_CTTS = "ctts";
static const char *BOX_TRAK = "trak";
static const char *BOX_TKHD = "tkhd";
static const char *BOX_DTS  = "dts\0";
static const char *BOX_UDTA = "udta";

static const char *BOX_STYP = "styp";
static const char *BOX_MOOF = "moof";
static const char *BOX_MFHD = "mfhd";
static const char *BOX_TRAF = "traf";
static const char *BOX_TFHD = "tfhd";
static const char *BOX_TFDT = "tfdt";
static const char *BOX_TRUN = "trun";
static const char *BOX_TREX = "trex";
static const char *BOX_SIDX = "sidx";
static const char *BOX_PSSH = "pssh";

static const int SAMPLE_FLAGS_NO_SYNC = 0x01010000;
static const int SAMPLE_FLAGS_SYNC = 0x02000000;

static const char* HAS_CHILD_BOX[] = {BOX_MOOV, BOX_TRAK, BOX_DTS, BOX_MDIA, BOX_MINF, BOX_STBL, BOX_UDTA, BOX_MOOF, BOX_TRAF, "edts"};

bool has_child(const char type[4]){
    for (auto b : HAS_CHILD_BOX) {
        if (memcmp(type, b, 4) == 0) return true;
    }
    return false;
}

class BoxFTYP : public Box{
public:
    BoxFTYP(size_t sz) : Box(BOX_FTYP, sz) {
    }
    char major[4];
    uint32_t minor;
    std::vector<uint32_t> compat;

    virtual void dump_attr(std::ostream &os, const std::string &prefix) const {
        os << prefix << " major: " << major << std::endl;
        os << prefix << " minor: " << minor << std::endl;
    }

    void parse(std::istream &is) {
        is.read(major, 4);
        minor = read32(is);
        for (int i=0; i<(size-16)/4; i++) {
            uint32_t b;
            is.read((char*)&b, 4);
            compat.push_back(b);
        }
    }

    virtual size_t calcSize() {size = compat.size()*4 +16; return size;}

    virtual void write(std::ostream &os) const {
        Box::write(os);
        os.write(major, 4);
        write32(os, minor);
        os.write((char*)&compat[0], compat.size()*4);
    }
};

class BoxFREE : public Box{
public:
    BoxFREE(size_t sz) : Box(BOX_FREE, sz) {
        body.resize(sz - 8);
    }
    std::vector<uint8_t> body;

    virtual void dump_attr(std::ostream &os, const std::string &prefix) const {
        os << prefix << " body: [";
        for (int i=0; i<10 && i < body.size(); i++) {
            os << (uint32_t)body[i] << ",";
        }
        os << "...] " << body.size() << std::endl;
    }

    void parse(std::istream &is) {
        is.read((char*)&body[0], size - 8);
    }
    virtual void write(std::ostream &os) const {
        Box::write(os);
        if (size > 8) {
            os.write((char*)&body[0], size - 8);
        }
    }
};


class BoxMVHD : public FullBox{
public:
    BoxMVHD(size_t sz = HEADER_SIZE + 24 * 4) : FullBox(BOX_MVHD, sz) {}
    uint32_t created;
    uint32_t modified;
    uint32_t timeScale;
    uint64_t duration;
    uint32_t rate;
    uint32_t volume;
    uint32_t matrix[9];
    uint32_t next_track_id;

    void init() {
        created = 0;
        modified = 0;
        rate = 0x10000;
        volume = 0x1000000;
        next_track_id = 3;
        for (int i=0; i<9; i++) matrix[i] = 0;
        matrix[0] = 0x10000;
        matrix[4] = 0x10000;
        matrix[8] = 0x40000000;
    }

    void parse(std::istream &is) {
        FullBox::parse(is);
        if (version != 0) return ; // TODO: error.
        created = read32(is);
        modified = read32(is);
        timeScale = read32(is);
        duration = read32(is);
        rate = read32(is);
        volume = read32(is);
        read32(is);
        read32(is);
        for (auto &d : matrix) {
            d = read32(is);
        }
        for (int i=0; i<6; i++) {read32(is);}
        next_track_id = read32(is);
    }
    virtual void write(std::ostream &os) const {
        FullBox::write(os);
        write32(os, created);
        write32(os, modified);
        write32(os, timeScale);
        write32(os, duration);
        write32(os, rate);
        write32(os, volume);
        write32(os, 0);
        write32(os, 0);
        for (auto &d : matrix) {
            write32(os, d);
        }
        for (int i=0; i<6; i++) {write32(os, 0);}
        write32(os, next_track_id);
    }

    void dump_attr(std::ostream &os, const std::string &prefix) const {
        FullBox::dump_attr(os,prefix);
        os << prefix << " created: " << created << std::endl;
        os << prefix << " modified: " << modified << std::endl;
        os << prefix << " duration: " << duration << "/" << timeScale << std::endl;
        os << prefix << " rate: " << rate << std::endl;
        os << prefix << " volume: " << volume << std::endl;
        os << prefix << " next_track: " << next_track_id << std::endl;
        os << prefix << " matrix: [";
        for (auto &d : matrix) {
            os << d << ",";
        }
        os << std::endl;
    }
};

class BoxMDHD : public FullBox{
public:
    BoxMDHD(size_t sz) : FullBox(BOX_MDHD, sz) {}
    BoxMDHD() : FullBox(BOX_MDHD, 0) {init();}

    uint64_t created;
    uint64_t modified;
    uint32_t time_scale;
    uint64_t duration;
    uint16_t lang;

    void init() {
        created = 0;
        modified = 0;
        time_scale = 1;
        duration = 0;
        lang = 0x55c4; // 'und'
        calcSize();
    }

    void parse(std::istream &is) {
        FullBox::parse(is);
        if (version == 1) {
            created = read64(is);
            modified = read64(is);
            time_scale = read32(is);
            duration = read64(is);
        } else {
            created = read32(is);
            modified = read32(is);
            time_scale = read32(is);
            duration = read32(is);
        }
        lang = read16(is); // 1b + 5b * 3
        read16(is); // 0
    }
    virtual void write(std::ostream &os) const {
        FullBox::write(os);
        if (version == 1) {
            write64(os, created);
            write64(os, modified);
            write32(os, time_scale);
            write64(os, duration);
        } else {
            write32(os, created);
            write32(os, modified);
            write32(os, time_scale);
            write32(os, duration);
        }
        write16(os, lang);
        write16(os, 0);
    }

    virtual size_t calcSize() {
        size = HEADER_SIZE + 2 * 4 + 3 * (version == 1 ? 8 : 4);
        return size;
    }

    virtual void dump_attr(std::ostream &os, const std::string &prefix) const {
        os << prefix << " timeScale: " << time_scale << std::endl;
        os << prefix << " duration: " << duration << std::endl;
    }
};

class BoxTKHD : public FullBox{
public:
    BoxTKHD(size_t sz = 0) : FullBox(BOX_TKHD, sz) {}

    uint64_t created;
    uint64_t modified;
    uint32_t track_id;
    uint64_t duration;
    uint16_t layer;
    uint16_t volume;
    uint32_t matrix[9];
    uint32_t width;
    uint32_t height;

    void init() {
        version = 0;
        flags = 3;
        created = 0;
        modified = 0;
        track_id = 1;
        duration = 0;
        layer = 0;
        volume = 0x100;
        for (int i=0; i<9; i++) matrix[i] = 0;
        matrix[0] = 0x10000;
        matrix[4] = 0x10000;
        matrix[8] = 0x40000000;
        width = 1;
        height = 1;
    }

    void parse(std::istream &is) {
        FullBox::parse(is);
        if (version == 1) {
            created = read64(is);
            modified = read64(is);
            track_id = read32(is);
            read32(is); // 0
            duration = read64(is);
        } else {
            created = read32(is);
            modified = read32(is);
            track_id = read32(is);
            read32(is); // 0
            duration = read32(is);
        }
        read64(is); // 0
        layer = read16(is);
        read16(is); //0
        volume = read16(is);
        read16(is); //0
        for (auto &d : matrix) {
            d = read32(is);
        }
        width = read32(is);
        height = read32(is);
    }
    virtual void write(std::ostream &os) const {
        FullBox::write(os);
        if (version == 1) {
            write64(os, created);
            write64(os, modified);
            write32(os, track_id);
            write32(os, 0);
            write64(os, duration);
        } else {
            write32(os, created);
            write32(os, modified);
            write32(os, track_id);
            write32(os, 0);
            write32(os, duration);
        }
        write64(os, 0);
        write16(os, layer);
        write16(os, 0);
        write16(os, volume);
        write16(os, 0);
        for (auto &d : matrix) {
            write32(os, d);
        }
        write32(os, width);
        write32(os, height);
    }

    virtual size_t calcSize() {
        size = HEADER_SIZE + 17 * 4 + 3 * (version == 1 ? 8 : 4);
        return size;
    }

    void dump_attr(std::ostream &os, const std::string &prefix) const {
        FullBox::dump_attr(os,prefix);
        os << prefix << " created: " << created << std::endl;
        os << prefix << " modified: " << modified << std::endl;
        os << prefix << " track_id: " << track_id << std::endl;
        os << prefix << " duration: " << duration << std::endl;
        os << prefix << " volume: " << volume << std::endl;
        os << prefix << " width: " << width/0x10000 << std::endl;
        os << prefix << " height: " << height/0x10000 << std::endl;
        os << prefix << " matrix: [";
        for (auto &d : matrix) {
            os << d << ",";
        }
        os << std::endl;
    }
};


class BoxHDLR : public FullBox{
public:
    BoxHDLR(size_t sz) : FullBox(BOX_HDLR, sz) {}

    char qt_type1[4];
    char media_type[4];
    char qt_type2[12];
    std::string type_name;

    void init() {
        memset(qt_type1, 0, sizeof(qt_type1));
        memset(media_type, 0, sizeof(media_type));
        memset(qt_type2, 0, sizeof(qt_type2));
        type_name = "";
    }

    std::string typeAsString() const {
        return std::string(media_type, 4);
    }
    std::string name() const {
        return type_name;
    }

    void parse(std::istream &is) {
        FullBox::parse(is);
        is.read(qt_type1, 4);
        is.read(media_type, 4);
        is.read(qt_type2, 12);
        type_name.resize(size - HEADER_SIZE - 20 - 1);
        is.read(&type_name[0], type_name.size());
        read8(is);
    }

    void write(std::ostream &os) const {
        FullBox::write(os);
        os.write(qt_type1, 4);
        os.write(media_type, 4);
        os.write(qt_type2, 12);
        os.write(type_name.c_str(), type_name.size());
        write8(os, 0);
    }

    size_t calcSize() {size = HEADER_SIZE + 20 + type_name.size() + 1; return size;}

    virtual void dump_attr(std::ostream &os, const std::string &prefix) const {
        os << prefix << " type: " << typeAsString() << std::endl;
        os << prefix << " name: " << type_name << std::endl;
    }
};

class BoxSTSD : public FullBufBox{
public:
    BoxSTSD(size_t sz) : FullBufBox(BOX_STSD, sz) {}

    uint32_t count() const {return ui32(0);}
    uint32_t type() const {return ui32(8);}
    std::string typeAsString() const {
        return std::string((char*)&buf[8],4);
    }
    std::string desc() const {
        return std::string((char*)&buf[12],ui32(4) - 8);
    }

    virtual void dump_attr(std::ostream &os, const std::string &prefix) const {
        uint32_t c = count();
        os << prefix << " count: " << c << std::endl;
        int pos = 4;
        for (int i=0; i<c && i<10;i++) {
            int len = ui32(pos);
            os << prefix << " desc len:" << len <<  std::endl;
            os << prefix << "  type: " << ui32(pos+4) << "(" << typeAsString() << std::endl;
            pos += len;
        }
    }
};

class BoxSTSC : public FullBufBox{
public:
    BoxSTSC(size_t sz) : FullBufBox(BOX_STSC, sz) {}
    BoxSTSC() : FullBufBox(BOX_STSC, 16) {clear();}

    uint32_t count() const {return ui32(0);}
    uint32_t first(int n) const {return ui32(4 + n * 12);}
    uint32_t spc(int n) const {return ui32(4 + n * 12 + 4);}
    void clear(){buf.resize(4); ui32(0,0);}

    uint32_t sampleToChunk(int n) const {
        // n: [0..(numSample-1)]
        uint32_t ofs = 0;
        uint32_t ch = 1;
        uint32_t lch = 1;
        uint32_t lspc = 1;
        uint32_t c = count();
        for (uint32_t i = 0; i<c; i++) {
            ofs += (first(i) - lch) * lspc;
            if (n < ofs) break;
            ch = first(i) + (n - ofs) / spc(i);
            lspc = spc(i);
            lch = first(i);
        }
        return ch - 1;
    }

    virtual void dump_attr(std::ostream &os, const std::string &prefix) const {
        uint32_t c = count();
        os << prefix << " count: " << c << std::endl;
        int pos = 4;
        for (int i=0; i<c && i<10;i++) {
            os << prefix << " stsc first_chunk:" << first(i) <<  std::endl;
            os << prefix << "     spc:" << ui32(pos+4) <<  std::endl;
            os << prefix << "     descidx:" << ui32(pos+8) <<  std::endl;
            pos += 12;
        }
    }
};

class BoxSTTS : public FullBufBox{
public:
    BoxSTTS(size_t sz) : FullBufBox(BOX_STTS, sz) {}
    BoxSTTS() : FullBufBox(BOX_STTS, 16) {clear();}

    uint32_t count() const {return ui32(0);}

    uint32_t count(uint32_t n) const {return ui32(4 + n*8);}
    uint32_t delta(uint32_t n) const {return ui32(4 + n*8 + 4);}
    void clear(){buf.resize(4); ui32(0,0);}

    uint64_t sampleToTime(uint32_t n) const {
        uint32_t c = count();
        uint64_t t = 0;
        uint64_t tc = 0;
        for (uint32_t i = 0; i<c; i++) {
            if (n < count(i)) {
                return t + n * delta(i);
            }
            n -= count(i);
            t += count(i) * delta(i);
        }
        return t;
    }

    virtual void dump_attr(std::ostream &os, const std::string &prefix) const {
        uint32_t c = count();
        os << prefix << " count: " << c << std::endl;
        int pos = 4;
        for (int i=0; i<c && i<10;i++) {
            os << prefix << " stts count:" << ui32(pos) <<  std::endl;
            os << prefix << "      delta:" << ui32(pos+4) <<  std::endl;
            pos += 8;
        }
    }
};

class BoxCTTS : public FullBufBox{
public:
    BoxCTTS(size_t sz) : FullBufBox(BOX_CTTS, sz) {}

    uint32_t count() const {return ui32(0);}

    uint32_t count(uint32_t n) const {return ui32(4 + 8*n);}
    uint32_t offset(uint32_t n) const {return ui32(4 + 8*n + 4);}
    uint32_t sampleToOffset(int n) const {
        // n: [0..(numSample-1)]
        uint32_t c = count();
        uint32_t ofs = 0;
        uint32_t s = 0;
        for (uint32_t i = 0; i<c; i++) {
            ofs = offset(i);
            s += count(i);
            if (n < s) break;
        }
        return ofs;
    }

    virtual void dump_attr(std::ostream &os, const std::string &prefix) const {
        uint32_t c = count();
        os << prefix << " count: " << c << std::endl;
        int pos = 4;
        for (int i=0; i<c && i<10;i++) {
            os << prefix << " sample count:" << ui32(pos) <<  std::endl;
            os << prefix << "        offset:" << ui32(pos+4) <<  std::endl;
            pos += 8;
        }
    }
};

class BoxSTCO : public FullBufBox{
public:
    BoxSTCO(size_t sz) : FullBufBox(BOX_STCO, sz) {}
    BoxSTCO() : FullBufBox(BOX_STCO, 16) {clear();}

    uint32_t count() const {return ui32(0);}
    uint32_t offset(int pos) const {return ui32(4+pos*4);}
    void clear(){buf.resize(4); ui32(0,0);}

    void moveAll(int ofs) {
        uint32_t c = count();
        for (int i=0; i<c;i++) {
            ui32(4+i*4, offset(i) + ofs);
        }
    }

    virtual void dump_attr(std::ostream &os, const std::string &prefix) const {
        uint32_t c = count();
        os << prefix << " count: " << c << std::endl;
        for (int i=0; i<c && i<10;i++) {
            os << prefix << "  offset:" << offset(i) <<  std::endl;
        }
    }
};

class BoxSTSS : public FullBufBox{
public:
    BoxSTSS(size_t sz) : FullBufBox(BOX_STSS, sz) {}

    uint32_t count() const {return ui32(0);}
    uint32_t sync(int pos) const {return ui32(4+pos*4);}
    bool include(uint32_t sample) const {
        uint32_t c = count();
        for (int i=0; i<c; i++) {
            if (sync(i) == sample) return true; // TODO binary search.
        }
        return false;
    }

    virtual void dump_attr(std::ostream &os, const std::string &prefix) const {
        uint32_t c = count();
        os << prefix << " count: " << c << std::endl;
        for (int i=0; i<c && i<10;i++) {
            os << prefix << "  sync:" << sync(i) <<  std::endl;
        }
    }
};

class BoxSTSZ : public FullBufBox{
public:
    BoxSTSZ(size_t sz) : FullBufBox(BOX_STSZ, sz) {}
    BoxSTSZ() : FullBufBox(BOX_STSZ, 16) {clear();}

    uint32_t constantSize() const {return ui32(0);}
    uint32_t count() const {return ui32(4);}
    uint32_t size(int pos) const {return ui32(8+pos*4);}
    void clear(){buf.resize(8); ui32(0,0); ui32(4,0);}

    virtual void dump_attr(std::ostream &os, const std::string &prefix) const {
        uint32_t c = count();
        os << prefix << " count: " << c << std::endl;
        os << prefix << " constant: " << constantSize() << std::endl;
        if (constantSize() == 0) {
            for (int i=0; i<c && i<10;i++) {
                os << prefix << "  size:" << size(i) <<  std::endl;
            }
        }
    }
};


class BoxSTYP : public Box{
public:
    BoxSTYP(size_t sz) : Box(BOX_STYP, sz) {
    }
    char major[4];
    uint32_t minor;
    std::vector<uint32_t> compat;

    void parse(std::istream &is) {
        is.read(major, 4);
        minor = read32(is);
        for (int i=0; i<(size-16)/4; i++) {
            uint32_t b;
            is.read((char*)&b, 4);
            compat.push_back(b);
        }
    }

    virtual void write(std::ostream &os) const {
        Box::write(os);
        os.write(major, 4);
        write32(os, minor);
        os.write((char*)&compat[0], compat.size()*4);
    }

    virtual size_t calcSize() {size = compat.size()*4 +16; return size;}

    virtual void dump_attr(std::ostream &os, const std::string &prefix) const {
        os << prefix << " major: " << major << std::endl;
        os << prefix << " minor: " << minor << std::endl;
    }
};

class BoxTREX : public FullBox{
public:
    uint32_t track_id;
    uint32_t sample_desc;
    uint32_t sample_duration;
    uint32_t sample_size;
    uint32_t sample_flags;

    BoxTREX(size_t sz) : FullBox(BOX_TREX, sz) {}
    BoxTREX() : FullBox(BOX_TREX, 8 + 4 + 20), track_id(1), sample_desc(1) {
        sample_duration = 0;
        sample_size = 0;
        sample_flags = 0;
    }

    void parse(std::istream &is) {
        FullBox::parse(is);
        track_id = read32(is);
        sample_desc = read32(is);
        sample_duration = read32(is);
        sample_size = read32(is);
        sample_flags = read32(is);
    }

    void write(std::ostream &os) const {
        FullBox::write(os);
        write32(os, track_id);
        write32(os, sample_desc);
        write32(os, sample_duration);
        write32(os, sample_size);
        write32(os, sample_flags);
    }

    size_t calcSize() {size = 32; return size;}

    virtual void dump_attr(std::ostream &os, const std::string &prefix) const {
        FullBox::dump_attr(os, prefix);
        os << prefix << " track_id: " << track_id << std::endl;
        os << prefix << " sample_desc: " << sample_desc << std::endl;
        os << prefix << " sample_duration: " << sample_duration << std::endl;
        os << prefix << " sample_size: " << sample_size << std::endl;
        os << prefix << " sample_flags: " << sample_flags << std::endl;
    }
};

class BoxSIDX : public FullBox{
public:
    uint32_t track_id;
    uint32_t time_scale;
    uint64_t pts;
    uint64_t first_offset; // offset to moof
    std::vector<uint32_t> data;

    BoxSIDX(size_t sz) : FullBox(BOX_SIDX, sz) {}
    BoxSIDX() : FullBox(BOX_SIDX, HEADER_SIZE+28), track_id(1), time_scale(1000),first_offset(0) {version = 1;}

    int count() const {return data.size()/3;}
    uint32_t duration(int n) const {return data[n*3+1];}
    bool startsWithSAP(int n) const {return (data[n*3+2]&0x80000000) != 0;}
    void add(uint32_t ref, uint32_t duration, uint32_t flag) {
        data.push_back(ref);
        data.push_back(duration);
        data.push_back(flag);
    }

    void parse(std::istream &is) {
        FullBox::parse(is);
        track_id = read32(is);
        time_scale = read32(is);
        pts = read64(is);
        first_offset = read64(is);
        int count = read32(is);
        for (int i=0; i<count; i++) {
            data.push_back(read32(is));
            data.push_back(read32(is));
            data.push_back(read32(is));
        }
    }

    void write(std::ostream &os) const {
        FullBox::write(os);
        write32(os, track_id);
        write32(os, time_scale);
        write64(os, pts);
        write64(os, first_offset);
        write32(os, count());
        for (int i=0; i<data.size(); i++) {
            write32(os, data[i]);
        }
    }

    size_t calcSize() {size = HEADER_SIZE+28 + data.size()*sizeof(uint32_t); return size;}

    virtual void dump_attr(std::ostream &os, const std::string &prefix) const {
        FullBox::dump_attr(os, prefix);
        os << prefix << " track_id: " << track_id << std::endl;
        os << prefix << " time_scale: " << time_scale << std::endl;
        os << prefix << " pts: " << pts << std::endl;
        os << prefix << " first_offset: " << first_offset << std::endl;
        os << prefix << " count: " << count() << std::endl;
        for (int i=0; i<count(); i++) {
          os << prefix << "  ref: " << data[i*3] << " duration:" << data[i*3+1] << " sap:" << startsWithSAP(i) << std::endl;
        }
    }
};

class BoxMFHD : public FullBox{
public:
    uint32_t fragments;

    BoxMFHD(size_t sz = 0) : FullBox(BOX_MFHD, sz), fragments(1) {}

    void parse(std::istream &is) {
        FullBox::parse(is);
        fragments = read32(is);
    }

    void write(std::ostream &os) const {
        FullBox::write(os);
        write32(os, fragments);
    }

    size_t calcSize() {size = HEADER_SIZE + 4; return size;}

    void dump_attr(std::ostream &os, const std::string &prefix) const {
        FullBox::dump_attr(os, prefix);
        os << prefix << " fragments: " << fragments << std::endl;
    }
};


class BoxTFDT : public FullBox{
public:
    uint64_t flag_start;

    BoxTFDT(size_t sz = 0) : FullBox(BOX_TFDT, sz), flag_start(0) {}

    void parse(std::istream &is) {
        FullBox::parse(is);
        if (version == 1) {
            flag_start = read64(is);
        } else {
            flag_start = read32(is);
        }
    }

    void write(std::ostream &os) const {
        FullBox::write(os);
        write64(os, flag_start);
    }

    size_t calcSize() {
        version = 1; // always 64bit
        size = HEADER_SIZE + 8;
        return size;
    }

    void dump_attr(std::ostream &os, const std::string &prefix) const {
        FullBox::dump_attr(os, prefix);
        os << prefix << " flag_start: " << flag_start << std::endl;
    }
};

class BoxTRUN : public FullBox{
public:
    static const int FLAG_DATA_OFFSET = 0x01;
    static const int FLAG_FIRST_SAMPLE_FLAGS = 0x04;
    static const int FLAG_SAMPLE_DURATION = 0x0100;
    static const int FLAG_SAMPLE_SIZE = 0x0200;
    static const int FLAG_SAMPLE_FLAGS = 0x0400;
    static const int FLAG_SAMPLE_CTS = 0x0800;

    uint64_t data_offset; // := sizeof moof.
    std::vector<uint32_t> data;

    BoxTRUN(size_t sz) : FullBox(BOX_TRUN, sz) {}
    BoxTRUN() : FullBox(BOX_TRUN, HEADER_SIZE+28) {}

    int count() const {return data.size()/fields();}
    uint32_t duration(int n) const {return data[n*3+1];}
    bool startsWithSAP(int n) const {return (data[n*3+2]&0x80000000) != 0;}
    void add(uint32_t v) {
        data.push_back(v);
    }

    void parse(std::istream &is) {
        FullBox::parse(is);
        int n = read32(is) * fields();
        for (int i=0; i<n; i++) {
            data.push_back(read32(is));
        }
    }

    void write(std::ostream &os) const {
        FullBox::write(os);
        write32(os, count());

        if (flags & FLAG_DATA_OFFSET) {
            write32(os, data_offset);
        }

        if (flags & FLAG_FIRST_SAMPLE_FLAGS) {
            write32(os, 0);
        }

        for (int i=0; i<data.size(); i++) {
            write32(os, data[i]);
        }
    }

    size_t calcSize() {size = HEADER_SIZE+8 + data.size()*sizeof(uint32_t); return size;}

    virtual void dump_attr(std::ostream &os, const std::string &prefix) const {
        FullBox::dump_attr(os, prefix);
        os << prefix << " count: " << count() << std::endl;
    }

private:
    int fields() const {
        int f = 0;
        if (flags & FLAG_SAMPLE_DURATION) f++;
        if (flags & FLAG_SAMPLE_SIZE) f++;
        if (flags & FLAG_SAMPLE_FLAGS) f++;
        if (flags & FLAG_SAMPLE_CTS) f++;
        return f;
    }
};

class BoxTFHD : public FullBox{
public:
    static const int FLAG_BASE_DATA_OFFSET = 0x01;
    static const int FLAG_STSD_ID = 0x02;
    static const int FLAG_DEFAULT_DURATION = 0x08;
    static const int FLAG_DEFAULT_SIZE = 0x10;
    static const int FLAG_DEFAULT_FLAGS = 0x20;
    static const int FLAG_DURATION_IS_EMPTY = 0x010000;
    static const int FLAG_DEFAULT_BASE_IS_MOOF = 0x020000;

    uint32_t track_id;
    uint32_t default_duration;
    uint32_t default_size;
    uint32_t default_flags;

    BoxTFHD(size_t sz = 0) : FullBox(BOX_TFHD, sz), track_id(1) {
        flags = FLAG_DEFAULT_BASE_IS_MOOF | FLAG_DEFAULT_DURATION;
    }

    void parse(std::istream &is) {
        FullBox::parse(is);
        track_id = read32(is);
    }

    void write(std::ostream &os) const {
        FullBox::write(os);
        write32(os, track_id);
        if (flags & FLAG_BASE_DATA_OFFSET) {
            write64(os, 0);
        }
        if (flags & FLAG_DEFAULT_DURATION) {
            write32(os, default_duration);
        }
        if (flags & FLAG_DEFAULT_SIZE) {
            write32(os, default_size);
        }
        if (flags & FLAG_DEFAULT_FLAGS) {
            write32(os, default_flags);
        }
    }

    size_t calcSize() {
        size = HEADER_SIZE + 4;
        if (flags & FLAG_BASE_DATA_OFFSET) {
            size += 8;
        }
        if (flags & FLAG_DEFAULT_DURATION) {
            size += 4;
        }
        if (flags & FLAG_DEFAULT_SIZE) {
            size += 4;
        }
        if (flags & FLAG_DEFAULT_FLAGS) {
            size += 4;
        }
        return size;
    }

    void dump_attr(std::ostream &os, const std::string &prefix) const {
        FullBox::dump_attr(os, prefix);
        os << prefix << " track_id: " << track_id << std::endl;
    }
};

class BoxPSSH : public FullBox{
public:
    uint8_t system_id[16];
    std::vector<std::string> kids;
    std::vector<uint8_t> data;

    BoxPSSH(size_t sz = 0) : FullBox(BOX_PSSH, sz) {}

    void parse(std::istream &is) {
        FullBox::parse(is);
        is.read((char*)system_id, 16);
        int count = read32(is);
        char keybuf[16];
        for (int i=0;i<count; i++) {
            is.read(keybuf, 16);
            kids.push_back(std::string(keybuf, 16));
        }
        data.resize(read32(is));
        is.read((char*)&data[0], data.size());
    }

    void write(std::ostream &os) const {
        FullBox::write(os);
        os.write((char*)system_id ,16);
        write32(os, kids.size());
        for (auto &kid : kids) {
            os.write(kid.c_str(), 16);
        }
        write32(os, data.size());
        os.write((char*)&data[0], data.size());
    }

    size_t calcSize() {size = HEADER_SIZE + 20 + kids.size()*16; return size;}

    void dump_attr(std::ostream &os, const std::string &prefix) const {
        FullBox::dump_attr(os, prefix);
        os << prefix << " count: " << kids.size() << std::endl;
    }
};

class BoxSimpleList : public Box {
    bool chktype(const char t1[4], const char t2[4]) {
        return memcmp(t1, t2, 4)==0;
    }
public:
    BoxSimpleList(const char boxtype[4], size_t sz = 0) : Box(boxtype, sz) {}

    void add(Box *b) {
        b->ref_count++;
        children.push_back(b);
    }
    void clear() {
        for (auto &b :children) {
            b->ref_count--;
            if (b->ref_count <= 0) delete b;
        }
        children.clear();
    }

    Box* createBox(const char boxtype[4], const size_t sz) {
        if (chktype(boxtype, BOX_FTYP)) {
            return new BoxFTYP(sz);
        }
        if (chktype(boxtype, BOX_FREE)) {
            return new BoxFREE(sz);
        }
        if (chktype(boxtype, BOX_MVHD)) {
            return new BoxMVHD(sz);
        }
        if (chktype(boxtype, BOX_MDHD)) {
            return new BoxMDHD(sz);
        }
        if (chktype(boxtype, BOX_TKHD)) {
            return new BoxTKHD(sz);
        }
        if (chktype(boxtype, BOX_HDLR)) {
            return new BoxHDLR(sz);
        }
        if (chktype(boxtype, BOX_STSC)) {
            return new BoxSTSC(sz);
        }
        if (chktype(boxtype, BOX_STSD)) {
            return new BoxSTSD(sz);
        }
        if (chktype(boxtype, BOX_STSS)) {
            return new BoxSTSS(sz);
        }
        if (chktype(boxtype, BOX_STSZ)) {
            return new BoxSTSZ(sz);
        }
        if (chktype(boxtype, BOX_STCO)) {
            return new BoxSTCO(sz);
        }
        if (chktype(boxtype, BOX_STTS)) {
            return new BoxSTTS(sz);
        }
        if (chktype(boxtype, BOX_CTTS)) {
            return new BoxCTTS(sz);
        }

        if (chktype(boxtype, BOX_STYP)) {
            return new BoxSTYP(sz);
        }
        if (chktype(boxtype, BOX_SIDX)) {
            return new BoxSIDX(sz);
        }
        if (chktype(boxtype, BOX_TREX)) {
            return new BoxTREX(sz);
        }

        if (has_child(boxtype)) {
            return new BoxSimpleList(boxtype, sz);
        }
        if (sz > BOX_READ_SIZE_LIMIT) { // skip large box.
            return new UnknownBoxRef(boxtype, sz);
        }
        return new UnknownBox(boxtype, sz);
    }

    void parse(std::istream &is) {
        size_t pos = is.tellg();
        size_t end = pos + size - 8;
        char type[5] = {0};
        while (pos < end) {
            uint32_t sz = read32(is);
            is.read(&type[0],4);
            if (is.eof()) break;

            Box *b = createBox(type,sz);
            b->parse(is);
            children.push_back(b);
            pos += sz;
            is.seekg(pos,  std::ios_base::beg);
        }
    }

    virtual size_t calcSize() {
        size = 8;
        for (auto &b : children) {
            size += b->calcSize();
        }
        return size;
    }
};


class Mp4Root : public BoxSimpleList {
public:
    Mp4Root() : BoxSimpleList("ROOT", 0x7fffffff) {} // TODO size
    virtual void write(std::ostream &os) const {
        for (int i=0; i<children.size(); i++) {
            children[i]->calcSize();
            children[i]->write(os);
        }
    }
};

static inline std::ostream& operator<<(std::ostream &os, const Box& b) {
    b.dump(os, "");
    return os;
}

} // namespace isobmff

#endif
