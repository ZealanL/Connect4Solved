#include "Search.h"
#include "InstaSolver.h"

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

	Value bestEval = Eval::EvalAndCropValidMoves(board, validMovesMask);
	if (bestEval != VALUE_INVALID)
		return bestEval;

	bool useTable = board.moveCount < (BOARD_CELL_COUNT - 8);

	uint64_t hash = 0;
	TranspositionTable::Entry* entryPtr = NULL;
	TranspositionTable::Entry entry = {};
	if (useTable) {
		hash = TranspositionTable::HashBoard(board);
		entryPtr = table->Find(hash);
		entry = *entryPtr;
		outInfo.totalTableSeaches++;
	}

	BoardMask tableBestMove = 0;

	if (useTable && entry.hash == hash) {
		// We have a matching entropy

		tableBestMove = entry.bestMove;

		if (entry.eval >= cache.max) {
			// Exceeds maximum, prune
			return entry.eval;
		} else if (!entry.isCutNode) { // It's exact
			return entry.eval;
		}

#if DEBUG_TRANSPOSITION_TABLE
		if (entry.board != board) {
			ERR_CLOSE(
				"Hash collision (depth: " << (int)cache.depthElapsed << ")" << std::endl <<
				(void*)hash << " != " << (void*)TranspositionTable::HashBoard(entry.board) << std::endl <<
				"Our next board: " << board << ", entry's next board: " << entry.board
			);
		}
#endif
	}

	// Check insta-solve solution
	// (We only check on at least 1 depth, otherwise the best move would fail)
	if (cache.depthElapsed > 1) {
		auto solveResult = InstaSolver::Solve(board);
		if (solveResult.type) {
			bool returnSolveResult =
				(solveResult.type == InstaSolver::LOWER_BOUND && solveResult.eval >= cache.max) ||
				(solveResult.type == InstaSolver::UPPER_BOUND && solveResult.eval < cache.min) ||
				(solveResult.type == InstaSolver::EXACT);

			if (returnSolveResult)
				return solveResult.eval;
		}
	}
	auto nodesBefore = outInfo.totalSearched;

	struct RatedMove {
		BoardMask move;
		float eval;
	};
	RatedMove ratedMoves[BOARD_SIZE_X];
	int numMoves = 0;

	if (board.IsSymmetrical()) {
		// We can just only consider moves on one side
		BoardMask sidedMask = 0;
		for (int x = 0; x < BOARD_SIZE_X / 2 + 1; x++)
			sidedMask |= BoardMask::GetColumnMask(x);

		validMovesMask &= sidedMask;
		if (tableBestMove && !(tableBestMove && sidedMask)) {
			// Flip table best move to match our side
			tableBestMove = tableBestMove.FlipX();
		}
	}

	auto moveItr = MoveIterator(validMovesMask);
	while (BoardMask move = moveItr.GetNext()) {

		float moveRating = Eval::RateMove(board, move);

		if (tableBestMove == move)
			moveRating = FLT_MAX;

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
	
	BoardMask bestMove = 0;
	for (size_t i = 0; i < numMoves; i++) {
		Value nextEval = VALUE_INVALID;
		auto move = ratedMoves[i].move;

		BoardState nextBoard = board;
		nextBoard.FillMove(move);

		nextEval = AlphaBetaSearch(table, nextBoard, outInfo, cache.ProgressDepth());
		nextEval = -nextEval;
		nextEval.depth++;

		if (nextEval >= cache.max) {
			bestEval = nextEval;
			bestMove = move;
			outInfo.totalPruned++;
			break;
		}

		if (nextEval > bestEval) {
			bestEval = nextEval;

			if (nextEval > cache.min)
				cache.min = nextEval;

			bestMove = move;
		}
	}
	bool hitCutoff = bestEval >= cache.max;

	if (useTable) {
		entry.hash = hash;
		entry.bestMove = bestMove;
		entry.eval = bestEval;
		entry.isCutNode = hitCutoff;
#if DEBUG_TRANSPOSITION_TABLE
		entry.board = board;
#endif
		*entryPtr = entry;
	}
	outInfo.bestMove[cache.depthElapsed] = bestMove;

	return bestEval;
}

std::vector<BoardMask> Search::FindPVFromTable(TranspositionTable* table, const BoardState& board, BoardMask firstMove) {
	std::vector<BoardMask> result = { firstMove };
	
	BoardState curBoard = board;
	curBoard.FillMove(firstMove);

	while (true) {
		auto hash = TranspositionTable::HashBoard(curBoard);
		auto entry = table->Find(hash);
		if (hash != entry->hash)
			break;

		if (entry->bestMove == 0)
			break;

		result.push_back(entry->bestMove);
		curBoard.FillMove(entry->bestMove);
	}

	return result;
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

	BoardMask bestMove = searchInfo.bestMove[0];
	if (!bestMove) {
		// Just pick the first valid move
		auto itr = MoveIterator(validMoves);
		bestMove = itr.GetNext();
	}

	auto pv = FindPVFromTable(table, board, bestMove);
	std::string pvStr = {};
	for (BoardMask move : pv)
		pvStr += '1' + (int)(Util::BitMaskToIndex(move) / 8);

	uint64_t movesPerSecond = searchInfo.totalSearched / timeElapsed;
		
	if (log) {
		LOG(
			"Eval: " << eval <<
			", searched: " << Util::NumToStr(searchInfo.totalSearched) << "/" << Util::NumToStr(searchInfo.totalPruned) <<
			", moves/sec: " << Util::NumToStr(movesPerSecond) <<
			", tablehitfrac: " << searchInfo.GetTableHitFrac() <<
			", tablefillfrac: " << table->GetFillFrac()
		);
		LOG(" > PV: " << pvStr);
	}

	return { bestMove, eval };
}