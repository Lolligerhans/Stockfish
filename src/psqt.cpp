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
constexpr QScore Bonus[][RANK_NB][int(FILE_NB) / 2] = {
  { },
  { },
  { // Knight
{ Q(-175,-93,1,3),	 Q(-91,-64,6,7),	 Q(-72,-52,5,-2),	 Q(-66,-23,-2,0) },
{ Q(-71,-67,-1,-2),	 Q(-42,-56,1,-2),	 Q(-28,-14,-1,-7),	 Q(-17,9,2,0) },
{ Q(-60,-39,3,2),	 Q(-8,-30,4,1),	 Q(7,-8,-1,0),	 Q(10,31,4,-7) },
{ Q(-38,-35,-2,4),	 Q(9,-6,-2,1),	 Q(40,12,-7,2),	 Q(47,26,-4,3) },
{ Q(-34,-45,3,1),	 Q(13,-15,3,1),	 Q(44,10,0,-2),	 Q(50,37,3,-4) },
{ Q(-14,-50,-2,12),	 Q(22,-44,-2,1),	 Q(61,-15,0,4),	 Q(53,17,1,2) },
{ Q(-68,-73,-3,5),	 Q(-19,-44,-4,3),	 Q(5,-54,-2,2),	 Q(33,13,-7,1) },
{ Q(-198,-95,-1,-4),	 Q(-80,-78,-6,-1),	 Q(-50,-58,8,-8),	 Q(-25,-23,-2,0) }
  },
  { // Bishop
{ Q(-35,-38,-5,-1),	 Q(-2,-20,1,4),	 Q(-8,-24,2,2),	 Q(-15,-10,2,-2) },
{ Q(-15,-26,0,-6),	 Q(2,-6,1,3),	 Q(14,-10,-2,1),	 Q(2,0,1,0) },
{ Q(-3,-12,0,0),	 Q(15,-3,-4,1),	 Q(-6,-3,-4,4),	 Q(13,7,3,0) },
{ Q(-6,-11,1,1),	 Q(7,-4,-2,-3),	 Q(18,-1,0,2),	 Q(27,12,3,-3) },
{ Q(-6,-12,-1,1),	 Q(21,-1,1,4),	 Q(17,-7,3,0),	 Q(22,12,1,0) },
{ Q(-12,-21,0,2),	 Q(2,3,1,-6),	 Q(-1,1,-3,0),	 Q(8,2,-2,0) },
{ Q(-13,-24,-2,-2),	 Q(-14,-16,1,2),	 Q(2,0,5,2),	 Q(-1,1,2,2) },
{ Q(-29,-31,5,2),	 Q(1,-28,2,3),	 Q(-9,-27,1,-3),	 Q(-16,-15,0,2) }
  },
  { // Rook
{ Q(-30,-6,3,-5),	 Q(-19,-10,2,-6),	 Q(-15,-11,0,-2),	 Q(-4,-9,-2,-3) },
{ Q(-21,-10,0,-3),	 Q(-13,-11,3,-2),	 Q(-7,-2,0,0),	 Q(6,-2,-1,-1) },
{ Q(-26,8,1,-3),	 Q(-10,-6,-1,-2),	 Q(-4,0,2,1),	 Q(6,-3,0,4) },
{ Q(-15,-6,4,2),	 Q(-5,0,1,1),	 Q(-6,-8,0,-1),	 Q(-6,8,4,3) },
{ Q(-24,-3,1,-3),	 Q(-14,5,-2,-1),	 Q(-2,5,3,0),	 Q(4,-5,-5,4) },
{ Q(-19,8,-1,0),	 Q(-5,4,-2,0),	 Q(5,-8,0,0),	 Q(11,13,2,3) },
{ Q(-3,3,-2,4),	 Q(11,4,-3,1),	 Q(17,19,0,4),	 Q(17,-6,1,-3) },
{ Q(-18,18,-3,3),	 Q(-17,-1,3,-2),	 Q(-2,18,2,-2),	 Q(9,13,-4,3) }
  },
  { // Queen
{ Q(4,-70,3,-3),	 Q(-7,-54,-5,1),	 Q(-9,-46,10,-4),	 Q(7,-23,2,1) },
{ Q(-6,-57,1,4),	 Q(3,-34,-2,-5),	 Q(7,-19,1,1),	 Q(12,-2,1,1) },
{ Q(-3,-37,-1,-3),	 Q(7,-20,2,-2),	 Q(14,-7,0,-2),	 Q(6,3,3,1) },
{ Q(2,-23,0,-2),	 Q(5,-3,-2,0),	 Q(8,15,-2,-2),	 Q(9,17,-3,-2) },
{ Q(3,-28,2,3),	 Q(15,-6,-3,-1),	 Q(11,15,-2,-2),	 Q(4,21,-3,0) },
{ Q(2,-40,3,1),	 Q(10,-18,1,-1),	 Q(8,-10,1,1),	 Q(12,2,-3,0) },
{ Q(-4,-57,-5,-3),	 Q(2,-26,-2,2),	 Q(10,-24,-6,2),	 Q(5,-9,1,2) },
{ Q(-4,-73,-2,3),	 Q(-4,-46,-4,2),	 Q(1,-41,1,-1),	 Q(-4,-35,1,2) }
  },
  { // King
{ Q(278,-5,2,7),	 Q(329,47,1,-7),	 Q(273,84,-4,9),	 Q(189,79,5,-4) },
{ Q(280,59,-1,4),	 Q(295,109,-3,5),	 Q(241,131,-10,-6),	 Q(181,140,6,5) },
{ Q(198,94,2,5),	 Q(257,130,-13,1),	 Q(170,168,-1,-2),	 Q(126,172,-12,2) },
{ Q(160,100,-2,0),	 Q(195,156,4,8),	 Q(138,173,1,2),	 Q(87,175,-10,-5) },
{ Q(155,95,-16,-10),	 Q(180,165,-6,-5),	 Q(108,200,4,-1),	 Q(68,204,6,4) },
{ Q(125,104,-1,-6),	 Q(147,174,-5,3),	 Q(80,187,-8,-8),	 Q(28,192,-5,8) },
{ Q(87,51,-7,1),	 Q(133,128,-3,-3),	 Q(70,117,-6,4),	 Q(34,130,1,0) },
{ Q(62,9,-1,2),	 Q(91,58,2,0),	 Q(48,74,10,7),	 Q(2,81,2,-1) }
  }
};

