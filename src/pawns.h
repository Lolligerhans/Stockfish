/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2008 Tord Romstad (Glaurung author)
  Copyright (C) 2008-2015 Marco Costalba, Joona Kiiski, Tord Romstad
  Copyright (C) 2015-2019 Marco Costalba, Joona Kiiski, Gary Linscott, Tord Romstad

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef PAWNS_H_INCLUDED
#define PAWNS_H_INCLUDED

#include "misc.h"
#include "position.h"
#include "types.h"

namespace Pawns {

/// Pawns::Entry contains various information about a pawn structure. A lookup
/// to the pawn hash table (performed by calling the probe function) returns a
/// pointer to an Entry object.

struct Entry {

  Score pawn_score(Color c) const { return scores[c]; }
  Bitboard pawn_attacks(Color c) const { return pawnAttacks[c]; }
  Bitboard passed_pawns(Color c) const { return passedPawns[c]; }
  template<Color Us>
  Bitboard outpost_squares() const&;
  int passed_count() const { return popcount(passedPawns[WHITE] | passedPawns[BLACK]); }

  template<Color Us>
  Score king_safety(const Position& pos) {
    return  kingSquares[Us] == pos.square<KING>(Us) && castlingRights[Us] == pos.castling_rights(Us)
          ? kingSafety[Us] : (kingSafety[Us] = do_king_safety<Us>(pos));
  }

  template<Color Us>
  Score do_king_safety(const Position& pos);

  template<Color Us>
  void evaluate_shelter(const Position& pos, Square ksq, Score& shelter);

  template<Color Us>
  void compute_outposts(void) const&;

  Key key;
  Score scores[COLOR_NB];
  Bitboard passedPawns[COLOR_NB];
  Bitboard pawnAttacks[COLOR_NB];
  mutable Bitboard outpostSquares[COLOR_NB];
  Square kingSquares[COLOR_NB];
  Score kingSafety[COLOR_NB];
  int castlingRights[COLOR_NB];
};

template<Color Us>
inline Bitboard Entry::outpost_squares() const&
{
    if (outpostSquares[Us] == AllSquares) compute_outposts<Us>();
    return outpostSquares[Us];
}

template<Color Us>
void Entry::compute_outposts() const&
{
    constexpr Direction        Down = (Us == WHITE ? SOUTH : NORTH);
    constexpr Bitboard OutpostRanks = (Us == WHITE ? Rank4BB | Rank5BB | Rank6BB
                                                   : Rank5BB | Rank4BB | Rank3BB);

    Bitboard const& pat = pawnAttacks[~Us];
    Bitboard const atkSpanThem = pat | shift<Down>(pat) | shift<Down+Down>(pat);

    outpostSquares[Us] = OutpostRanks & pawnAttacks[Us] & ~atkSpanThem;
}

typedef HashTable<Entry, 131072> Table;

Entry* probe(const Position& pos);

} // namespace Pawns

#endif // #ifndef PAWNS_H_INCLUDED
