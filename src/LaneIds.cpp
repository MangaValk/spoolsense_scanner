#include "LaneIds.h"

#include <cstdio>
#include <cstring>

namespace LaneIds {

bool format(const char* base, uint8_t lane, uint8_t laneCount, char* out, size_t outLen) {
    if (out == nullptr || outLen == 0) return false;
    out[0] = '\0';
    if (base == nullptr || lane >= (laneCount ? laneCount : 1)) return false;
    int n = (laneCount <= 1)
            ? snprintf(out, outLen, "%s", base)
            : snprintf(out, outLen, "%s-L%u", base, (unsigned)(lane + 1));
    if (n < 0 || (size_t)n >= outLen) {
        out[0] = '\0';
        return false;
    }
    return true;
}

int parse(const char* base, uint8_t laneCount, const char* id, size_t idLen) {
    if (base == nullptr || id == nullptr) return -1;
    const size_t baseLen = strlen(base);
    if (idLen < baseLen || strncmp(id, base, baseLen) != 0) return -1;

    if (laneCount <= 1) {
        return (idLen == baseLen) ? 0 : -1;  // stock: only the bare ID
    }

    // "<base>-L<n>", n = 1..laneCount, no leading zeros
    const char* p = id + baseLen;
    size_t rest = idLen - baseLen;
    if (rest < 3 || p[0] != '-' || p[1] != 'L' || p[2] < '1' || p[2] > '9') return -1;
    unsigned n = 0;
    for (size_t i = 2; i < rest; i++) {
        if (p[i] < '0' || p[i] > '9') return -1;
        n = n * 10 + (unsigned)(p[i] - '0');
        if (n > 255) return -1;
    }
    return (n >= 1 && n <= laneCount) ? (int)(n - 1) : -1;
}

}  // namespace LaneIds
