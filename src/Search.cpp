#include "Search.h"

uint64_t Search::PerfTest(const BoardState& board, int depth, int depthElapsed) {
	BoardMask validMovesMask = board.GetValidMoveMask();

	if (depth > 1) {
		BoardMask winMask = board.winMasks[board.turnSwitch];

		uint64_t count = 0;
		auto moveItr = MoveIterator(validMovesMask);
		while (BoardMask singleMoveMask = moveItr.GetNext()) {
			if (winMask & singleMoveMask) {
				count += 1; // Move wins the game
				continue;
			}

			BoardState nextBoard = board;
			nextBoard.FillMove(singleMoveMask);

			count += PerfTest(nextBoard, depth - 1, depthElapsed + 1);
		}
		return count;
	} else {
		return Util::BitCount64(validMovesMask);
	}
}

Value Search::AlphaBetaSearch(
	TranspositionTable* table, const BoardState& board,
	SearchInfo& outInfo, SearchCache cache) {

	outInfo.totalSearched++;
	BoardMask validMovesMask = board.GetValidMoveMask();
	BoardMask hbSelf = board.teams[board.turnSwitch];
	BoardMask hbOpp = board.teams[!board.turnSwitch];
	BoardMask selfWinMask = board.winMasks[board.turnSwitch];
	BoardMask oppWinMask = board.winMasks[!board.turnSwitch];

	Value bestEval = Eval::EvalValidMoves(hbSelf, hbOpp, selfWinMask, oppWinMask, validMovesMask);
	if (bestEval != VALUE_INVALID) {
		outInfo.totalWins++;
		return bestEval;
	}

	struct RatedMove {
		BoardMask move;
		int eval;
	};
	RatedMove ratedMoves[BOARD_SIZE_X];
	auto moveItr = MoveIterator(validMovesMask);
	int numMoves = 0;
	while (BoardMask move = moveItr.GetNext()) {

		int moveRating = Eval::RateMove(hbSelf, hbOpp, selfWinMask, move, board.moveCount);

		ratedMoves[numMoves] = RatedMove{ move, moveRating };
		numMoves++;
	}

	// Insertion sort the moves
	for (size_t i = 1; i < numMoves; i++) {
		for (size_t j = i; j > 0;) {
			RatedMove prev = ratedMoves[j - 1];
			RatedMove cur = ratedMoves[j];

			if (cur.eval > prev.eval) {
				// Swap
				ratedMoves[j - 1] = cur;
				ratedMoves[j] = prev;
				j--;
			} else {
				break;
			}
		}
	}

	for (size_t i = 0; i < numMoves; i++) {
		Value nextEval = VALUE_INVALID;
		auto move = ratedMoves[i].move;

		BoardState nextBoard = board;
		nextBoard.FillMove(move);

		uint64_t hash = TranspositionTable::HashBoard(nextBoard);
		TranspositionTable::Entry* entry = table->Find(hash);

		outInfo.totalTableSeaches++;
		if (entry->hash == hash) {

#if TEST_HASH_COLLISION
			if (entry->board != nextBoard) {
				ERR_CLOSE(
					"Hash collision (depthRemaining: " << depthRemaining << ")" << std::endl <<
					(void*)hash << " != " << (void*)TranspositionTable::HashBoard(entry->board) << std::endl <<
					"Our board: " << nextBoard << ", entry's board: " << entry->board
				);
			}
#endif

			// Use the table entry
			nextEval = entry->eval;
			nextEval.depth++;
			outInfo.totalTableHits++;
		} else { // No table hit
			nextEval = -AlphaBetaSearch(table, nextBoard, outInfo, cache.ProgressDepth());

			entry->eval = nextEval;
			entry->hash = hash;
#if TEST_HASH_COLLISION
			entry->board = nextBoard;
#endif
			nextEval.depth++;
		}

		if (nextEval >= cache.max) {
			bestEval = nextEval;
			if (cache.depthElapsed == 0)
				outInfo.bestMove = move;

			outInfo.totalPruned++;
			break;
		}

		if (nextEval > bestEval) {
			bestEval = nextEval;

			if (nextEval > cache.min)
				cache.min = nextEval;

			if (cache.depthElapsed == 0)
				outInfo.bestMove = move;
		}
	}

	return bestEval;
}

SearchResult Search::Search(TranspositionTable* table, const BoardState& board, bool log) {
	Timer timer = {};
	BoardMask validMoves = board.GetValidMoveMask();

	RASSERT(validMoves, "No valid moves in the position");

	BoardMask winMoveMask = validMoves & board.winMasks[board.turnSwitch];
	if (winMoveMask) {
		// We have a winning move this turn

		if (log)
			LOG("[Playing winning move]");

		auto moveItr = MoveIterator(winMoveMask);
		for (int i = 0; i < BOARD_SIZE_X; i++) {
			if (!board.IsMoveValid(i)) {
				continue;
			}

			BoardMask moveMask = 0;
			moveMask.Set(i, board.GetNextY(i), true);

			if (winMoveMask & moveMask)
				return { moveMask, 1 };
		}

		ERR_CLOSE("Thought we had winning move, but never found it");
	}
	
	SearchInfo searchInfo = {};
	Value eval = AlphaBetaSearch(table, board, searchInfo);
	double timeElapsed = timer.Elapsed();

	BoardMask bestMove = searchInfo.bestMove;
	if (!bestMove) {
		auto itr = MoveIterator(validMoves);
		bestMove = itr.GetNext();
	}

	uint64_t movesPerSecond = searchInfo.totalSearched / timeElapsed;
		
	if (log) {
		LOG(
			"Eval: " << eval <<
			", searched: " << Util::NumToStr(searchInfo.totalSearched) << "/" << Util::NumToStr(searchInfo.totalPruned) <<
			", moves/sec: " << Util::NumToStr(movesPerSecond) <<
			", winfrac: " << searchInfo.GetWinFrac() <<
			", tablehitfrac: " << searchInfo.GetTableHitFrac() <<
			", tablefillfrac: " << table->GetFillFrac()
		);
	}

	return { bestMove, eval };
}