#pragma once

#include "Framework.h"

#include "BoardState.h"
#include "Eval.h"
#include "Timer.h"
#include "TranspositionTable.h"

#include "Util.h"

uint64_t RandU64() {
	thread_local static auto engine = std::default_random_engine();
	thread_local static auto distribution = std::uniform_int_distribution<unsigned long long>(
		std::numeric_limits<std::uint64_t>::min(),
		std::numeric_limits<std::uint64_t>::max()
	);

	return distribution(engine);
}

uint64_t PerfTest(const BoardState& board, int depth, int depthElapsed = 0) {
	BoardMask validMovesMask = board.GetValidMoveMask();

	if (depth > 1) {
		BoardMask winMask = board.GetWinMask(board.turnSwitch);

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
	uint64_t totalPruned = 0;

	double GetTableHitFrac() const {
		return (totalTableSeaches > 0) ? (double)totalTableHits / (double)totalTableSeaches : 0;
	}

	double GetWinFrac() const {
		return (double)totalWins / (double)totalSearched;
	}
};

int MinMaxSearch(
	const BoardState& board, int depthRemaining, AccumSearchInfo& outInfo, int depthElapsed = 0) {

	outInfo.totalSearched++;
	BoardMask validMovesMask = board.GetValidMoveMask();
	BoardMask winMask = board.GetWinMask(board.turnSwitch);

	int max = -INT_MAX;
	auto moveItr = MoveIterator(validMovesMask);
	while (BoardMask singleMoveMask = moveItr.GetNext()) {
		int nextEval;

		if (winMask & singleMoveMask) {
			// Move wins the game
			outInfo.totalWins++;
			nextEval = WIN_BASE_VALUE - 1;
		} else if (depthRemaining > 1) {
			BoardState nextBoard = board;
			nextBoard.FillMove(singleMoveMask);

			nextEval = -MinMaxSearch(nextBoard, depthRemaining - 1, outInfo, depthElapsed + 1);
		} else {
			// No possible wins within our depth
			nextEval = 0;
		}
		max = MAX(max, nextEval);
	}
	return max;
}


int AlphaBetaSearch(
	TranspositionTable& table, const BoardState& board, int depthRemaining, 
	AccumSearchInfo& outInfo,
	int alpha = -INT_MAX, int beta = INT_MAX, int depthElapsed = 0) {

	outInfo.totalSearched++;
	BoardMask validMovesMask = board.GetValidMoveMask();
	BoardMask hbSelf = board.teams[board.turnSwitch];
	BoardMask hbOpp = board.teams[!board.turnSwitch];

	BoardMask winMask = BoardMask::GetWinMask(hbSelf);

	{ // Win checking
		if (validMovesMask & winMask) {
			// We can win this turn
			return WIN_BASE_VALUE - 1;
		}

		BoardMask oppWinMask = BoardMask::GetWinMask(hbOpp);
		BoardMask oppWinNextMask = oppWinMask & validMovesMask;

		if (oppWinNextMask) {
			// If the opponent has a win next turn, we must play there
			// This will prevent searching stupid moves that lose
			validMovesMask &= oppWinNextMask;

			if (Util::MultipleBitsSet(validMovesMask)) {
				// Opponent has multiple winning moves next turn, we cannot stop them all
				return -WIN_BASE_VALUE + 2;
			}
		}

		// Everywhere below a winning square for the opponent
		BoardMask belowOppWin = (oppWinMask >> 1);

		// We can't play there
		validMovesMask &= ~belowOppWin;

		if (validMovesMask == 0)
			return -WIN_BASE_VALUE + 2; // Opponent will win next turn
	}

	int bestEval = -INT_MAX;

	// Returns true if we should stop searching
	auto fnProcessMove = [&](BoardMask singleMoveMask) -> bool {
		int nextEval = -INT_MAX;

		if (depthRemaining > 1) {
			BoardState nextBoard = board;
			nextBoard.FillMove(singleMoveMask);

			TranspositionTable::Entry* entry = NULL;
			bool useTable = depthRemaining >= 4;
			if (useTable) {
				auto hash = TranspositionTable::HashBoard(nextBoard);
				entry = table.Find(hash);
				outInfo.totalTableSeaches++;
				if (entry->fullHash == hash && entry->depthRemaining >= depthRemaining) {
					// Use the table entry
					nextEval = entry->eval;
					outInfo.totalTableHits++;
				} else {
					entry->fullHash = hash;
					entry->depthRemaining = depthRemaining;
					entry->time = table.timeCounter;
				}
			}
			if (nextEval == -INT_MAX) {
				nextEval = -AlphaBetaSearch(table, nextBoard, depthRemaining - 1, outInfo, -beta, -alpha, depthElapsed + 1);

				if (entry)
					entry->eval = nextEval;
			}
		} else {
			// No possible wins within our depth
			nextEval = 0;
		}

		if (nextEval > bestEval) {
			bestEval = nextEval;
		}
		if (nextEval > alpha)
			alpha = nextEval;
		if (bestEval >= beta) {
			outInfo.totalPruned++;
			return true;
		}
		
		return false;
	};

	bool sortMoves = depthRemaining >= 4;

	if (sortMoves) {
		struct RatedMove {
			BoardMask move;
			int eval;
		};
		RatedMove ratedMoves[BOARD_SIZE_X];
		auto moveItr = MoveIterator(validMovesMask);
		int numMoves = 0;
		while (BoardMask singleMoveMask = moveItr.GetNext()) {
			ratedMoves[numMoves] = RatedMove{ singleMoveMask, Eval::EvalMove(board, singleMoveMask) };
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
			if (fnProcessMove(ratedMoves[i].move))
				break;
		}

	} else {
		auto moveItr = MoveIterator(validMovesMask);
		while (BoardMask singleMoveMask = moveItr.GetNext()) {
			if (fnProcessMove(singleMoveMask))
				break;
		}
	}

	// For win counting
	if (abs(bestEval) >= WIN_MIN_VALUE)
		bestEval -= (bestEval > 0) ? 1 : -1;

	return bestEval;
}

struct SearchResult {
	int evals[BOARD_SIZE_X];
};

SearchResult Search(TranspositionTable& table, const BoardState& board, bool perft, double maxTime, bool log) {

	SearchResult result = {};

	Timer overallTimer = {};
	double lastMoveSearchTime = 0;

	BoardMask winMoveMask = board.GetValidMoveMask() & board.GetWinMask(board.turnSwitch);
	if (winMoveMask) {
		// We have a winning move this turn

		LOG("[Playing winning move]");

		for (int i = 0; i < BOARD_SIZE_X; i++) {
			if (!board.IsMoveValid(i)) {
				result.evals[i] = -INT_MAX;
				continue;
			}

			BoardMask moveMask = 0;
			moveMask.Set(i, board.GetNextY(i), true);

			if (board.GetWinMask(board.turnSwitch) & moveMask) {
				result.evals[i] = WIN_BASE_VALUE - 1;
			} else {
				result.evals[i] = 0;
			}
		}

		return result;
	}

	for (int curDepth = 1;; curDepth++) {
		
		if (perft) {
			Timer perftTimer = {};
			uint64_t perft = PerfTest(board, curDepth);
			LOG(
				"perft " << curDepth << ": " << perft <<
				", moves/sec: " << Util::NumToStr(perft / perftTimer.Elapsed())
			);
		} else {

			std::vector<int> newEvals = {};

			Timer curTimer = {};
			int newBestMoveIndex = 0;
			int bestEval = -INT_MAX;
			AccumSearchInfo searchInfo = {};
			for (int i = 0; i < BOARD_SIZE_X; i++) {

				if (!board.IsMoveValid(i)) {
					newEvals.push_back(-INT_MAX);
					result.evals[i] = -INT_MAX;
					continue;
				}

				BoardState nextBoard = board;
				nextBoard.DoMove(i);

				Timer moveTimer = {};
				int eval = -AlphaBetaSearch(table, nextBoard, curDepth - 1, searchInfo, -INT_MAX, INT_MAX);
				lastMoveSearchTime = moveTimer.Elapsed();

				newEvals.push_back(eval);

				if (eval > bestEval) {
					bestEval = eval;
					newBestMoveIndex = i;
				}

				if (overallTimer.Elapsed() + lastMoveSearchTime >= maxTime)
					return result;
			}
			uint64_t movesPerSecond = searchInfo.totalSearched / MAX(curTimer.Elapsed(), 1e-7);

			RASSERT(newEvals.size() == BOARD_SIZE_X, "Bad eval amount");
			std::copy(newEvals.begin(), newEvals.end(), result.evals);

			double timeElapsed = overallTimer.Elapsed();
			LOG(
				std::setprecision(5) <<
				"Depth: " << curDepth <<
				", eval: " << Eval::EvalToString(bestEval, 1) <<
				", bestmove: " << (newBestMoveIndex + 1) <<
				", searched: " << Util::NumToStr(searchInfo.totalSearched) << "/" << Util::NumToStr(searchInfo.totalPruned) <<
				", moves/sec: " << Util::NumToStr(movesPerSecond) <<
				", winfrac: " << searchInfo.GetWinFrac() <<
				", tablefrac: " << searchInfo.GetTableHitFrac() <<
				", time: " << timeElapsed
			);

			if (abs(bestEval) >= WIN_MIN_VALUE) {
				break;
			}
		}
	}

	return result;
}

int main() {

	Eval::Init();

	BoardState board = {};

	bool computerOnly = false;
	double computerSearchTime = 2.f;

	auto table = TranspositionTable(300);

	std::string movesStr = {};

	while (true) {
		LOG(board);
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
			auto searchResult = Search(table, board, false, computerSearchTime, true);

			int chosenMoveIndex;
			{
				// Pick the move with the highest eval
				// If multiple moves have the best evla, pick one at random

				int maxEval = -INT_MAX;
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