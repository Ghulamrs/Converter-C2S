// KeepCheck: the round trips the sidecar promises, run in memory over one case.
#ifndef C2S_KEEPCHECK_H
#define C2S_KEEPCHECK_H

#include <string>

#include "Options.h"

namespace c2s {

class KeepCheck {
public:
    static int run(const std::string &source, const std::string &name, Direction direction,
                   const Permissions &permissions);
};

}

#endif
