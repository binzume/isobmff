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
    is.write(buf, 1);
}
static inline void write24(std::ostream &is, uint32_t d) {
    char buf[] = {d >> 16, d >> 8, d};
    is.write(buf, 3);
}
static inline void write32(std::ostream &is, uint32_t d) {
    char buf[] = {d >> 24, d >> 16, d >> 8, d};
    is.write(buf, 4);
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
        buf.resize(sz - 12);
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
        is.read((char*)&buf[0], size - 12);
    }
    virtual void write(std::ostream &os) const {
        FullBox::write(os);
        if (size > 8) {
            os.write((char*)&buf[0], buf.size());
        }
    }

    virtual size_t calcSize() {size = buf.size()+12; return size;}
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

static const char *BOX_STYP = "styp";
static const char *BOX_MOOF = "moof";
static const char *BOX_MFHD = "mfhd";
static const char *BOX_TRAF = "traf";
static const char *BOX_TFHD = "tfhd";
static const char *BOX_TFDT = "tfdt";
static const char *BOX_TRUN = "trun";
static const char *BOX_TREX = "trex";
static const char *BOX_SIDX = "sidx";


static const char* HAS_CHILD[] = {BOX_MOOV, BOX_TRAK, BOX_DTS};

bool has_child(const char type[4]){
    return memcmp(type, BOX_MOOV, 4) == 0 || memcmp(type, BOX_TRAK, 4) == 0 ||
        memcmp(type, BOX_DTS, 4) == 0 || memcmp(type, BOX_MDIA, 4) == 0 ||
        memcmp(type, BOX_MINF, 4) == 0 || memcmp(type, BOX_STBL, 4) == 0
        || memcmp(type, "udta", 4) == 0 || memcmp(type, BOX_MOOF, 4) == 0 || memcmp(type, BOX_TRAF, 4) == 0;
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
    BoxMVHD(size_t sz) : FullBox(BOX_MVHD, sz) {}
    uint32_t created;
    uint32_t modified;
    uint32_t rate;
    uint32_t volume;
    uint32_t timeScale;
    uint64_t duration;
    std::vector<uint8_t> buf;

    virtual void dump_attr(std::ostream &os, const std::string &prefix) const {
        FullBox::dump_attr(os,prefix);
        os << prefix << " created: " << created << std::endl;
        os << prefix << " modified: " << modified << std::endl;
        os << prefix << " duration: " << duration << "/" << timeScale << std::endl;
        os << prefix << " rate: " << rate << std::endl;
        os << prefix << " volume: " << volume << std::endl;
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
        buf.resize(size - (8 + 4 + 4 * 6)); // fullbox + 32bit*6
        is.read((char*)&buf[0], size - (8 + 4 + 4 * 6));
    }
    virtual void write(std::ostream &os) const {
        FullBox::write(os);
        write32(os, created);
        write32(os, modified);
        write32(os, timeScale);
        write32(os, duration);
        write32(os, rate);
        write32(os, volume);
        os.write((char*)&buf[0], buf.size());
    }
};

class BoxMDHD : public FullBufBox{
public:
    BoxMDHD(size_t sz) : FullBufBox(BOX_MDHD, sz) {}

    uint64_t created() const {
        if (version == 0) {
            return ui32(0);
        } else {
            return ui64(0);
        }
    }
    uint64_t modified() const {
        if (version == 0) {
            return ui32(4);
        } else {
            return ui64(8);
        }
    }
    uint32_t timeScale() const {
        if (version == 0) {
            return ui32(4*2);
        } else {
            return ui32(8*2);
        }
    }
    uint64_t duration() const {
        if (version == 0) {
            return ui32(4*2 + 4);
        } else {
            return ui64(8*2 + 4);
        }
    }

    virtual void dump_attr(std::ostream &os, const std::string &prefix) const {
        os << prefix << " timeScale: " << timeScale() << std::endl;
        os << prefix << " duration: " << duration() << std::endl;
    }
};

class BoxTKHD : public FullBufBox{
public:
    BoxTKHD(size_t sz) : FullBufBox(BOX_TKHD, sz) {}

