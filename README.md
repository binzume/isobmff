ISO base media file format (isobmff)

C++ header only.

# Usage

Include isobmff.h.

```c
#include "isobmff.h"
using namespace isobmff;
```

See: isobmff_test.cpp

## TODO


```c

mp4.parse(is)

BoxMVHD *mvhd = (BoxMVHD*)mp4.findByType(BOX_MVHD);
if (mvhd != null) {
    cout << mvhd.duration << endl;
}

os << mp4;
```

# License

TBD

