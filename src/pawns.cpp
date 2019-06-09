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

  #undef S
  #undef V

  template<Color Us>
  Score evaluate(const Position& pos, Pawns::Entry* e, Bitboard& sp2) {

    constexpr Color     Them = (Us == WHITE ? BLACK : WHITE);
    constexpr Direction Up   = (Us == WHITE ? NORTH : SOUTH);

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

    sp2 = 0;

    // Loop through all pawns of the current color and score each pawn
    while ((s = *pl++) != SQ_NONE)
    {
        assert(pos.piece_on(s) == make_piece(Us, PAWN));

        File f = file_of(s);
        Rank r = relative_rank(Us, s);

        const Bitboard span = pawn_attack_span(Us, s);
        sp2 |= e->pawnAttacksSpan[Us] & span;
        e->pawnAttacksSpan[Us] |= span;


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
  Bitboard sp2[COLOR_NB];
  e->scores[WHITE] = evaluate<WHITE>(pos, e, sp2[WHITE]);
  e->scores[BLACK] = evaluate<BLACK>(pos, e, sp2[BLACK]);

  e->compute_fixed<WHITE>(pos, sp2[WHITE]);
  e->compute_fixed<BLACK>(pos, sp2[BLACK]);

  return e;
}

/// Entry::compute_fixed computes a bitboard of squares which could be attacked
/// by the opponents pawns when advancing. Like pawn_attack_span, it does NOT
/// account for pawns changing files through captures. Unlike pawn_attack_span,
/// it DOES account for pawns being blocked or backwards with no hope to advance
/// on their own.
///
/// Apart from the computed fluent_span, also some intermediate values could be
/// used for bonuses. For example, the untouchable Bitboard is filled with
/// squares which the algorithm thinks are unlikely to be attacked by an
/// opponents pawn in the near future.

template<Color Us>
void Entry::compute_fixed(const Position& pos, Bitboard& sp2) &
{{
    constexpr bool W        = Us == WHITE;
    constexpr Color Them    = ~Us;
    constexpr Direction Up  = (W ? NORTH : SOUTH);

    // inputs
    const Bitboard ourPawns     = pos.pieces(Us, PAWN);
    const Bitboard theriPawns   = pos.pieces(Them, PAWN);
    const Bitboard init         = this->pawnAttacksSpan[Them];
    const Bitboard init2        = sp2; // squares challenged by more than 1 opponent in current configuration
    const Bitboard init1        = init ^ init2; // squares challenged by exactly 1 opponent in current configuration

    // total bitboards
    Bitboard totalShut = 0; // shut squares(!)
    Bitboard totalConsidered = ourPawns;
    Bitboard totalUntouchable = 0;

    // iteration bitboards: updated at the beginning of each main iteration, but not in-between
    Bitboard iterSpan,
             iterSpan1,
             iterSpan2;
    Bitboard iterFix;
    // next bitboards: online construction for running iteration
    Bitboard nextSpan   = init,
             nextSpan1  = init1,
             nextSpan2  = init2;
    Bitboard newFixi    = 0; // shut pawns(!)

    auto const addNewSpan = [&](Bitboard span) -> void
    {
        nextSpan2 |= nextSpan & span;
        nextSpan  |= span;
        nextSpan1  = span ^ nextSpan2; // TODO optimize (dont call every time)
    }

iteration:

    iterSpan    = nextSpan;  nextSpan  = 0;
    iterSpan1   = nextSpan1; nextSpan1 = 0;
    iterSpan2   = nextSpan2; nextSpan2 = 0;
    iterFix     = nextFix;

    // find iteration untouchables
    Bitboard faceToFace         = ourPawns & iterSpan1; // pawns facing exactly 1 opponent
    Bitboard faceOffs           = pawn_attacks_bb<Us> (faceToFace);

    totalConsidered            |= faceOffs;

    Bitboard considered         = totalConsidered & ~totalUntouchable; // sqaures which MIGHT block opponents, if they are outside of any attack span
    Bitboard untouchable        = considered & ~iterSpan;
    const Bitboard iterUntouchable = totalUntouchable; // memorize situation before this iteration
    totalUntouchable           |= untouchable;

removal:
    // _________________________________________________________________
    // step 1: remove a sinlge layer
    //
    //  - stepFixed     pawns shut down during step 1 at some distance
    //  - newFix        pawns shut down during this iteration TODO ???
    //  - totalShut     squares on which opponents pawns will run into our
    //                  untouchables eventually
    Bitboard stepFixed = 0; // pawns shut during half-iteration ("at once")
    if (!untouchable) goto end; // span can't change if there's not a single untouchable pawn TODO earlier?
    else while (untouchable)
    {
        const Square    u           = pop_lsb(&untouchable);    // our untouchable pawn
        const Bitboard  shutting    = forward_file_bb(Us, u);   // squares in front of untouchable
        const Bitboard  fix         = shutting & theirPawns;    // their pawns in front of untouchable

        totalShut |= shutting;
        stepFixed |= fix;
    }
    newFix |= stepFixed;

    if (!stepFixed)
        goto prepare_next;

    // _________________________________________________________________
    // step 2: 1-step shortcut to possibly remove layer 2 (using span2)
    //
    //  - untouchable       our untouchable pawns (found duting this step)
    //  - totalUntouchable  our global untouchable bb with all squares found
    //                      untouchable (updated with new-found untouchables).
    //  - considered        squares which become untouchable when leaving their
    //                      attack span. out pawns and face-offs (when found
    //                      untouchable during this step, removen from this
    //                      bb).
    //  - addNewSpan()      adding new restricted pawn attack span for this
    //                      iteration
    //  - stepFixed         their pawns fixed in step 1 (destructed during this
    //                      step).
    else while (stepFixed)
    {
        // compute updated span for blocked pawn
        const Square    s           = pop_lsb(&stepFixed);
        const Bitboard  span        = pawn_attack_span<Them> (s);
        const Bitboard  block       = forward_file_bb(Them, s) & totalUntouchable;
        const Square    b           = frontmost_sq(Us, block);
        const Bitboard  blockerSpan = pawn_attack_span<Them> (b);

        const Bitboard prevBlock    = block & iterUntouchable;
        Bitboard deniedSpan;
        if (prevBlock) // also need span from previous iteration if has beed restricted before
        {
            const Square    p           = frontmost_sq(Us, prevBlock);
            const Bitboard  prevBlkSpan = pawn_attack_span<Them> (p);
            const Bitboard  prevSpan    = span ^ prevBlkSpan;

            deniedSpan = prevSpan & blockerSpan;
        }
        else
        {
            deniedSpan = span & blockerSpan;
        }
        //
        // update untouch/considered from new span insights
        const Bitboard  leftoverSpan = span ^ blockerSpan;
        const Bitboard onlyResp = deniedSpan & iterSpan1; // can use iterSpan because we check iterblocks before
        const Bitboard onlyRespUntouch = onlyResp & considered;

        addNewSpan(leftoverSpan); // TODO I think impossible to correct during current iteration
        untouchable |= onlyRespUntouch;
        considered  ^= onlyRespUntouch;

        // TODO algorithm for face-off required? (effect?) (need span3 logic i think)
        // TODO algorithm for pawn-double-attacks required? (how?)(if both are face-to-face = block also 1-span squares)
    }
    totalUntouchable |= untouchable;

    goto removal;

prepare_next:

    // _________________________________________________________________
    // step 3: compute entire fluent span to prepare next iteration
    //
    // addNewSpan()     update fluent span bbs for next iteration
    Bitboard fluent = theirPawns ^ newFix;
    while (fluent)
    {
        const Square f = pop_lsb(fluent);
        const Bitboard span = pawn_attack_span<Them> (f);
        addNewSpan(span);
    }

    goto iteration;

end:

    // TODO continue...

 }


































//        const bool newPawnShutDown  = bool(shut & ~newFix); //        const bool belowAllPrevious = bool(shutting & ~totalShut); // TODO use totalShut or current iteration accum //        if (!newPawnShutDown && belowAllPrevious) //            continue; // don't recalculate if  frontmost stopper did not change











    // constructible bitboards
    /*
    Bitboard cFluentSpan    = 0;    // their probable pawn attack span
    Bitboard cShutDown      = 0;    // their probably blocked pawns
    Bitboard cUntouchable   = 0;    // our probably safe pawns
    */
    Bitboard cFluentSpan    = 0;    // their probable pawn attack span

    constexpr Color Them = ~Us;
    const Bitboard ourPawns = pos.pieces(Us, PAWN);
    const Bitboard theirPawns = pos.pieces(Them, PAWN);

    Bitboard lastSpan = AllSquares;
    Bitboard newSpan = this->pawnAttacksSpan[Them];
    Bitboard new2    = sp2; sp2 = AllSquares;
    Bitboard shutSquares = 0;
    const Bitboard sp1 = newSpan ^ new2;


//    const Bitboard totalConsidered = ourPawns | this->pawnAttacks[Us];
    Bitboard totalConsidered = ourPawns;
    Bitboard totalUntouch = 0;

    Bitboard touchable = totalConsidered;

    do
    {
        const Bitboard oneVone = ourPawns & touchable & (sp2 ^ new2);
        const Bitboard atk   = pawn_attacks_bb<Us>(oneVone);
        totalConsidered |= atk; touchable |= atk;
        Bitboard untouchable = touchable & (lastSpan ^ newSpan);
        /* cUntouchable |= untouchable; */

        totalUntouch |= untouchable;
        touchable ^= untouchable;

        lastSpan = newSpan; sp2 = new2;
        newSpan = new2 = 0;

        auto update = [&](Bitboard sp)->void
        { new2 |= newSpan & sp, newSpan |= sp; };

        if( untouchable ) while( untouchable )
        {
            const Square u = pop_lsb(&untouchable);

            const Bitboard shutting = forward_file_bb(Us, u);
            shutSquares |= shutting;

            Bitboard shutDown = shutSquares & theirPawns;
            /* cShutDown |= shutDown; */
            while (shutDown)
            {
                const Square s = pop_lsb(&shutDown);
                const Bitboard sp = pawn_attack_span(Them, s);                          // their full span
                const Bitboard firstShutSpan =
                    pawn_attack_span(Them,frontmost_sq(Us,  forward_file_bb(Them, s)    // span of first blocker
                                                          & totalUntouch));

                const Bitboard reducedSpan = sp ^ firstShutSpan;
                update(reducedSpan);

                // fast-block algorithm (TODO might not work as intended)
                const Bitboard& const affectedSpan = firstShutSpan;
                // if the newly shut down pawn created a span which was ONLY
                // ONCE in original span, then add to untouch before next
                // iteration (prevent waiting for loop adding span of fluent
                // pawns)
                const Bitboard fastUntouchable = touchable & affectedSpan & sp1;

                // handle newfound results as shortcut:
                untouchable |= fastUntouchable;
                totalUntouch |= fastUntouchable;
            }
        }
        else break;

        Bitboard fluentPawns = theirPawns & ~shutSquares;
        while (fluentPawns)
        {
            const Square f = pop_lsb(&fluentPawns);
            const Bitboard sp = pawn_attack_span(Them, f);
            update(sp);
        }

    } while (newSpan != lastSpan);

    cFluentSpan = lastSpan;

    // squares which will probably not be attacked by out pawns anymore = fix
    this->fix[Them] = ~cFluentSpan;
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
