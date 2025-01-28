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
	AccumSearchInfo& outInfo, SearchCache cache) {

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
	while (BoardMask singleMoveMask = moveItr.GetNext()) {
		int moveRating = Eval::RateMove(hbSelf, hbOpp, selfWinMask, singleMoveMask);

		ratedMoves[numMoves] = RatedMove{ singleMoveMask, moveRating };
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
		auto singleMoveMask = ratedMoves[i].move;

		TranspositionTable::Entry* entry = NULL;

		BoardState nextBoard = board;
		nextBoard.FillMove(singleMoveMask);

		
		uint64_t hash = TranspositionTable::HashBoard(nextBoard);
		entry = table->Find(hash);

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
		} else {
			entry->hash = hash;
#if TEST_HASH_COLLISION
			entry->board = nextBoard;
#endif
		}

		if (nextEval == VALUE_INVALID) { // No table hit
			nextEval = -AlphaBetaSearch(table, nextBoard, outInfo, cache.ProgressDepth());
			entry->eval = nextEval;
			nextEval.depth++;
		}

		if (nextEval >= cache.beta) {
			bestEval = nextEval;
			outInfo.totalPruned++;
			break;
		}

		if (nextEval > bestEval) {
			bestEval = nextEval;
			if (nextEval > cache.alpha)
				cache.alpha = nextEval;
		}
	}

	return bestEval;
}

SearchResult Search::Search(TranspositionTable* table, const BoardState& board, bool perft, bool log) {

	SearchResult result = {};

	Timer overallTimer = {};
	double lastMoveSearchTime = 0;

	BoardMask winMoveMask = board.GetValidMoveMask() & board.winMasks[board.turnSwitch];
	if (winMoveMask) {
		// We have a winning move this turn

		if (log)
			LOG("[Playing winning move(s)]");

		auto moveItr = MoveIterator(winMoveMask);
		for (int i = 0; i < BOARD_SIZE_X; i++) {
			if (!board.IsMoveValid(i)) {
				result.evals[i] = VALUE_INVALID;
				continue;
			}

			BoardMask moveMask = 0;
			moveMask.Set(i, board.GetNextY(i), true);

			if (winMoveMask & moveMask) {
				result.evals[i] = { 1, 1 };
			} else {
				result.evals[i] = 0;
			}
		}

		return result;
	}

	if (perft) {
		for (int curDepth = 0; curDepth < 100; curDepth++) {
			Timer perftTimer = {};
			uint64_t perft = PerfTest(board, curDepth);
			if (log) {
				LOG(
					"perft " << curDepth << ": " << perft <<
					", moves/sec: " << Util::NumToStr(perft / perftTimer.Elapsed())
				);
			}
		}
	} else {

		std::vector<Value> newEvals = {};

		Timer curTimer = {};
		int newBestMoveIndex = 0;
		Value bestEval = VALUE_INVALID;
		AccumSearchInfo searchInfo = {};
		for (int i = 0; i < BOARD_SIZE_X; i++) {

			if (!board.IsMoveValid(i)) {
				newEvals.push_back(VALUE_INVALID);
				result.evals[i] = VALUE_INVALID;
				continue;
			}

			BoardState nextBoard = board;
			nextBoard.DoMove(i);

			Timer moveTimer = {};
			Value eval = -AlphaBetaSearch(table, nextBoard, searchInfo);
			lastMoveSearchTime = moveTimer.Elapsed();
			newEvals.push_back(eval);

			if (eval > bestEval) {
				bestEval = eval;
				bestEval.depth++;
				newBestMoveIndex = i;
			}
		}
		uint64_t movesPerSecond = searchInfo.totalSearched / MAX(curTimer.Elapsed(), 1e-7);

		RASSERT(newEvals.size() == BOARD_SIZE_X, "Bad eval amount");
		std::copy(newEvals.begin(), newEvals.end(), result.evals);

		double timeElapsed = overallTimer.Elapsed();
		if (log) {
			LOG(
				"Eval: " << bestEval <<
				", searched: " << Util::NumToStr(searchInfo.totalSearched) << "/" << Util::NumToStr(searchInfo.totalPruned) <<
				", moves/sec: " << Util::NumToStr(movesPerSecond) <<
				", winfrac: " << searchInfo.GetWinFrac() <<
				", tablefrac: " << searchInfo.GetTableHitFrac()
			);
		}
	}

	return result;
}
