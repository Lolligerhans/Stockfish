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

#include <algorithm>
#include <cassert>

#include "bitboard.h"
#include "pawns.h"
#include "position.h"
#include "thread.h"

namespace {

  #define V Value
  #define S(mg, eg) make_score(mg, eg)

  // Pawn penalties
  constexpr Score Backward = S( 9, 24);
  constexpr Score Doubled  = S(11, 56);
  constexpr Score Isolated = S( 5, 15);

  // Connected pawn bonus
  constexpr int Connected[RANK_NB] = { 0, 7, 8, 12, 29, 48, 86 };

  // Strength of pawn shelter for our king by [distance from edge][rank].
  // RANK_1 = 0 is used for files where we have no pawn, or pawn is behind our king.
  constexpr Value ShelterStrength[int(FILE_NB) / 2][RANK_NB] = {
    { V( -6), V( 81), V( 93), V( 58), V( 39), V( 18), V(  25) },
    { V(-43), V( 61), V( 35), V(-49), V(-29), V(-11), V( -63) },
    { V(-10), V( 75), V( 23), V( -2), V( 32), V(  3), V( -45) },
    { V(-39), V(-13), V(-29), V(-52), V(-48), V(-67), V(-166) }
  };

  // Danger of enemy pawns moving toward our king by [distance from edge][rank].
  // RANK_1 = 0 is used for files where the enemy has no pawn, or their pawn
  // is behind our king.
  constexpr Value UnblockedStorm[int(FILE_NB) / 2][RANK_NB] = {
    { V( 89), V(107), V(123), V(93), V(57), V( 45), V( 51) },
    { V( 44), V(-18), V(123), V(46), V(39), V( -7), V( 23) },
    { V(  4), V( 52), V(162), V(37), V( 7), V(-14), V( -2) },
    { V(-10), V(-14), V( 90), V(15), V( 2), V( -7), V(-16) }
  };

  class PawnDex
  {
    public:  using index_t = uint_fast16_t;
    private: using rank_t  = uint_fast8_t;

    public:
      template<Color Us>
      static index_t
      index(Bitboard pawnConfig, Rank r, bool moreOfUs = false, bool moreOfThem = false)
      {
          (void) moreOfUs; (void) moreOfThem;
          rank_t rank = std::max(relative_rank(Us, r) - 4, 0);
          (void) rank;
          return
               // first 10 bits: pawns
                 (pawnConfig & 0x00000001)
               | (pawnConfig & 0x00000004) >>  1
               | (pawnConfig & 0x00000100) >>  6
               | (pawnConfig & 0x00000400) >>  7 // TODO optimize
               | (pawnConfig & 0x00070000) >> 12
               | (pawnConfig & 0x07000000) >> 17
               ;
//               // 2 bits: rank
//               | (rank & 0x3) << 10
//
//               // 2 bits: more pawns us/them
//               | (moreOfUs) << 12
//               | (moreOfThem) << 13;
      }

      constexpr static uint_fast16_t indexRange() { return 1 << 10; }
  };

  // Input: pawndex(cropshift(pawns))
  // +-------+
  // | 7 8 9 | 4 to 9: consider their pawns only
  // | 4 5 6 |
  // | 2 x 3 | x: our pawn in question
  // | 0 . 1 | 0-3: consider our pawns only
  // +-------+
  // Index forms by shifting every numbered square to the bit determined by its
  // number.  Option: Hashtable with trivial FAST hash over the bare input.
  // E.g. could try just using mod 1024.
  //
  // Output:
  //

  // Score: baseline constant score
  // meta: binary foramt for additional information. e.g., when parsing pawn
  // top to bottom, could contain information that next pawn is serving greater
  // good by protecting the current pawn (pawn below doesnt know about this
  // otherwise)
  // for now, lets leave this 0
  // can be stuff like: "stops isolani" in combination with "can advance on turn"
  // +---------+
  // | . . . . |
  // | . o . . | can advance 2nd pawn from left
  // | . x . o |
  // | . . x x |
  // +---------+
  // point is, rarely will a pawn be like 5 swquares behind its nieghbor yet
  // relevant to actual position, especially if we apply a push_into algorithm
  // beforehand
  //
  // TODO ignore symmetric pawn structuresa?
  // TODO can possible return much better lockdown bitboards, since every pawn
  // pretty much knows if it is locked down (probably SOME pawntak needed)
  /*
  using lut_elem_t = union { uint_fast64_t raw=0ul; struct { Score s; uint32_t meta; } elem; };
  std::array<lut_elem_t, 1024>
      PawnLUT{};
      */
  using lut_elem_t = Score;
  lut_elem_t L[PawnDex::indexRange()] = {SCORE_ZERO};
  TUNE(SetRange(-100, 100), L);

