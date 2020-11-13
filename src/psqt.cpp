/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2021 The Stockfish developers (see AUTHORS file)

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

#include <algorithm>

#include "types.h"
#include "bitboard.h"

#include "offset.h"

namespace PSQT {

#define S(mg, eg) make_score(mg, eg)

// Bonus[PieceType][Square / 2] contains Piece-Square scores. For each piece
// type on a given square a (middlegame, endgame) score pair is assigned. Table
// is defined for files A..D and white side: it is symmetric for black side and
// second half of the files.
constexpr Score Bonus[][RANK_NB][int(FILE_NB) / 2] = {
  { },
  { },
  { // Knight
   { S(-175, -96) - mobOffset(KNIGHT), S(-92,-65) - mobOffset(KNIGHT), S(-74,-49) - mobOffset(KNIGHT), S(-73,-21) - mobOffset(KNIGHT) },
   { S( -77, -67) - mobOffset(KNIGHT), S(-41,-54) - mobOffset(KNIGHT), S(-27,-18) - mobOffset(KNIGHT), S(-15,  8) - mobOffset(KNIGHT) },
   { S( -61, -40) - mobOffset(KNIGHT), S(-17,-27) - mobOffset(KNIGHT), S(  6, -8) - mobOffset(KNIGHT), S( 12, 29) - mobOffset(KNIGHT) },
   { S( -35, -35) - mobOffset(KNIGHT), S(  8, -2) - mobOffset(KNIGHT), S( 40, 13) - mobOffset(KNIGHT), S( 49, 28) - mobOffset(KNIGHT) },
   { S( -34, -45) - mobOffset(KNIGHT), S( 13,-16) - mobOffset(KNIGHT), S( 44,  9) - mobOffset(KNIGHT), S( 51, 39) - mobOffset(KNIGHT) },
   { S(  -9, -51) - mobOffset(KNIGHT), S( 22,-44) - mobOffset(KNIGHT), S( 58,-16) - mobOffset(KNIGHT), S( 53, 17) - mobOffset(KNIGHT) },
   { S( -67, -69) - mobOffset(KNIGHT), S(-27,-50) - mobOffset(KNIGHT), S(  4,-51) - mobOffset(KNIGHT), S( 37, 12) - mobOffset(KNIGHT) },
   { S(-201,-100) - mobOffset(KNIGHT), S(-83,-88) - mobOffset(KNIGHT), S(-56,-56) - mobOffset(KNIGHT), S(-26,-17) - mobOffset(KNIGHT) }
  },
  { // Bishop
   { S(-37,-40) - mobOffset(BISHOP), S(-4 ,-21) - mobOffset(BISHOP), S( -6,-26) - mobOffset(BISHOP), S(-16, -8) - mobOffset(BISHOP) },
   { S(-11,-26) - mobOffset(BISHOP), S(  6, -9) - mobOffset(BISHOP), S( 13,-12) - mobOffset(BISHOP), S(  3,  1) - mobOffset(BISHOP) },
   { S(-5 ,-11) - mobOffset(BISHOP), S( 15, -1) - mobOffset(BISHOP), S( -4, -1) - mobOffset(BISHOP), S( 12,  7) - mobOffset(BISHOP) },
   { S(-4 ,-14) - mobOffset(BISHOP), S(  8, -4) - mobOffset(BISHOP), S( 18,  0) - mobOffset(BISHOP), S( 27, 12) - mobOffset(BISHOP) },
   { S(-8 ,-12) - mobOffset(BISHOP), S( 20, -1) - mobOffset(BISHOP), S( 15,-10) - mobOffset(BISHOP), S( 22, 11) - mobOffset(BISHOP) },
   { S(-11,-21) - mobOffset(BISHOP), S(  4,  4) - mobOffset(BISHOP), S(  1,  3) - mobOffset(BISHOP), S(  8,  4) - mobOffset(BISHOP) },
   { S(-12,-22) - mobOffset(BISHOP), S(-10,-14) - mobOffset(BISHOP), S(  4, -1) - mobOffset(BISHOP), S(  0,  1) - mobOffset(BISHOP) },
   { S(-34,-32) - mobOffset(BISHOP), S(  1,-29) - mobOffset(BISHOP), S(-10,-26) - mobOffset(BISHOP), S(-16,-17) - mobOffset(BISHOP) }
  },
  { // Rook
   { S(-31, -9) - mobOffset(ROOK), S(-20,-13) - mobOffset(ROOK), S(-14,-10) - mobOffset(ROOK), S(-5, -9) - mobOffset(ROOK) },
   { S(-21,-12) - mobOffset(ROOK), S(-13, -9) - mobOffset(ROOK), S( -8, -1) - mobOffset(ROOK), S( 6, -2) - mobOffset(ROOK) },
   { S(-25,  6) - mobOffset(ROOK), S(-11, -8) - mobOffset(ROOK), S( -1, -2) - mobOffset(ROOK), S( 3, -6) - mobOffset(ROOK) },
   { S(-13, -6) - mobOffset(ROOK), S( -5,  1) - mobOffset(ROOK), S( -4, -9) - mobOffset(ROOK), S(-6,  7) - mobOffset(ROOK) },
   { S(-27, -5) - mobOffset(ROOK), S(-15,  8) - mobOffset(ROOK), S( -4,  7) - mobOffset(ROOK), S( 3, -6) - mobOffset(ROOK) },
   { S(-22,  6) - mobOffset(ROOK), S( -2,  1) - mobOffset(ROOK), S(  6, -7) - mobOffset(ROOK), S(12, 10) - mobOffset(ROOK) },
   { S( -2,  4) - mobOffset(ROOK), S( 12,  5) - mobOffset(ROOK), S( 16, 20) - mobOffset(ROOK), S(18, -5) - mobOffset(ROOK) },
   { S(-17, 18) - mobOffset(ROOK), S(-19,  0) - mobOffset(ROOK), S( -1, 19) - mobOffset(ROOK), S( 9, 13) - mobOffset(ROOK) }
  },
  { // Queen
   { S( 3,-69) - mobOffset(QUEEN), S(-5,-57) - mobOffset(QUEEN), S(-5,-47) - mobOffset(QUEEN), S( 4,-26) - mobOffset(QUEEN) },
   { S(-3,-55) - mobOffset(QUEEN), S( 5,-31) - mobOffset(QUEEN), S( 8,-22) - mobOffset(QUEEN), S(12, -4) - mobOffset(QUEEN) },
   { S(-3,-39) - mobOffset(QUEEN), S( 6,-18) - mobOffset(QUEEN), S(13, -9) - mobOffset(QUEEN), S( 7,  3) - mobOffset(QUEEN) },
   { S( 4,-23) - mobOffset(QUEEN), S( 5, -3) - mobOffset(QUEEN), S( 9, 13) - mobOffset(QUEEN), S( 8, 24) - mobOffset(QUEEN) },
   { S( 0,-29) - mobOffset(QUEEN), S(14, -6) - mobOffset(QUEEN), S(12,  9) - mobOffset(QUEEN), S( 5, 21) - mobOffset(QUEEN) },
   { S(-4,-38) - mobOffset(QUEEN), S(10,-18) - mobOffset(QUEEN), S( 6,-12) - mobOffset(QUEEN), S( 8,  1) - mobOffset(QUEEN) },
   { S(-5,-50) - mobOffset(QUEEN), S( 6,-27) - mobOffset(QUEEN), S(10,-24) - mobOffset(QUEEN), S( 8, -8) - mobOffset(QUEEN) },
   { S(-2,-75) - mobOffset(QUEEN), S(-2,-52) - mobOffset(QUEEN), S( 1,-43) - mobOffset(QUEEN), S(-2,-36) - mobOffset(QUEEN) }
  },
  { // King
   { S(271,  1), S(327, 45), S(271, 85), S(198, 76) },
   { S(278, 53), S(303,100), S(234,133), S(179,135) },
   { S(195, 88), S(258,130), S(169,169), S(120,175) },
   { S(164,103), S(190,156), S(138,172), S( 98,172) },
   { S(154, 96), S(179,166), S(105,199), S( 70,199) },
   { S(123, 92), S(145,172), S( 81,184), S( 31,191) },
   { S( 88, 47), S(120,121), S( 65,116), S( 33,131) },
   { S( 59, 11), S( 89, 59), S( 45, 73), S( -1, 78) }
  }
};

constexpr Score PBonus[RANK_NB][FILE_NB] =
  { // Pawn (asymmetric distribution)
   { },
   { S(  3,-10), S(  3, -6), S( 10, 10), S( 19,  0), S( 16, 14), S( 19,  7), S(  7, -5), S( -5,-19) },
   { S( -9,-10), S(-15,-10), S( 11,-10), S( 15,  4), S( 32,  4), S( 22,  3), S(  5, -6), S(-22, -4) },
   { S( -4,  6), S(-23, -2), S(  6, -8), S( 20, -4), S( 40,-13), S( 17,-12), S(  4,-10), S( -8, -9) },
   { S( 13, 10), S(  0,  5), S(-13,  4), S(  1, -5), S( 11, -5), S( -2, -5), S(-13, 14), S(  5,  9) },
   { S(  5, 28), S(-12, 20), S( -7, 21), S( 22, 28), S( -8, 30), S( -5,  7), S(-15,  6), S( -8, 13) },
   { S( -7,  0), S(  7,-11), S( -3, 12), S(-13, 21), S(  5, 25), S(-16, 19), S( 10,  4), S( -8,  7) }
  };

#undef S

Score psq[PIECE_NB][SQUARE_NB];


// PSQT::init() initializes piece-square tables: the white halves of the tables are
// copied from Bonus[] and PBonus[], adding the piece value, then the black halves of
// the tables are initialized by flipping and changing the sign of the white scores.
void init() {

  for (Piece pc : {W_PAWN, W_KNIGHT, W_BISHOP, W_ROOK, W_QUEEN, W_KING})
  {
      Score score = make_score(PieceValue[MG][pc], PieceValue[EG][pc]);

      for (Square s = SQ_A1; s <= SQ_H8; ++s)
      {
          File f = File(edge_distance(file_of(s)));
          psq[ pc][s] = score + (type_of(pc) == PAWN ? PBonus[rank_of(s)][file_of(s)]
                                                     : Bonus[pc][rank_of(s)][f]);
          psq[~pc][flip_rank(s)] = -psq[pc][s];
      }
  }
}

} // namespace PSQT
