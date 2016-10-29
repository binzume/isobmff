#include "flv.h"
#include <iostream>
#include <fstream>

using namespace flv;
using namespace std;

int main() {
    ifstream f("test.flv", ios::binary);

    FLVHeader header;
    parse(header, f);
    cout << "ver:" << (int)header.version << endl;

    for (int i=0; i<200 && !f.eof(); i++) {
        uint32_t prev_size = read32(f);
        FLVTagHeader th;
        parse(th, f);
        cout << prev_size << " time:" << th.timestamp << " type:" << (int)th.type  << " size:" << th.size << endl;
        skip_data(th, f);
    }

    return 0;
}