  // Cropshift:crop 4x3 rectange, then use bitshitft w/o the worrying
  // cropshift <bitboard mask, int x, int>
  //
  // set b to match corresponding ours/their pawns before use
  template<Color Us>
  inline Bitboard
  cropshift(Bitboard b, Rank const& r, File const& f)
  {
      //   A B
      // +-------+
      // | x x x |    (07)
      // | x x x |    (07)
      // | x . x | 2  (05)
      // | x . x | 1  (05) LSB
      // +-------+

      static constexpr Bitboard cropMask = 0x07070505;

      // shift b in place
      int_fast8_t srl;
      if (Us == WHITE) srl = 8*(r-RANK_2) + (f-FILE_B);
      else             srl = 8*(r-RANK_3) + (f-FILE_B);
      Bitboard shifted = (srl > 0 ? b >>  srl
                                  : b << -srl);

      // turn upside down for BLACK
      if (Us == BLACK)
      {
          shifted = (shifted & 0xFF00FF) << 8 | (shifted & 0xFF00FF00) >> 8;
          shifted = (shifted & 0xFFFF) << 8 | (shifted & 0xFFFF0000) >> 8;
      }
      return shifted & cropMask;
  }


  #undef S
  #undef V

  template<Color Us>
  Score evaluate(const Position& pos, Pawns::Entry* e) {

    constexpr Color     Them = (Us == WHITE ? BLACK : WHITE);
    constexpr Direction Up   = (Us == WHITE ? NORTH : SOUTH);
    constexpr Direction Down = -Up;

    Bitboard b, neighbours, stoppers, doubled, support, phalanx;
    Bitboard lever, leverPush;
    Square s;
    bool opposed, backward;
    Score score = SCORE_ZERO;
    const Square* pl = pos.squares<PAWN>(Us);

    Bitboard ourPawns   = pos.pieces(  Us, PAWN);
    Bitboard theirPawns = pos.pieces(Them, PAWN);

    e->passedPawns[Us] = e->pawnAttacksSpan[Us] = e->weakUnopposed[Us] = 0;
    e->kingSquares[Us]   = SQ_NONE;
    e->pawnAttacks[Us]   = pawn_attacks_bb<Us>(ourPawns);

    // Loop through all pawns of the current color and score each pawn
    while ((s = *pl++) != SQ_NONE)
    {
        assert(pos.piece_on(s) == make_piece(Us, PAWN));

        File f = file_of(s);
        Rank r = relative_rank(Us, s);

        e->pawnAttacksSpan[Us] |= pawn_attack_span(Us, s);

        goto lut;

        // Flag the pawn
        opposed    = theirPawns & forward_file_bb(Us, s);
        stoppers   = theirPawns & passed_pawn_span(Us, s);
        lever      = theirPawns & PawnAttacks[Us][s];
        leverPush  = theirPawns & PawnAttacks[Us][s + Up];
        doubled    = ourPawns   & (s - Up);
        neighbours = ourPawns   & adjacent_files_bb(f);
        phalanx    = neighbours & rank_bb(s);
        support    = neighbours & rank_bb(s - Up);

        // A pawn is backward when it is behind all pawns of the same color
        // on the adjacent files and cannot be safely advanced.
        backward =  !(ourPawns & pawn_attack_span(Them, s + Up))
                  && (stoppers & (leverPush | (s + Up)));

        // Passed pawns will be properly scored in evaluation because we need
        // full attack info to evaluate them. Include also not passed pawns
        // which could become passed after one or two pawn pushes when are
        // not attacked more times than defended.
        if (   !(stoppers ^ lever ^ leverPush)
            && (support || !more_than_one(lever))
            && popcount(phalanx) >= popcount(leverPush))
            e->passedPawns[Us] |= s;

        else if (stoppers == square_bb(s + Up) && r >= RANK_5)
        {
            b = shift<Up>(support) & ~theirPawns;
            while (b)
                if (!more_than_one(theirPawns & PawnAttacks[Us][pop_lsb(&b)]))
                    e->passedPawns[Us] |= s;
        }

        // Score this pawn
        if (support | phalanx)
        {
            int v =  Connected[r] * (phalanx ? 3 : 2) / (opposed ? 2 : 1)
                   + 17 * popcount(support);

            score += make_score(v, v * (r - 2) / 4);
        }
        else if (!neighbours)
            score -= Isolated, e->weakUnopposed[Us] += !opposed;

        else if (backward)
            score -= Backward, e->weakUnopposed[Us] += !opposed;

        if (doubled && !support)
            score -= Doubled;
    }

lut:

    // score our pawns by pawntable
    score = SCORE_ZERO;
    e->passedPawns[Us] = Bitboard(0ul);
    for( Bitboard p = ourPawns; p; )
    {   Square sq = pop_lsb(&p);

        // current pawn
        Rank r = rank_of(sq); File f = file_of(sq);

        // window around pawn containing our and their pawns where approprioate
        assert(r > 0 && r < 7);
        Rank r2 = (Us == WHITE ? Rank(r+1) : Rank(r-1));
        Rank r3 = (Us == WHITE ? Rank(r-1) : Rank(r+1));

        // combine our pawn behind and their pawns in front to a combo BB
        Bitboard closeBB = file_bb(f) | adjacent_files_bb(f);
        Bitboard upspanBB   = closeBB & forward_ranks_bb(Us, r);
        Bitboard downspanBB = closeBB & forward_ranks_bb(Them, r2);
        Bitboard combo =  (theirPawns & upspanBB)
                        | (ourPawns   & downspanBB);

        // move combo BB to SOUTH_WEST corner of the board (flip horizontally
        // for black)
        auto cs = cropshift<Us>(combo, r, f);

        // concatenate bits for 10 pawns, relative rank and information about
        // pawn excluded in cropping to form LUT index
        bool moreOfThem = (relative_rank(Us, r) <= RANK_4) && (combo & forward_ranks_bb(Us, Rank(r+2)));
        bool moreOfUs = combo & (forward_ranks_bb(Them, r3) | (s+Down));
        PawnDex::index_t i = PawnDex::index<Us>(cs, r, moreOfUs, moreOfThem);

        // challenge pawn LUT
        score += L[i];
    }


    return score;
  }

} // namespace

