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
{ Q(-175,-94,1,2),	 Q(-91,-65,5,6),	 Q(-73,-51,4,-2),	 Q(-68,-22,-1,0) },
{ Q(-74,-67,-1,-2),	 Q(-42,-55,1,-2),	 Q(-28,-16,-1,-6),	 Q(-16,9,2,0) },
{ Q(-61,-39,3,2),	 Q(-12,-29,3,1),	 Q(6,-8,-1,0),	 Q(11,30,3,-6) },
{ Q(-37,-35,-2,3),	 Q(8,-4,-1,1),	 Q(40,12,-6,1),	 Q(48,27,-3,2) },
{ Q(-39,3,2,-12),	 Q(-29,3,1,6),	 Q(-8,-1,0,11),	 Q(30,3,-6,-37) },
{ Q(-12,-50,-1,10),	 Q(22,-44,-1,1),	 Q(60,-15,0,3),	 Q(53,17,1,2) },
{ Q(-68,-71,-3,4),	 Q(-22,-46,-3,3),	 Q(5,-53,-2,1),	 Q(35,12,-6,1) },
{ Q(-199,-97,-1,-3),	 Q(-81,-82,-5,-1),	 Q(-52,-57,7,-7),	 Q(-25,-21,-1,0) }
  },
  { // Bishop
{ Q(-36,-39,-4,-1),	 Q(-3,-21,1,3),	 Q(-7,-25,2,1),	 Q(-15,-9,1,-2) },
{ Q(-14,-26,0,-5),	 Q(4,-7,1,3),	 Q(14,-11,-1,1),	 Q(2,1,1,0) },
{ Q(-4,-12,0,0),	 Q(15,-2,-3,1),	 Q(-5,-2,-3,3),	 Q(12,7,3,0) },
{ Q(-5,-12,1,1),	 Q(7,-4,-1,-3),	 Q(18,0,0,2),	 Q(27,12,3,-2) },
{ Q(-12,0,0,15),	 Q(-2,-3,1,-5),	 Q(-2,-3,3,12),	 Q(7,3,0,-5) },
{ Q(-12,-21,0,1),	 Q(3,3,1,-5),	 Q(0,2,-2,0),	 Q(8,3,-1,0) },
{ Q(-13,-23,-1,-1),	 Q(-12,-15,1,2),	 Q(3,0,5,1),	 Q(-1,1,2,1) },
{ Q(-31,-31,4,1),	 Q(1,-28,1,2),	 Q(-9,-27,1,-2),	 Q(-16,-16,0,2) }
  },
  { // Rook
{ Q(-30,-7,2,-4),	 Q(-19,-11,1,-5),	 Q(-14,-11,0,-2),	 Q(-4,-9,-2,-2) },
{ Q(-21,-11,0,-2),	 Q(-13,-10,2,-2),	 Q(-7,-1,0,0),	 Q(6,-2,-1,0) },
{ Q(-25,7,1,-3),	 Q(-10,-7,-1,-2),	 Q(-3,-1,1,1),	 Q(5,-4,0,3) },
{ Q(-14,-6,3,2),	 Q(-5,0,1,1),	 Q(-5,-8,0,-1),	 Q(-6,8,4,2) },
{ Q(7,1,-3,-10),	 Q(-7,-1,-2,-3),	 Q(-1,1,1,5),	 Q(-4,0,3,-14) },
{ Q(-20,7,-1,0),	 Q(-4,3,-1,0),	 Q(5,-8,0,0),	 Q(12,12,2,3) },
{ Q(-3,3,-2,3),	 Q(11,5,-3,1),	 Q(17,19,0,3),	 Q(17,-6,0,-3) },
{ Q(-18,18,-3,2),	 Q(-18,-1,3,-2),	 Q(-2,18,2,-2),	 Q(9,13,-4,2) }
  },
  { // Queen
{ Q(3,-70,3,-2),	 Q(-6,-55,-4,1),	 Q(-7,-46,8,-3),	 Q(6,-24,2,1) },
{ Q(-5,-56,1,3),	 Q(4,-33,-1,-4),	 Q(7,-20,1,1),	 Q(12,-3,1,1) },
{ Q(-3,-38,-1,-2),	 Q(7,-19,2,-2),	 Q(14,-8,0,-2),	 Q(6,3,2,1) },
{ Q(3,-23,0,-2),	 Q(5,-3,-2,0),	 Q(8,14,-1,-2),	 Q(9,20,-2,-1) },
{ Q(-38,-1,-2,7),	 Q(-19,2,-2,14),	 Q(-8,0,-2,6),	 Q(3,2,1,3) },
{ Q(0,-39,2,1),	 Q(10,-18,1,-1),	 Q(7,-10,1,1),	 Q(11,2,-2,0) },
{ Q(-5,-54,-4,-2),	 Q(3,-27,-1,2),	 Q(10,-24,-5,2),	 Q(6,-9,0,2) },
{ Q(-3,-73,-2,3),	 Q(-3,-48,-3,2),	 Q(1,-42,1,-1),	 Q(-3,-34,1,2) }
  },
  { // King
{ Q(275,-2,1,6),	 Q(328,46,1,-6),	 Q(272,84,-3,7),	 Q(193,78,5,-3) },
{ Q(279,56,-1,3),	 Q(298,106,-3,4),	 Q(238,132,-8,-5),	 Q(180,138,5,4) },
{ Q(197,92,2,4),	 Q(258,130,-11,1),	 Q(170,169,-1,-1),	 Q(124,173,-10,1) },
{ Q(162,101,-2,0),	 Q(193,156,3,6),	 Q(138,172,1,1),	 Q(91,174,-8,-4) },
{ Q(92,2,4,258),	 Q(130,-11,1,170),	 Q(169,-1,-1,124),	 Q(173,-10,1,162) },
{ Q(124,99,-1,-5),	 Q(146,173,-5,2),	 Q(81,186,-7,-7),	 Q(29,192,-4,6) },
{ Q(87,49,-6,1),	 Q(128,125,-2,-3),	 Q(68,117,-5,3),	 Q(34,130,1,0) },
{ Q(61,10,-1,2),	 Q(90,58,2,0),	 Q(47,74,8,6),	 Q(1,80,1,-1) }
  }
};

