#pragma once

#include "Framework.h"

#include "BoardState.h"
#include "Eval.h"
#include "Timer.h"
#include "TranspositionTable.h"

uint64_t RandU64() {
	thread_local static auto engine = std::default_random_engine();
	thread_local static auto distribution = std::uniform_int_distribution<unsigned long long>(
		std::numeric_limits<std::uint64_t>::min(),
		std::numeric_limits<std::uint64_t>::max()
	);

	return distribution(engine);
}

uint64_t PerfTest(const BoardState& board, int depth, int depthElapsed = 0) {
	if (abs(Eval::EvalBoard(board, true)) >= WIN_MIN_VALUE)
		return 0;

	uint64_t count = 0;
	for (int i = 0; i < BOARD_SIZE_X; i++) {
		if (board.IsMoveValid(i)) {
			if (depth > 1) {
				BoardState nextBoard = board;
				nextBoard.DoMove(i);
				count += PerfTest(nextBoard, depth - 1, depthElapsed + 1);
			} else {
				count++;
			}
		}
	}

	return count;
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

int AlphaBetaSearch(
	TranspositionTable& table, const BoardState& board, int depthRemaining, 
	AccumSearchInfo& outInfo,
	int alpha = -INT_MAX, int beta = INT_MAX, int depthElapsed = 0) {
	outInfo.totalSearched++;

	int eval = Eval::EvalBoard(board, depthRemaining > 0);
	bool someoneWon = abs(eval) >= WIN_MIN_VALUE;
	if (someoneWon)
		outInfo.totalWins++;

	if (depthRemaining == 0 || someoneWon)
		return eval;

	bool doTableLookup = depthRemaining >= 3;
	TranspositionTable::Entry* tableEntry = NULL;

	if (doTableLookup) {
		outInfo.totalTableSeaches++;
		auto hash = table.HashBoard(board);
		tableEntry = table.Find(hash);

		if (tableEntry->fullHash == hash && tableEntry->depthRemaining >= depthRemaining) {
			outInfo.totalTableHits++;
			return tableEntry->eval;
		} else {
			tableEntry->fullHash = hash;
			tableEntry->time = table.timeCounter;
			tableEntry->depthRemaining = depthRemaining;
			// Eval will be set after the search
		}
	}

	int bestMove = 0;
	int bestEval = -INT_MAX;
	auto fnProgressDepth = [&](const BoardState& nextBoard, int moveIndex) {
		
		bool doTableLookup = depthRemaining >= 3;

		int nextEval = -AlphaBetaSearch(table, nextBoard, depthRemaining - 1, outInfo, -beta, -alpha, depthElapsed + 1);

		if (nextEval > bestEval) {
			bestEval = nextEval;
			bestMove = moveIndex;
		}
		if (nextEval > alpha)
			alpha = nextEval;
		if (bestEval >= beta) {
			outInfo.totalPruned++;
			return true;
		} else {
			return false;
		}
	};

	bool sortMoves = depthRemaining >= 3;

	struct Move {
		int index;
		int eval;
	};

	Move moves[BOARD_SIZE_X];
	if (sortMoves) {
		for (int i = 0; i < BOARD_SIZE_X; i++) {
			if (!board.IsMoveValid(i))
				moves[i] = { i, -INT_MAX };

			moves[i] = { i, Eval::EvalMove(board, i) };
		}

		// Insertion sort the moves
		for (size_t i = 1; i < BOARD_SIZE_X; i++) {
			for (size_t j = i; j > 0;) {
				Move prev = moves[j - 1];
				Move cur = moves[j];

				if (prev.eval < cur.eval) {
					// Swap
					moves[j - 1] = cur;
					moves[j] = prev;
					j--;
				} else {
					break;
				}
			}
		}
	}

	for (int i = 0; i < BOARD_SIZE_X; i++) {
		int move;
		if (sortMoves) {
			move = moves[i].index;
		} else {
			// Very basic scroll so that we start at the middle
			move = (i + BOARD_SIZE_X / 2) % BOARD_SIZE_X;
		}

		if (!board.IsMoveValid(move))
			continue;

		BoardState nextBoard = board;
		nextBoard.DoMove(move);
		if (fnProgressDepth(nextBoard, move))
			break;
	}

	if (tableEntry) {
		tableEntry->eval = bestEval;
		tableEntry->bestMove = bestMove;
	}

	if (abs(bestEval) >= WIN_MIN_VALUE) {
		bestEval -= (bestEval > 0) ? 1 : -1;
	}

	return bestEval;
}

int MinMaxSearch(
	const BoardState& board, int depthRemaining, AccumSearchInfo& outInfo, int depthElapsed = 0) {

	outInfo.totalSearched++;

	int eval = Eval::EvalBoard(board, true);
	bool isWin = abs(eval) >= WIN_MIN_VALUE;
	outInfo.totalWins += isWin;

	if (depthRemaining == 0 || isWin)
		return eval;

	int max = -INT_MAX;
	for (int i = 0; i < BOARD_SIZE_X; i++) {
		int move = i;

		if (!board.IsMoveValid(move))
			continue;

		BoardState nextBoard = board;
		nextBoard.DoMove(move);
		
		int nextEval = -MinMaxSearch(nextBoard, depthRemaining - 1, outInfo, depthElapsed + 1);
		max = MAX(max, nextEval);
	}
	
	return max;
}

struct SearchResult {
	int evals[BOARD_SIZE_X];
	int bestMove = 0;
};

SearchResult Search(TranspositionTable& table, const BoardState& board, double maxTime, bool log) {

	SearchResult result = {};

	Timer overallTimer = {};
	double lastMoveSearchTime = 0;

	for (int curDepth = 1;; curDepth++) {
		
		Timer curTimer = {};
		int newBestMove = 0;
		int bestEval = -INT_MAX;
		AccumSearchInfo searchInfo = {};
		for (int i = 0; i < BOARD_SIZE_X; i++) {

			if (!board.IsMoveValid(i)) {
				result.evals[i] = -INT_MAX;
				continue;
			}

			BoardState nextBoard = board;
			nextBoard.DoMove(i);

			Timer moveTimer = {};
			int eval = -AlphaBetaSearch(table, nextBoard, curDepth - 1, searchInfo);
			lastMoveSearchTime = moveTimer.Elapsed();

			result.evals[i] = eval;
			
			if (eval > bestEval) {
				bestEval = eval;
				newBestMove = i;
			}

			if (overallTimer.Elapsed() + lastMoveSearchTime >= maxTime)
				return result;
		}
		uint64_t movesPerSecond = searchInfo.totalSearched / MAX(curTimer.Elapsed(), 1e-7);
		result.bestMove = newBestMove;

		int movesToWin = 0;

		LOG(
			std::setprecision(5) <<
			"Depth: " << curDepth << 
			", eval: " << Eval::EvalToString(bestEval, 1) << 
			", searched: " << searchInfo.totalSearched <<
			", pruned: " << searchInfo.totalPruned <<
			", moves/sec: " << (movesPerSecond / 1'000'000.0) << "m" <<
			", winfrac: " << searchInfo.GetWinFrac() <<
			", tablefrac: " << searchInfo.GetTableHitFrac()
		);

		if (abs(bestEval) >= WIN_MIN_VALUE) {
			break;
		}
	}

	return result;
}

int main() {

	Eval::Init();

	BoardState board = {};
	
	auto table = TranspositionTable(700);

	bool computerOnly = true;
	double computerSearchTime = 3.0f;

	while (true) {
		LOG(board);

		bool humansTurn = !board.turnSwitch && !computerOnly;

		if (abs(Eval::EvalBoard(board, true)) >= WIN_MIN_VALUE) {
			LOG("Game over. " << ((humansTurn || computerOnly) ? "Computer" : "Human") << " won!");
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
			auto searchResult = Search(table, board, computerSearchTime, true);
			LOG("Playing move: " << searchResult.bestMove);
			board.DoMove(searchResult.bestMove);
		} else {
			// Human can move
			while (true) {
				std::cout << "Your move index: ";
				std::string line;
				std::getline(std::cin, line);
				int moveIndex;
				bool invalid = false;
				try {
					moveIndex = std::stoi(line);
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

				board.DoMove(moveIndex);
				break;
			}
		}
	}

	return EXIT_SUCCESS;
}