namespace Pawns {

/// Pawns::probe() looks up the current position's pawns configuration in
/// the pawns hash table. It returns a pointer to the Entry if the position
/// is found. Otherwise a new Entry is computed and stored there, so we don't
/// have to recompute all when the same pawns configuration occurs again.

Entry* probe(const Position& pos) {

  Key key = pos.pawn_key();
  Entry* e = pos.this_thread()->pawnsTable[key];

  if (e->key == key)
      return e;

  e->key = key;
  e->scores[WHITE] = evaluate<WHITE>(pos, e);
  e->scores[BLACK] = evaluate<BLACK>(pos, e);

  return e;
}


/// Entry::evaluate_shelter() calculates the shelter bonus and the storm
/// penalty for a king, looking at the king file and the two closest files.

template<Color Us>
void Entry::evaluate_shelter(const Position& pos, Square ksq, Score& shelter) {

  constexpr Color     Them = (Us == WHITE ? BLACK : WHITE);
  constexpr Direction Down = (Us == WHITE ? SOUTH : NORTH);
  constexpr Bitboard BlockSquares =  (Rank1BB | Rank2BB | Rank7BB | Rank8BB)
                                   & (FileABB | FileHBB);

  Bitboard b = pos.pieces(PAWN) & ~forward_ranks_bb(Them, ksq);
  Bitboard ourPawns = b & pos.pieces(Us);
  Bitboard theirPawns = b & pos.pieces(Them);

  Value bonus[] = { (shift<Down>(theirPawns) & BlockSquares & ksq) ? Value(374) : Value(5),
                    VALUE_ZERO };

  File center = clamp(file_of(ksq), FILE_B, FILE_G);
  for (File f = File(center - 1); f <= File(center + 1); ++f)
  {
      b = ourPawns & file_bb(f);
      Rank ourRank = b ? relative_rank(Us, backmost_sq(Us, b)) : RANK_1;

      b = theirPawns & file_bb(f);
      Rank theirRank = b ? relative_rank(Us, frontmost_sq(Them, b)) : RANK_1;

      int d = std::min(f, ~f);
      bonus[MG] += ShelterStrength[d][ourRank];

      if (ourRank && (ourRank == theirRank - 1))
          bonus[MG] -= 82 * (theirRank == RANK_3), bonus[EG] -= 82 * (theirRank == RANK_3);
      else
          bonus[MG] -= UnblockedStorm[d][theirRank];
  }

  if (bonus[MG] > mg_value(shelter))
      shelter = make_score(bonus[MG], bonus[EG]);
}


/// Entry::do_king_safety() calculates a bonus for king safety. It is called only
/// when king square changes, which is about 20% of total king_safety() calls.

template<Color Us>
Score Entry::do_king_safety(const Position& pos) {

  Square ksq = pos.square<KING>(Us);
  kingSquares[Us] = ksq;
  castlingRights[Us] = pos.castling_rights(Us);

  Bitboard pawns = pos.pieces(Us, PAWN);
  int minPawnDist = pawns ? 8 : 0;

  if (pawns & PseudoAttacks[KING][ksq])
      minPawnDist = 1;

  else while (pawns)
      minPawnDist = std::min(minPawnDist, distance(ksq, pop_lsb(&pawns)));

  Score shelter = make_score(-VALUE_INFINITE, VALUE_ZERO);
  evaluate_shelter<Us>(pos, ksq, shelter);

  // If we can castle use the bonus after the castling if it is bigger
  if (pos.can_castle(Us | KING_SIDE))
      evaluate_shelter<Us>(pos, relative_square(Us, SQ_G1), shelter);

  if (pos.can_castle(Us | QUEEN_SIDE))
      evaluate_shelter<Us>(pos, relative_square(Us, SQ_C1), shelter);

  return shelter - make_score(VALUE_ZERO, 16 * minPawnDist);
}

// Explicit template instantiation
template Score Entry::do_king_safety<WHITE>(const Position& pos);
template Score Entry::do_king_safety<BLACK>(const Position& pos);

} // namespace Pawns
