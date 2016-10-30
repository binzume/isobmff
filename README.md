ISO base media file format (isobmff)

C++ header only.

# Usage

Include isobmff.h.

```c
#include "isobmff.h"
using namespace isobmff;
```

See: isobmff_test.cpp



```c

mp4.parse(ifs)

BoxMVHD *mvhd = (BoxMVHD*)mp4.findByType(BOX_MVHD);
if (mvhd != nullptr) {
    cout << mvhd.duration << endl;
}

ofs << mp4;
```

## Examples

- isobmff_tests.cpp dump mp4 box tree.
- flv_tests.cpp  dump flv tags.
- mp4toflv.cpp  mp4 to flv converter(only video track)

# License

TBD

