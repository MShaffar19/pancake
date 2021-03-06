// Author: Ivan Sovic

#ifndef PANCAKE_ALIGNMENT_SES_DISTANCE_BANDED_H
#define PANCAKE_ALIGNMENT_SES_DISTANCE_BANDED_H

#include <cstdint>

#include <iostream>
#include <limits>
#include <memory>
#include <sstream>

#include <pacbio/alignment/SesResults.h>

namespace PacBio {
namespace Pancake {
namespace Alignment {

SesResults SESDistanceBanded(const char* query, size_t queryLen, const char* target,
                             size_t targetLen, int32_t maxDiffs, int32_t bandwidth);
}
}
}

#endif  // PANCAKE_ALIGNMENT_SES_DISTANCE_BANDED_H
