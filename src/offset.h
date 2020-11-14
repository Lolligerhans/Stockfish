#ifndef OFFSET_H_INCLUDED
#define OFFSET_H_INCLUDED 1

#include "types.h"

namespace
{

constexpr auto S = make_score;

constexpr Score MobOffsets[] =
{
    S(-5, 0),
    S(-35, -22),
    S(-15, -14),
    S(-50, -90) // Queen mobility is on average about S(50, 90).
};

}

// Returns score offset such that average mobility bonus will be closer to S(0, 0) for the piece type
inline constexpr Score mobOffset(PieceType pt)
{
    constexpr int Offset = KNIGHT;
    assert(Offset <= pt && pt < Offset+std::size(MobOffsets));
    return MobOffsets[int(pt) - Offset];
}

#endif
