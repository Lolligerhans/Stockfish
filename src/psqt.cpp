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
   { S(-177,  -92), S( -96,  -65), S( -77,  -50), S( -72,  -21), S( -71,  -20), S( -71,  -49), S( -91,  -65), S(-173,  -95) },
   { S( -78,  -69), S( -39,  -53), S( -27,  -18), S( -15,    8), S( -16,    7), S( -26,  -18), S( -42,  -52), S( -78,  -67) },
   { S( -61,  -41), S( -17,  -25), S(   5,   -7), S(  11,   28), S(  12,   28), S(   5,   -8), S( -17,  -27), S( -61,  -39) },
   { S( -36,  -36), S(   8,   -1), S(  39,   14), S(  48,   28), S(  49,   28), S(  41,   14), S(   9,   -2), S( -35,  -36) },
   { S( -34,  -45), S(  14,  -16), S(  46,    9), S(  49,   38), S(  49,   38), S(  46,    9), S(  12,  -15), S( -34,  -46) },
   { S( -10,  -51), S(  22,  -44), S(  58,  -16), S(  52,   16), S(  52,   18), S(  63,  -16), S(  22,  -45), S(  -8,  -52) },
   { S( -64,  -70), S( -26,  -49), S(   4,  -51), S(  36,   11), S(  38,   12), S(   4,  -51), S( -27,  -51), S( -68,  -71) },
   { S(-200,  -99), S( -83,  -87), S( -54,  -54), S( -26,  -17), S( -26,  -17), S( -57,  -57), S( -82,  -87), S(-202,  -99) }
  },
  { // Bishop
   { S( -51,  -56), S(  -5,  -29), S(  -8,  -36), S( -23,  -13), S( -23,  -12), S(  -8,  -37), S(  -6,  -30), S( -52,  -58) },
   { S( -15,  -37), S(   9,  -13), S(  19,  -17), S(   4,    1), S(   3,    2), S(  19,  -16), S(   9,  -14), S( -15,  -37) },
   { S(  -7,  -16), S(  20,   -2), S(  -6,   -1), S(  17,   11), S(  17,   10), S(  -6,   -2), S(  21,   -1), S(  -9,  -16) },
   { S(  -4,  -20), S(  12,   -6), S(  25,    0), S(  40,   16), S(  38,   17), S(  25,    0), S(  11,   -7), S(  -5,  -20) },
   { S( -13,  -17), S(  28,   -2), S(  22,  -15), S(  31,   15), S(  31,   15), S(  22,  -15), S(  30,   -1), S( -13,  -16) },
   { S( -16,  -29), S(   7,    6), S(   0,    5), S(  11,    6), S(  13,    6), S(   1,    4), S(   7,    5), S( -17,  -30) },
   { S( -17,  -32), S( -14,  -20), S(   4,    0), S(   1,    2), S(   0,    1), S(   5,    0), S( -15,  -21), S( -18,  -30) },
   { S( -49,  -46), S(   1,  -43), S( -15,  -38), S( -23,  -23), S( -23,  -24), S( -14,  -38), S(   1,  -41), S( -48,  -45) }
  },
  { // Rook
   { S( -32,   -9), S( -20,  -14), S( -14,  -10), S(  -4,   -8), S(  -5,  -10), S( -14,   -9), S( -20,  -12), S( -31,  -10) },
   { S( -22,  -12), S( -13,   -8), S(  -9,   -1), S(   6,   -2), S(   7,   -2), S(  -8,   -2), S( -13,   -9), S( -21,  -12) },
   { S( -26,    6), S( -10,   -8), S(  -1,   -1), S(   3,   -5), S(   2,   -5), S(   0,   -3), S( -11,   -8), S( -24,    6) },
   { S( -13,   -7), S(  -6,    1), S(  -4,   -9), S(  -6,    7), S(  -7,    8), S(  -3,   -9), S(  -5,    1), S( -14,   -6) },
   { S( -27,   -6), S( -15,    8), S(  -5,    6), S(   2,   -7), S(   3,   -6), S(  -5,    6), S( -15,    6), S( -27,   -5) },
   { S( -23,    6), S(  -3,    1), S(   7,   -7), S(  11,   10), S(  13,    9), S(   7,   -6), S(  -2,    0), S( -23,    6) },
   { S(  -1,    3), S(  10,    4), S(  16,   20), S(  18,   -4), S(  18,   -6), S(  16,   20), S(  12,    5), S(  -1,    4) },
   { S( -16,   18), S( -19,    0), S(  -1,   19), S(   8,   13), S(   9,   13), S(  -1,   20), S( -19,   -1), S( -17,   17) }
  },
  { // Queen
   { S(   4,  -66), S(  -6,  -55), S(  -4,  -47), S(   4,  -26), S(   4,  -26), S(  -5,  -47), S(  -6,  -58), S(   2,  -67) },
   { S(  -3,  -51), S(   6,  -30), S(   8,  -22), S(  12,   -4), S(  12,   -5), S(   8,  -22), S(   4,  -31), S(  -3,  -55) },
   { S(  -3,  -39), S(   6,  -18), S(  12,  -10), S(   8,    3), S(   7,    4), S(  13,   -9), S(   5,  -18), S(  -4,  -38) },
   { S(   4,  -23), S(   5,   -2), S(   8,   13), S(   7,   23), S(   8,   24), S(  10,   12), S(   6,   -3), S(   3,  -23) },
   { S(   0,  -29), S(  13,   -6), S(  12,   10), S(   5,   21), S(   5,   20), S(  12,    9), S(  15,   -7), S(   1,  -29) },
   { S(  -5,  -39), S(  10,  -18), S(   5,  -13), S(   8,    1), S(   7,    1), S(   6,  -13), S(  10,  -18), S(  -4,  -38) },
   { S(  -5,  -50), S(   7,  -27), S(   9,  -25), S(   8,   -8), S(   7,   -7), S(  10,  -24), S(   7,  -27), S(  -4,  -49) },
   { S(  -1,  -75), S(  -1,  -53), S(   0,  -42), S(  -2,  -35), S(  -2,  -36), S(   1,  -45), S(  -2,  -51), S(  -2,  -72) }
  },
  { // King
   { S( 269,    0), S( 327,   44), S( 269,   84), S( 204,   75), S( 188,   75), S( 269,   88), S( 329,   46), S( 274,    2) },
   { S( 280,   51), S( 299,  104), S( 239,  135), S( 179,  135), S( 178,  137), S( 229,  135), S( 299,   99), S( 277,   52) },
   { S( 195,   88), S( 251,  129), S( 169,  174), S( 121,  171), S( 119,  177), S( 173,  172), S( 261,  134), S( 189,   90) },
   { S( 160,  101), S( 191,  154), S( 139,  171), S( 100,  173), S(  96,  169), S( 134,  170), S( 188,  155), S( 160,  101) },
   { S( 154,   94), S( 177,  166), S( 107,  195), S(  73,  185), S(  69,  203), S( 102,  198), S( 178,  172), S( 163,   95) },
   { S( 122,   97), S( 148,  173), S(  83,  186), S(  32,  199), S(  31,  191), S(  81,  186), S( 152,  174), S( 123,   93) },
   { S(  90,   50), S( 119,  123), S(  66,  117), S(  34,  131), S(  34,  134), S(  63,  114), S( 116,  117), S(  88,   47) },
   { S(  58,   11), S(  89,   61), S(  46,   76), S(  -2,   78), S(  -2,   76), S(  45,   70), S(  89,   61), S(  58,   11) }
  }
};

constexpr Score PBonus[RANK_NB][FILE_NB] =
  { // Pawn (asymmetric distribution)
   { },
   { S(   3,  -10), S(   3,   -6), S(   9,   10), S(  19,    1), S(  16,   13), S(  18,    8), S(   7,   -4), S(  -5,  -19) },
   { S(  -9,  -10), S( -15,  -10), S(  11,  -11), S(  15,    4), S(  32,    4), S(  22,    3), S(   6,   -6), S( -22,   -4) },
   { S(  -9,    6), S( -24,   -3), S(   7,   -8), S(  20,   -5), S(  41,  -13), S(  17,  -12), S(   4,  -10), S( -12,   -9) },
   { S(  13,    9), S(   0,    3), S( -14,    2), S(   2,  -11), S(  10,  -12), S(  -1,   -5), S( -14,   13), S(   6,    8) },
   { S(  -7,   29), S( -12,   21), S(  -7,   20), S(  22,   28), S(  -8,   30), S(  -5,    6), S( -15,    6), S( -18,   14) },
   { S(  -6,   -1), S(   8,  -10), S(  -2,   13), S( -13,   20), S(   5,   25), S( -17,   20), S(  10,    4), S(  -8,    7) }
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
