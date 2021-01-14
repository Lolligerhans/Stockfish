#ifndef PSQT_H_INCLUDED
#define PSQT_H_INCLUDED 1

#include <cassert>

#include "types.h"
#include "bitboard.h"
#include "types.h"

namespace PSQT
{

// Parameters for Bishop tiling
extern const Score BishTiling[4];
 
// Parameters for Queen tiling
extern const Score QueenTiling[4];

// Introduce new namespace name such that we cannot accidentally use psq array
// directly
namespace hidden
{
extern Score psq[PIECE_NB][SQUARE_NB];
}

// Shift index such that bishops are skipped
inline Piece idx(Piece p)
{
    // Bishops and Queen no longer relevant
    assert(p != W_BISHOP);
    assert(p != B_BISHOP);
    assert(p != W_QUEEN);
    assert(p != B_QUEEN);

    return p;
}

// Function to replace direct access to psq array. If Piece is a bishop, we
// implement 4-ring tiling
inline Score psq(Piece piece, Square s)
{
    // For bishops, using ring tiling
    if (type_of(piece) == BISHOP)
    {
        // Find tiling index. 0 = outmost ring, 3 = central 4 squares
        auto tileIdx = std::min(
                edge_distance(file_of(s)),
                edge_distance(rank_of(s))
                );
        return color_of(piece) == WHITE ? BishTiling[tileIdx] : -BishTiling[tileIdx];
    }
    else if (type_of(piece) == QUEEN)
    {
        auto tileIdx = std::min(
                edge_distance(file_of(s)),
                edge_distance(rank_of(s))
                );
        return color_of(piece) == WHITE ? QueenTiling[tileIdx] : -QueenTiling[tileIdx];
    }

    // For psq values which we have not touched, use old psq table. idx()
    // adjusts the piece value such that we obtain the index in the array where
    // bishops were removed.
    else
    {
        return hidden::psq[idx(piece)][s];
    }
}

} // namespace PSQT

#endif // PSQT_H_INCLUDED
