#ifndef PSQT_H_INCLUDED
#define PSQT_H_INCLUDED 1

#include <cassert>

#include "types.h"
#include "bitboard.h"
#include "types.h"

namespace PSQT
{

// Parameters for Bishop tiling
inline constexpr Score BishTiling[4] =
{
    make_score(BishopValueMg, BishopValueEg) + make_score(-17, -31), // outmost ring
    make_score(BishopValueMg, BishopValueEg) + make_score(10, -5),
    make_score(BishopValueMg, BishopValueEg) + make_score(12, 1),
    make_score(BishopValueMg, BishopValueEg) + make_score(35, 16)  // central 4 squares
};
 
// Parameters for Queen tiling
inline constexpr Score QueenTiling[4] =
{
    make_score(QueenValueMg, QueenValueEg) + make_score(-17, -31), // outmost ring
    make_score(QueenValueMg, QueenValueEg) + make_score(10, -5),
    make_score(QueenValueMg, QueenValueEg) + make_score(12, 1),
    make_score(QueenValueMg, QueenValueEg) + make_score(35, 16)  // central 4 squares
};

// Introduce new namespace name such that we cannot accidentally use psq array
// directly
namespace hidden
{
extern Score psq[PIECE_NB /*-2 if you want to shrink array size*/][SQUARE_NB];
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

    // Index mapping when using smaller psq array
    /*
    // Pieces above W_BISHOP shifted by 1,
    // pieced also above B_BISHOP by 1 again (2 in total).
    return Piece(
            (p) - bool(p > W_BISHOP) - bool(p > B_BISHOP)
            );
    */
}

// Function to replace direct access to psq array. If Piece is a bishop, we
// implement 4-ring tiling
inline Score psq(Piece piece, Square s)
{
    // For bishops, using ring tiling
    if (type_of(piece) == BISHOP || type_of(piece) == QUEEN)
    {
        // Find tiling index. 0 = outmost ring, 3 = central 4 squares
        auto tileIdx = std::min(
                edge_distance(file_of(s)),
                edge_distance(rank_of(s))
                );
        return color_of(piece) == WHITE ? BishTiling[tileIdx] : -BishTiling[tileIdx];
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
