#ifndef FLV_H_
#define FLV_H_

#include <istream>
#include <ostream>
#include <stdint.h>

namespace flv {


const static uint8_t TAG_TYPE_AUDIO = 8;
const static uint8_t TAG_TYPE_VIDEO = 9;
const static uint8_t TAG_TYPE_SCRIPT = 18;

struct FLVHeader {
    char signature[3];
    uint8_t version;
    uint8_t type_flags;
    uint32_t data_offset;
};

struct FLVTagHeader {
    uint8_t type;
    uint32_t size; // 24bit data size
    uint32_t timestamp; // 24bit + 8bit;
    uint32_t stream_id; // 24bit
    // data...
};

// utils
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
static inline uint8_t read8(std::istream &is) {
    uint8_t buf[1];
    is.read((char*)&buf[0],sizeof(buf));
    return buf[0];
}

inline static void parse(FLVHeader &h, std::istream &is) {
    is.read(h.signature, 3);
    h.version = read8(is);
    h.type_flags = read8(is);
    h.data_offset = read32(is);
}

inline static void parse(FLVTagHeader &th, std::istream &is) {
    th.type = read8(is);
    th.size = read24(is);
    th.timestamp = read24(is);
    th.timestamp |= (read8(is) << 24);
    th.stream_id = read24(is);
}

inline static int skip_data(FLVTagHeader &th, std::istream &is) {
    is.seekg(th.size,  std::ios_base::cur); // skip
    return th.size;
}


} // namespace flv


#endif