QScore PBonus[RANK_NB][FILE_NB] =
  { // Pawn (asymmetric distribution)
   { },
{ Q(4,-8,0,6),	 Q(2,-5,-1,-2),	 Q(11,7,1,0),	 Q(19,5,1,2),	 Q(17,14,-3,3),	 Q(22,7,1,-1),	 Q(9,-4,0,0),	 Q(-2,-20,1,0) },
{ Q(-9,-9,0,1),	 Q(-15,-6,1,0),	 Q(11,-8,1,0),	 Q(16,5,-3,1),	 Q(30,1,-3,3),	 Q(24,3,2,-7),	 Q(6,-7,-2,-3),	 Q(-21,-7,-3,-1) },
{ Q(-2,6,-3,4),	 Q(-19,0,1,-2),	 Q(9,-8,2,-3),	 Q(18,-3,-4,2),	 Q(38,-15,-1,0),	 Q(15,-12,0,-1),	 Q(1,-11,2,1),	 Q(-4,-5,-3,0) },
{ Q(13,13,1,-1),	 Q(-3,5,1,2),	 Q(-11,2,-2,0),	 Q(1,-8,-1,0),	 Q(13,-5,0,-2),	 Q(0,-5,2,0),	 Q(-12,14,-2,2),	 Q(4,9,-2,0) },
{ Q(2,26,1,-1),	 Q(-12,16,-6,3),	 Q(-5,20,1,0),	 Q(21,29,3,-1),	 Q(-9,33,0,0),	 Q(-4,10,1,-1),	 Q(-15,9,1,-4),	 Q(-12,14,5,2) },
{ Q(-7,0,-2,3),	 Q(6,-15,0,1),	 Q(-2,16,2,-3),	 Q(-11,24,-1,-1),	 Q(2,23,0,-2),	 Q(-15,17,4,-2),	 Q(9,7,-1,3),	 Q(-11,7,0,-3) },
  };

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
