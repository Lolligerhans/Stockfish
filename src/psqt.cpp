/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2008 Tord Romstad (Glaurung author)
  Copyright (C) 2008-2015 Marco Costalba, Joona Kiiski, Tord Romstad
  Copyright (C) 2015-2020 Marco Costalba, Joona Kiiski, Gary Linscott, Tord Romstad

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

Value PieceValue[PHASE_NB][PIECE_NB] = {
  { VALUE_ZERO, PawnValueMg, KnightValueMg, BishopValueMg, RookValueMg, QueenValueMg },
  { VALUE_ZERO, PawnValueEg, KnightValueEg, BishopValueEg, RookValueEg, QueenValueEg }
};

namespace PSQT {

#define S(mg, eg) make_score(mg, eg)

// Bonus[PieceType][Square / 2] contains Piece-Square scores. For each piece
// type on a given square a (middlegame, endgame) score pair is assigned. Table
// is defined for files A..D and white side: it is symmetric for black side and
// second half of the files.
constexpr Score Bonus[][RANK_NB][FILE_NB] = {
  { },
  { },
  { // Knight
   { S(-175, -94), S( -95, -64), S( -76, -50), S( -74, -21), S( -72, -20), S( -72, -50), S( -90, -66), S(-175, -94) },
   { S( -77, -69), S( -39, -54), S( -28, -18), S( -15,   8), S( -16,   8), S( -25, -18), S( -42, -51), S( -77, -67) },
   { S( -61, -41), S( -17, -25), S(   5,  -7), S(  11,  27), S(  11,  27), S(   6,  -8), S( -17, -27), S( -61, -40) },
   { S( -36, -36), S(   8,  -2), S(  39,  13), S(  48,  28), S(  48,  28), S(  41,  14), S(  10,  -1), S( -35, -35) },
   { S( -34, -45), S(  14, -15), S(  45,   9), S(  49,  38), S(  49,  38), S(  46,   9), S(  13, -16), S( -33, -46) },
   { S(  -9, -51), S(  22, -44), S(  58, -15), S(  52,  16), S(  53,  18), S(  63, -16), S(  22, -44), S(  -8, -53) },
   { S( -64, -70), S( -26, -49), S(   4, -51), S(  36,  11), S(  38,  13), S(   5, -52), S( -27, -50), S( -68, -70) },
   { S(-199,-102), S( -82, -85), S( -55, -54), S( -27, -17), S( -27, -17), S( -57, -57), S( -82, -89), S(-202, -99) },
  },
  { // Bishop
   { S( -52, -57), S(  -5, -30), S(  -8, -37), S( -24, -13), S( -24, -12), S(  -8, -36), S(  -6, -30), S( -53, -58) },
   { S( -14, -37), S(   9, -12), S(  20, -17), S(   4,   0), S(   3,   1), S(  19, -17), S(   9, -14), S( -15, -37) },
   { S(  -8, -16), S(  20,  -1), S(  -6,  -1), S(  17,  12), S(  17,  11), S(  -7,  -2), S(  21,   0), S(  -9, -16) },
   { S(  -4, -20), S(  12,  -6), S(  25,   0), S(  40,  16), S(  39,  17), S(  25,   1), S(  12,  -7), S(  -5, -20) },
   { S( -12, -17), S(  28,  -1), S(  22, -15), S(  31,  15), S(  31,  14), S(  22, -15), S(  30,  -2), S( -13, -17) },
   { S( -16, -29), S(   6,   6), S(   0,   5), S(  11,   6), S(  12,   5), S(   1,   4), S(   7,   4), S( -17, -30) },
   { S( -17, -32), S( -14, -20), S(   5,   0), S(   1,   2), S(  -1,   1), S(   5,   0), S( -14, -21), S( -18, -31) },
   { S( -50, -46), S(   1, -44), S( -16, -37), S( -23, -23), S( -23, -24), S( -13, -39), S(   1, -42), S( -49, -46) },
  },
  { // Rook
   { S( -32,  -9), S( -21, -13), S( -14, -10), S(  -5,  -9), S(  -5, -10), S( -14, -10), S( -20, -12), S( -32, -10) },
   { S( -22, -12), S( -13,  -8), S(  -9,  -1), S(   6,  -2), S(   7,  -1), S(  -8,  -2), S( -13,  -8), S( -20, -12) },
   { S( -25,   6), S( -11,  -7), S(   0,  -1), S(   3,  -5), S(   2,  -5), S(   0,  -2), S( -11,  -8), S( -25,   6) },
   { S( -13,  -7), S(  -6,   0), S(  -4,  -9), S(  -6,   8), S(  -7,   8), S(  -4,  -9), S(  -5,   1), S( -14,  -6) },
   { S( -27,  -5), S( -15,   8), S(  -5,   7), S(   2,  -7), S(   3,  -6), S(  -5,   7), S( -14,   7), S( -27,  -5) },
   { S( -23,   6), S(  -4,   1), S(   7,  -6), S(  11,  10), S(  13,   9), S(   7,  -6), S(  -3,   1), S( -23,   7) },
   { S(  -1,   3), S(  10,   4), S(  17,  20), S(  18,  -4), S(  18,  -5), S(  16,  20), S(  12,   5), S(   0,   5) },
   { S( -16,  18), S( -18,   0), S(  -2,  19), S(   8,  12), S(   9,  13), S(  -1,  19), S( -19,  -2), S( -17,  17) },
  },
  { // Queen
   { S(   5, -66), S(  -6, -56), S(  -5, -47), S(   4, -25), S(   3, -26), S(  -5, -47), S(  -6, -57), S(   3, -67) },
   { S(  -3, -51), S(   6, -30), S(   8, -23), S(  13,  -4), S(  12,  -5), S(   8, -23), S(   4, -30), S(  -3, -57) },
   { S(  -3, -38), S(   6, -18), S(  12, -10), S(   8,   2), S(   7,   4), S(  13, -10), S(   6, -18), S(  -3, -38) },
   { S(   3, -23), S(   4,  -2), S(   8,  13), S(   7,  24), S(   7,  24), S(   9,  12), S(   6,  -3), S(   4, -23) },
   { S(  -1, -30), S(  13,  -7), S(  12,  10), S(   5,  20), S(   6,  20), S(  12,   9), S(  16,  -7), S(   0, -29) },
   { S(  -5, -39), S(  10, -18), S(   5, -12), S(   8,   1), S(   7,   1), S(   6, -13), S(  10, -18), S(  -4, -38) },
   { S(  -5, -49), S(   8, -27), S(  10, -25), S(   8,  -7), S(   7,  -7), S(  10, -24), S(   7, -27), S(  -4, -49) },
   { S(   0, -74), S(  -1, -52), S(   0, -43), S(  -1, -36), S(  -2, -37), S(   1, -43), S(  -2, -51), S(  -2, -72) },
  },
  { // King
   { S( 268,   1), S( 322,  43), S( 275,  82), S( 201,  77), S( 187,  76), S( 269,  86), S( 324,  46), S( 269,   2) },
   { S( 280,  52), S( 305, 104), S( 237, 133), S( 176, 137), S( 177, 138), S( 232, 134), S( 305, 102), S( 271,  52) },
   { S( 196,  88), S( 252, 127), S( 170, 173), S( 119, 174), S( 120, 179), S( 172, 174), S( 263, 132), S( 188,  89) },
   { S( 165, 103), S( 190, 153), S( 138, 172), S(  99, 175), S(  96, 171), S( 135, 171), S( 188, 155), S( 162, 100) },
   { S( 154,  96), S( 177, 165), S( 107, 202), S(  73, 186), S(  69, 201), S( 101, 199), S( 172, 173), S( 161,  94) },
   { S( 123,  96), S( 147, 175), S(  85, 183), S(  32, 201), S(  31, 193), S(  80, 182), S( 155, 175), S( 122,  95) },
   { S(  90,  50), S( 122, 125), S(  65, 117), S(  33, 130), S(  34, 132), S(  62, 112), S( 115, 119), S(  89,  47) },
   { S(  58,  11), S(  89,  61), S(  46,  76), S(  -3,  79), S(  -3,  74), S(  45,  71), S(  87,  60), S(  58,  11) },
  }
};

constexpr Score PBonus[RANK_NB][FILE_NB] =
  { // Pawn (asymmetric distribution)
   { },
   { S(   3, -10), S(   3,  -6), S(   9,  10), S(  19,   0), S(  16,  14), S(  18,   8), S(   7,  -5), S(  -4, -19) },
   { S(  -9, -10), S( -15, -10), S(  11, -10), S(  15,   5), S(  32,   3), S(  22,   2), S(   6,  -6), S( -22,  -4) },
   { S( -10,   6), S( -24,  -2), S(   7,  -8), S(  19,  -5), S(  41, -13), S(  17, -12), S(   4, -11), S( -11,  -9) },
   { S(  13,   9), S(   0,   3), S( -14,   3), S(   2, -11), S(  10, -13), S(  -1,  -5), S( -13,  13), S(   5,   7) },
   { S(  -6,  29), S( -12,  21), S(  -7,  20), S(  22,  28), S(  -8,  29), S(  -5,   7), S( -15,   5), S( -18,  13) },
   { S(  -6,  -1), S(   7,  -9), S(  -3,  12), S( -13,  20), S(   5,  25), S( -17,  20), S(  10,   4), S(  -8,   8) },
  };

#undef S

Score psq[PIECE_NB][SQUARE_NB];

// init() initializes piece-square tables: the white halves of the tables are
// copied from Bonus[] adding the piece value, then the black halves of the
// tables are initialized by flipping and changing the sign of the white scores.
void init() {

  for (Piece pc = W_PAWN; pc <= W_KING; ++pc)
  {
      PieceValue[MG][~pc] = PieceValue[MG][pc];
      PieceValue[EG][~pc] = PieceValue[EG][pc];

      Score score = make_score(PieceValue[MG][pc], PieceValue[EG][pc]);

      for (Square s = SQ_A1; s <= SQ_H8; ++s)
      {
          const File f = file_of(s);
          const Rank r = rank_of(s);
          psq[ pc][ s] = score + (type_of(pc) == PAWN ? PBonus : Bonus[pc])[r][f];
          psq[~pc][~s] = -psq[pc][s];
      }
  }
}

} // namespace PSQT
