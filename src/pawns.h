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

  static constexpr Bitboard PawnRanks = ~(Rank1BB | Rank8BB);
  static constexpr Bitboard PawnAtkRanks[] = { [WHITE] = ~(Rank1BB | Rank2BB),
                                               [BLACK] = ~(Rank7BB | Rank8BB) };

  template<Color c> inline
  Score pawn_score() const
  { if (c == WHITE) return make_score(squash2[c].details.wMG, squash3[c].details.wEG);
    else            return make_score(squash2[c].details.bMG, squash3[c].details.bEG); }
  template<Color c> Bitboard inline
      pawn_attacks() const { return squash2[c].raw & PawnAtkRanks[c]; }
  template<Color c> Bitboard inline
      passed_pawns() const { return squash[c].raw & PawnRanks; }
  template<Color c> Bitboard inline
      pawn_attacks_span() const { return squash3[c].raw & PawnAtkRanks[c]; }
  int weak_unopposed(Color c) const { return squash[c].details.weakUnopposed; }

  uint8_t passed_count() const { return squash[WHITE].details.passedPawnCount; }

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
  Score evaluate(const Position& pos) &;

private:
  template<Color c> Bitboard& passed_pawns_raw() & { return squash[c].raw; }
  template<Color c> Bitboard& pawn_attacks_raw() & { return squash2[c].raw; }
  template<Color c> Bitboard& pawn_attacks_span_raw() & { return squash3[c].raw; }

public:
  Key key;
private:
  union { Bitboard raw;
          struct { uint8_t passedPawnCount;
                   uint8_t passedPawns[6];
                   uint8_t weakUnopposed; } details;
        } squash[COLOR_NB];
  union { Bitboard raw;
          struct { uint16_t wMG;
                   uint16_t pawnAttacks[2];
                   uint16_t bMG; } details;
        } squash2[COLOR_NB];
  union { Bitboard raw;
          struct { uint16_t wEG;
                   uint16_t pawnAttacksSpan[2];
                   uint16_t bEG; } details;
        } squash3[COLOR_NB];
  Square kingSquares[COLOR_NB];
  Score kingSafety[COLOR_NB];
  int castlingRights[COLOR_NB];
  int pawnsOnSquares[COLOR_NB][COLOR_NB]; // [color][light/dark squares]
};

typedef HashTable<Entry, 16384> Table;

Score probe(const Position& pos, Entry**);

} // namespace Pawns

#endif // #ifndef PAWNS_H_INCLUDED
