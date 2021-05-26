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


#include "psqt.h"

#include <algorithm>

#include "bitboard.h"
#include "types.h"

namespace Stockfish {

namespace
{

auto constexpr S = make_score;

// 'Bonus' contains Piece-Square parameters.
// Scores are explicit for files A to D, implicitly mirrored for E to H.
QScore Bonus[][RANK_NB][int(FILE_NB) / 2] = {
  { },
  { },
  { // Knight
   { Q(-175, -96), Q(-92,-65), Q(-74,-49), Q(-73,-21) },
   { Q( -77, -67), Q(-41,-54), Q(-27,-18), Q(-15,  8) },
   { Q( -61, -40), Q(-17,-27), Q(  6, -8), Q( 12, 29) },
   { Q( -35, -35), Q(  8, -2), Q( 40, 13), Q( 49, 28) },
   { Q( -34, -45), Q( 13,-16), Q( 44,  9), Q( 51, 39) },
   { Q(  -9, -51), Q( 22,-44), Q( 58,-16), Q( 53, 17) },
   { Q( -67, -69), Q(-27,-50), Q(  4,-51), Q( 37, 12) },
   { Q(-201,-100), Q(-83,-88), Q(-56,-56), Q(-26,-17) }
  },
  { // Bishop
   { Q(-37,-40), Q(-4 ,-21), Q( -6,-26), Q(-16, -8) },
   { Q(-11,-26), Q(  6, -9), Q( 13,-12), Q(  3,  1) },
   { Q(-5 ,-11), Q( 15, -1), Q( -4, -1), Q( 12,  7) },
   { Q(-4 ,-14), Q(  8, -4), Q( 18,  0), Q( 27, 12) },
   { Q(-8 ,-12), Q( 20, -1), Q( 15,-10), Q( 22, 11) },
   { Q(-11,-21), Q(  4,  4), Q(  1,  3), Q(  8,  4) },
   { Q(-12,-22), Q(-10,-14), Q(  4, -1), Q(  0,  1) },
   { Q(-34,-32), Q(  1,-29), Q(-10,-26), Q(-16,-17) }
  },
  { // Rook
   { Q(-31, -9), Q(-20,-13), Q(-14,-10), Q(-5, -9) },
   { Q(-21,-12), Q(-13, -9), Q( -8, -1), Q( 6, -2) },
   { Q(-25,  6), Q(-11, -8), Q( -1, -2), Q( 3, -6) },
   { Q(-13, -6), Q( -5,  1), Q( -4, -9), Q(-6,  7) },
   { Q(-27, -5), Q(-15,  8), Q( -4,  7), Q( 3, -6) },
   { Q(-22,  6), Q( -2,  1), Q(  6, -7), Q(12, 10) },
   { Q( -2,  4), Q( 12,  5), Q( 16, 20), Q(18, -5) },
   { Q(-17, 18), Q(-19,  0), Q( -1, 19), Q( 9, 13) }
  },
  { // Queen
   { Q( 3,-69), Q(-5,-57), Q(-5,-47), Q( 4,-26) },
   { Q(-3,-54), Q( 5,-31), Q( 8,-22), Q(12, -4) },
   { Q(-3,-39), Q( 6,-18), Q(13, -9), Q( 7,  3) },
   { Q( 4,-23), Q( 5, -3), Q( 9, 13), Q( 8, 24) },
   { Q( 0,-29), Q(14, -6), Q(12,  9), Q( 5, 21) },
   { Q(-4,-38), Q(10,-18), Q( 6,-11), Q( 8,  1) },
   { Q(-5,-50), Q( 6,-27), Q(10,-24), Q( 8, -8) },
   { Q(-2,-74), Q(-2,-52), Q( 1,-43), Q(-2,-34) }
  },
  { // King
   { Q(271,  1), Q(327, 45), Q(271, 85), Q(198, 76) },
   { Q(278, 53), Q(303,100), Q(234,133), Q(179,135) },
   { Q(195, 88), Q(258,130), Q(169,169), Q(120,175) },
   { Q(164,103), Q(190,156), Q(138,172), Q( 98,172) },
   { Q(154, 96), Q(179,166), Q(105,199), Q( 70,199) },
   { Q(123, 92), Q(145,172), Q( 81,184), Q( 31,191) },
   { Q( 88, 47), Q(120,121), Q( 65,116), Q( 33,131) },
   { Q( 59, 11), Q( 89, 59), Q( 45, 73), Q( -1, 78) }
  }
};

QScore PBonus[RANK_NB][FILE_NB] =
  { // Pawn (asymmetric distribution)
   { },
   { Q(  2, -8), Q(  4, -6), Q( 11,  9), Q( 18,  5), Q( 16, 16), Q( 21,  6), Q(  9, -6), Q( -3,-18) },
   { Q( -9, -9), Q(-15, -7), Q( 11,-10), Q( 15,  5), Q( 31,  2), Q( 23,  3), Q(  6, -8), Q(-20, -5) },
   { Q( -3,  7), Q(-20,  1), Q(  8, -8), Q( 19, -2), Q( 39,-14), Q( 17,-13), Q(  2,-11), Q( -5, -6) },
   { Q( 11, 12), Q( -4,  6), Q(-11,  2), Q(  2, -6), Q( 11, -5), Q(  0, -4), Q(-12, 14), Q(  5,  9) },
   { Q(  3, 27), Q(-11, 18), Q( -6, 19), Q( 22, 29), Q( -8, 30), Q( -5,  9), Q(-14,  8), Q(-11, 14) },
   { Q( -7, -1), Q(  6,-14), Q( -2, 13), Q(-11, 22), Q(  4, 24), Q(-14, 17), Q( 10,  7), Q( -9,  7) }
  };
  
TUNE(SetRange(-300,350), Bonus, PSQT::init);
TUNE(SetRange(-40,50), PBonus, PSQT::init);

} // namespace


namespace PSQT
{

QScore psq[PIECE_NB][SQUARE_NB];

// PSQT::init() initializes piece-square tables: the white halves of the tables are
// copied from Bonus[] and PBonus[], adding the piece value, then the black halves of
// the tables are initialized by flipping and changing the sign of the white scores.
void init() {

  for (Piece pc : {W_PAWN, W_KNIGHT, W_BISHOP, W_ROOK, W_QUEEN, W_KING})
  {
    QScore score = Q(PieceValue[MG][pc], PieceValue[EG][pc]);

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

} // namespace Stockfish
