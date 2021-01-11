#ifndef PSQT_H_INCLUDED
#define PSQT_H_INCLUDED 1

#include "types.h"
#include "bitboard.h"
#include "types.h"

namespace PSQT
{

namespace hidden
{
extern Score psq[PIECE_NB/2][SQUARE_NB];
}

inline Score psq(Piece piece, Square s)
{
    if (color_of(piece) == WHITE)
        return  hidden::psq[ piece][s];
    else
        return -hidden::psq[~piece][flip_rank(s)];
}

} // namespace PSQT

#endif // PSQT_H_INCLUDED