constexpr QScore PBonus[RANK_NB][FILE_NB] =
  { // Pawn (asymmetric distribution)
   { },
{ Q(5,-7,0,8),	 Q(0,-4,-1,-2),	 Q(11,6,1,0),	 Q(20,4,1,3),	 Q(17,13,-3,4),	 Q(23,7,1,-1),	 Q(9,-3,0,0),	 Q(-1,-22,1,0) },
{ Q(-9,-8,0,1),	 Q(-15,-6,1,1),	 Q(10,-7,1,0),	 Q(16,5,-4,1),	 Q(29,0,-3,4),	 Q(25,3,3,-8),	 Q(6,-7,-2,-4),	 Q(-21,-8,-3,-1) },
{ Q(-2,5,-3,4),	 Q(-19,0,1,-2),	 Q(9,-9,2,-4),	 Q(17,-3,-5,2),	 Q(38,-15,-1,0),	 Q(13,-11,-1,-1),	 Q(1,-11,3,1),	 Q(-4,-5,-3,0) },
{ Q(14,14,1,-1),	 Q(-2,5,1,2),	 Q(-10,2,-3,0),	 Q(0,-9,-1,0),	 Q(14,-4,0,-2),	 Q(0,-6,3,0),	 Q(-12,14,-2,2),	 Q(4,9,-2,0) },
{ Q(1,26,2,-1),	 Q(-12,14,-7,3),	 Q(-4,20,1,0),	 Q(20,29,3,-2),	 Q(-9,36,0,0),	 Q(-4,11,1,-1),	 Q(-15,9,1,-4),	 Q(-13,13,6,3) },
{ Q(-7,1,-3,3),	 Q(6,-15,0,1),	 Q(-3,17,2,-4),	 Q(-12,25,-1,-1),	 Q(0,23,0,-2),	 Q(-15,17,4,-3),	 Q(8,7,-1,4),	 Q(-12,7,0,-3) }
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
