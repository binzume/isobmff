#include "flv.h"
#include <iostream>
#include <fstream>

using namespace flv;
using namespace std;

int main() {
    int maxtags = 100;
    ifstream f("test.flv", ios::binary);

    FLVHeader header;
    parse(header, f);
    cout << "ver:" << (int)header.version << endl;
    if (header.data_offset != 9) {
        f.seekg(header.data_offset);
    }

    for (int i=0; !f.eof() && i<maxtags; i++) {
        uint32_t prev_size = read32(f);
        FLVTagHeader th;
        int pos = f.tellg();
        parse(th, f);
        cout << "pos:" << pos << " time:" << th.timestamp << " type:" << (int)th.type  << " size:" << th.size << " prev:" << prev_size << endl;
        skip_data(th, f);
    }

    return 0;
}

