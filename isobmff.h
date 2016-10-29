// ISO base media file format (isobmff)

#ifndef FLV_ISOBMFF_
#define FLV_ISOBMFF_

#include <vector>
#include <istream>
#include <ostream>
#include <string>
#include <stdint.h>

namespace isobmff
{

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

    Box(const char boxtype[4], size_t sz){
        memcpy(type, boxtype, 4);
        type[4] = '\0';
        size = sz;
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

    void dump(std::ostream &os, const std::string &prefix) const {
        os << prefix << type;
        os << " size: " << size << std::endl;
        dump_attr(os, prefix);
        for (int i=0; i<children.size(); i++) {
            children[i]->dump(os, prefix + ". ");
        }
    }
    virtual void dump_attr(std::ostream &os, const std::string &prefix) const {}

    virtual void write(std::ostream &os) const {
        write32(os, size);
        os.write(type, 4);
        for (int i=0; i<children.size(); i++) {
            children[i]->write(os);
        }
    }

    virtual ~Box() {
        for (int i=0; i<children.size(); i++) {
            delete children[i];
        }
    }
};

class FullBox : public Box {
public:
    FullBox(const char boxtype[4], size_t sz) : Box(boxtype, sz){};
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

class UnknownBox : public Box {
public:
    std::vector<uint8_t> body;
    UnknownBox(const char boxtype[4], size_t sz) : Box(boxtype, sz) { body.resize(sz-8); }
    virtual void dump_attr(std::ostream &os, const std::string &prefix) const {
        os << prefix << " unknown_body: [";
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
static const char *BOX_MOOF = "moof";
static const char *BOX_MVHD = "mvhd";
static const char *BOX_MDIA = "mdia";
static const char *BOX_MDAT = "mdat";
static const char *BOX_MINF = "minf";
static const char *BOX_STCO = "stco";
static const char *BOX_STSC = "stsc";
static const char *BOX_STSD = "stsd";
static const char *BOX_STSZ = "stsz";
static const char *BOX_STSS = "stss";
static const char *BOX_STBL = "stbl";
static const char *BOX_TRAK = "trak";
static const char *BOX_TKHD = "tkhd";
static const char *BOX_DTS  = "dts\0";


static const char* HAS_CHILD[] = {BOX_MOOV, BOX_TRAK, BOX_DTS};

bool has_child(const char type[4]){
    return memcmp(type, BOX_MOOV, 4) == 0 || memcmp(type, BOX_TRAK, 4) == 0 ||
        memcmp(type, BOX_DTS, 4) == 0 || memcmp(type, BOX_MDIA, 4) == 0 ||
        memcmp(type, BOX_MINF, 4) == 0 || memcmp(type, "stbl", 4) == 0
        || memcmp(type, "udta", 4) == 0;
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
        size_t pos = is.tellg();
        is.read(major, 4);
        minor = read32(is);
        for (int i=0; i<(size-16)/4; i++) {
            uint32_t b;
            is.read((char*)&b, 4);
            compat.push_back(b);
        }
        is.seekg(pos + size - 8,  std::ios_base::beg);
    }

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
        os << prefix << " free: [";
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
    BoxMVHD(size_t sz) : FullBox(BOX_MVHD, sz) {
    }
    uint32_t created;
    uint32_t modified;
    uint32_t rate;
    uint32_t volume;
    uint32_t timeBase;
    uint64_t duration;
    std::vector<uint8_t> buf;

    virtual void dump_attr(std::ostream &os, const std::string &prefix) const {
        FullBox::dump_attr(os,prefix);
        os << prefix << " created: " << created << std::endl;
        os << prefix << " modified: " << modified << std::endl;
        os << prefix << " duration: " << duration << "/" << timeBase << std::endl;
        os << prefix << " rate: " << rate << std::endl;
        os << prefix << " volume: " << volume << std::endl;
    }

    void parse(std::istream &is) {
        size_t pos = is.tellg();
        FullBox::parse(is);
        if (version != 0) return ; // TODO: error.
        created = read32(is);
        modified = read32(is);
        timeBase = read32(is);
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
        write32(os, timeBase);
        write32(os, duration);
        write32(os, rate);
        write32(os, volume);
        os.write((char*)&buf[0], buf.size());
    }
};

class BoxSimpleList : public Box {
public:
    BoxSimpleList(const char boxtype[4], size_t sz) : Box(boxtype, sz) {
    }

    bool chktype(const char t1[4], const char t2[4]) {
        return memcmp(t1, t2, 4)==0;
    }

    Box* createBox(const char boxtype[4], const size_t sz, std::istream &is) {
        if (chktype(boxtype, BOX_FTYP)) {
            BoxFTYP *b = new BoxFTYP(sz);
            b->parse(is);
            return b;
        }
        if (chktype(boxtype, BOX_MVHD)) {
            BoxMVHD *b = new BoxMVHD(sz);
            b->parse(is);
            return b;
        }

        if (has_child(boxtype)) {
            BoxSimpleList *b = new BoxSimpleList(boxtype, sz);
            b->parse(is);
            return b;
        }
        if (sz > 1024 * 1024 * 10) { // skip large box.
            UnknownBoxRef *b = new UnknownBoxRef(boxtype, sz);
            b->parse(is);
            return b;
        }
        UnknownBox *b = new UnknownBox(boxtype, sz);
        b->parse(is);
        return b;
    }

    void parse(std::istream &is) {
        size_t pos = is.tellg();
        pos += size - 8;

        while (is.tellg() < pos) {
            uint32_t sz = read32(is);
            char type[5] = {0};
            is.read(&type[0],4);
            if (is.eof()) break;
            children.push_back(createBox(type,sz, is));
        }
        is.seekg(pos,  std::ios_base::beg);
    }

};

class Mp4Root : public BoxSimpleList {
public:
    Mp4Root() : BoxSimpleList("ROOT", 0x7fffffff) {} // TODO size
    virtual void write(std::ostream &os) const {
        for (int i=0; i<children.size(); i++) {
            children[i]->write(os);
        }
    }
};

static inline std::ostream& operator<<(std::ostream &os, const Box& b) {
    b.dump(os, "");
    return os;
}

} // isobmff

#endif
