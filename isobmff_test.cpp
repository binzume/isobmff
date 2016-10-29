#include "isobmff.h"
#include <iostream>
#include <fstream>

using namespace std;
using namespace isobmff;

int main() {

    ifstream f("test.mp4", ios::binary);

    Mp4Root mp4;
    mp4.parse(f);
    cout << mp4;

    Box *b = mp4.findByType(BOX_MVHD);
    cout << *b << endl;

    // add 'free' box.
    BoxFREE *fbox = new BoxFREE(32);
    strcpy((char*)&fbox->body[0], "Hello!");
    mp4.children.push_back(fbox);

    ofstream of("out.mp4", ios::binary); // skip mdat, if large.
    mp4.write(of);

    return 0;
}

