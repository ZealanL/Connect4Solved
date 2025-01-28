#pragma once

#include "Framework.h"

#include "BoardState.h"
#include "Eval.h"
#include "Timer.h"
#include "TranspositionTable.h"

#include "Util.h"

uint64_t PerfTest(const BoardState& board, int depth, int depthElapsed = 0) {
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

struct AccumSearchInfo {
	uint64_t totalSearched = 0;
	uint64_t totalTableSeaches = 0;
	uint64_t totalTableHits = 0;
	uint64_t totalWins = 0;
	uint64_t totalPruned = 0; // Times we pruned due to beta

	double GetTableHitFrac() const {
		return (totalTableSeaches > 0) ? (double)totalTableHits / (double)totalTableSeaches : 0;
	}

	double GetWinFrac() const {
		return (double)totalWins / (double)totalSearched;
	}
};

Value AlphaBetaSearch(
	TranspositionTable* table, const BoardState& board,
	AccumSearchInfo& outInfo,
	Value alpha = -1, Value beta = 1, int depthElapsed = 0) {

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
		int moveRating = Eval::RateMove(hbSelf, hbOpp, singleMoveMask); 

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

		BoardState nextBoard = board;
		nextBoard.FillMove(singleMoveMask);

		TranspositionTable::Entry* entry = NULL;
		auto hash = TranspositionTable::HashBoard(nextBoard);
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
		if (nextEval == VALUE_INVALID) {
			nextEval = -AlphaBetaSearch(table, nextBoard, outInfo, -beta, -alpha, depthElapsed + 1);
			if (entry)
				entry->eval = nextEval;
			nextEval.depth++;
		}

		if (nextEval >= beta) {
			bestEval = nextEval;
			outInfo.totalPruned++;
			break;
		}

		if (nextEval > bestEval) {
			bestEval = nextEval;
			if (nextEval > alpha)
				alpha = nextEval;
		}
	}


	return bestEval;
}

struct SearchResult {
	Value evals[BOARD_SIZE_X];
};

SearchResult Search(TranspositionTable* table, const BoardState& board, bool perft, bool log) {

	SearchResult result = {};

	Timer overallTimer = {};
	double lastMoveSearchTime = 0;

	BoardMask winMoveMask = board.GetValidMoveMask() & board.winMasks[board.turnSwitch];
	if (winMoveMask) {
		// We have a winning move this turn

		LOG("[Playing winning move(s)]");

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
			LOG(
				"perft " << curDepth << ": " << perft <<
				", moves/sec: " << Util::NumToStr(perft / perftTimer.Elapsed())
			);
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
		LOG(
			"Eval: " << bestEval <<
			", searched: " << Util::NumToStr(searchInfo.totalSearched) << "/" << Util::NumToStr(searchInfo.totalPruned) <<
			", moves/sec: " << Util::NumToStr(movesPerSecond) <<
			", winfrac: " << searchInfo.GetWinFrac() <<
			", tablefrac: " << searchInfo.GetTableHitFrac()
		);
	}

	return result;
}

int main() {
	Eval::Init();

	BoardState board = {};

	bool computerOnly = true;

	auto table = new TranspositionTable();

	std::string movesStr = {};

	while (true) {
		LOG("Board: " << board);
		if (!movesStr.empty())
			LOG("(Moves: " << movesStr << ")");

		bool humansTurn = !board.turnSwitch && !computerOnly;

		if (Eval::IsWonAfterMove(board)) {
			const char* winner;
			if (computerOnly) {
				winner = board.turnSwitch ? "Computer #1" : "Computer #2";
			} else {
				winner = humansTurn ? "Computer" : "Human";
			}

			LOG("Game over. " << winner << " won!");
			return EXIT_SUCCESS;
		}

		bool anyValidMoves = false;
		for (int i = 0; i < BOARD_SIZE_X; i++)
			anyValidMoves |= board.IsMoveValid(i);
		if (!anyValidMoves) {
			LOG("Game over. It's a draw.");
			return EXIT_SUCCESS;
		}

		if (!humansTurn) {
			auto searchResult = Search(table, board, false, true);

			int chosenMoveIndex;
			{
				// Pick the move with the highest eval
				// If multiple moves have the best evla, pick one at random

				Value maxEval = VALUE_INVALID;
				for (int i = 0; i < BOARD_SIZE_X; i++)
					maxEval = MAX(maxEval, searchResult.evals[i]);

				std::vector<int> equallyBestMoves;
				for (int i = 0; i < BOARD_SIZE_X; i++)
					if (searchResult.evals[i] == maxEval)
						equallyBestMoves.push_back(i);
				RASSERT(!equallyBestMoves.empty(), "Failed to find any best moves");

				chosenMoveIndex = equallyBestMoves[rand() % equallyBestMoves.size()];
			}

			LOG("Playing move: " << chosenMoveIndex + 1);
			movesStr += '1' + chosenMoveIndex;
			board.DoMove(chosenMoveIndex);
		} else {
			// Human can move
			while (true) {
				std::cout << "Your move index: ";
				std::string line;
				std::getline(std::cin, line);
				int moveIndex;
				bool invalid = false;
				try {
					moveIndex = std::stoi(line) - 1;
				} catch (std::exception& e) {
					LOG("Invalid move (cannot parse)");
					continue;
				}

				if (moveIndex < 0 || moveIndex >= BOARD_SIZE_X) {
					LOG("Invalid move (out of range)");
					continue;
				}

				if (!board.IsMoveValid(moveIndex)) {
					LOG("Invalid move (column full)");
					continue;
				}
				movesStr += '1' + moveIndex;
				board.DoMove(moveIndex);
				break;
			}
		}
	}

	return EXIT_SUCCESS;
}