    uint64_t created() const {
        if (version == 0) {
            return ui32(0);
        } else {
            return ui64(0);
        }
    }
    uint64_t modified() const {
        if (version == 0) {
            return ui32(4);
        } else {
            return ui64(8);
        }
    }
    uint32_t trackId() const {
        if (version == 0) {
            return ui32(4*2);
        } else {
            return ui32(8*2);
        }
    }
    // ui32 reserved.
    uint64_t duration() const {
        if (version == 0) {
            return ui32(4*2 + 4*2);
        } else {
            return ui64(8*2 + 4*2);
        }
    }
    // ui32[2] si16[2]

    uint16_t volume() const {
        if (version == 0) {
            return ui16(4*3 + 4*5);
        } else {
            return ui16(8*3 + 4*5);
        }
    }
    int32_t matrix(int n) const {
        if (version == 0) {
            return ui32(4*3 + 4*6 + n);
        } else {
            return ui32(8*3 + 4*6 + n);
        }
    }
    uint32_t width() const {
        if (version == 0) {
            return ui32(4*3 + 4*15);
        } else {
            return ui32(8*3 + 4*15);
        }
    }
    uint32_t height() const {
        if (version == 0) {
            return ui32(4*3 + 4*16);
        } else {
            return ui32(8*3 + 4*16);
        }
    }

    void duration(uint64_t t) {
        if (version == 0) {
            ui32(4*2 + 4*2, t);
        } else {
            ui32(8*2 + 4*2, t >> 32);
            ui32(8*2 + 4*2 + 4, t);
        }
    }
    void trackId(uint32_t id) {
        if (version == 0) {
            ui32(4*2, id);
        } else {
            ui32(8*2, id);
        }
    }

    virtual void dump_attr(std::ostream &os, const std::string &prefix) const {
        os << prefix << " track: " << trackId() << std::endl;
        os << prefix << " duration: " << duration() << std::endl;
        os << prefix << " volume: " << volume() << std::endl;
        os << prefix << " width: " << width()/65536 << std::endl;
        os << prefix << " height: " << height()/65536 << std::endl;
        os << prefix << " mat:[";
        for (int i=0;i<9;i++) os << matrix(i) << ",";
        os << "]" << std::endl;
    }
};


class BoxHDLR : public FullBufBox{
public:
    BoxHDLR(size_t sz) : FullBufBox(BOX_HDLR, sz) {}


    std::string typeAsString() const {
        return std::string((char*)&buf[4],4);
    }
    std::string name() const {
        return std::string((char*)&buf[20],buf.size()-20);
    }

    void name(const std::string &name) {
        buf.resize(20 + name.size() + 1);
        memcpy(&buf[20], name.c_str(), name.size()+1);
    }

    virtual void dump_attr(std::ostream &os, const std::string &prefix) const {
        os << prefix << " type: " << typeAsString() << std::endl;
        os << prefix << " name: " << name() << std::endl;
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
        uint32_t c = count();
        uint32_t ofs = 0;
        uint32_t ch = 1;
        uint32_t lch = 1;
        uint32_t lspc = 1;
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
            os << prefix << " stsc first_chunk:" << ui32(pos) <<  std::endl;
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
    BoxSIDX() : FullBox(BOX_SIDX, 12+28), track_id(1), time_scale(1000),first_offset(0) {version = 1;}

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

    size_t calcSize() {size = 12+28 + data.size()*sizeof(uint32_t); return size;}

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

    size_t calcSize() {size = 12 + 4; return size;}

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
        size = 12 + 8;
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
    BoxTRUN() : FullBox(BOX_TRUN, 12+28) {}

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

    size_t calcSize() {size = 12+8 + data.size()*sizeof(uint32_t); return size;}

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
        size = 12 + 4;
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
            auto *b = new BoxSTSD(sz);
            return b;
        }
        if (chktype(boxtype, BOX_STSS)) {
            auto *b = new BoxSTSS(sz);
            return b;
        }
        if (chktype(boxtype, BOX_STSZ)) {
            auto *b = new BoxSTSZ(sz);
            return b;
        }
        if (chktype(boxtype, BOX_STCO)) {
            auto *b = new BoxSTCO(sz);
            return b;
        }
        if (chktype(boxtype, BOX_STTS)) {
            auto *b = new BoxSTTS(sz);
            return b;
        }
        if (chktype(boxtype, BOX_CTTS)) {
            auto *b = new BoxCTTS(sz);
            return b;
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
