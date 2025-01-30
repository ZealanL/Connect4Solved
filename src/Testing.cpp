#include "Testing.h"

BoardState Testing::GeneratePosition(int numMoves) {
	BoardState board;
	while (true) { // Loop until we find the best move
		board = {};

		for (int i = 0; i < numMoves; i++) {
			BoardMask validMovesMask = board.GetValidMoveMask();
			Value value = Eval::EvalValidMoves(
				board.teams[board.turnSwitch],
				board.teams[!board.turnSwitch],
				board.winMasks[board.turnSwitch],
				board.winMasks[!board.turnSwitch],
				validMovesMask
			);

			if (value != VALUE_INVALID) {
				// We detected a simple win/loss, retry
				continue;
			}

			BoardMask chosenMove;
			if (!Util::HasMinBitsSet<2>(validMovesMask)) {
				// Only one legal move, just play it
				chosenMove = validMovesMask;
			} else {

				BoardMask moves[BOARD_SIZE_X];
				int numMoves = 0;

				MoveIterator moveItr = MoveIterator(validMovesMask);
				while (BoardMask move = moveItr.GetNext())
					moves[numMoves++] = move;

				chosenMove = moves[rand() % numMoves];
			}

			board.FillMove(chosenMove);
		}

		break;
	}

	return board;
}

void Testing::TestMoveEval(TranspositionTable* table, int numSamples) {
	LOG("Running move eval test...");
	srand(0);
	Timer timer = {};
	table->Reset();

	constexpr int DEPTHS[] = { 18, 22, 25 };

	for (int depth : DEPTHS) {
		size_t numFound = 0;

		for (int i = 0; i < numSamples; i++) {
			BoardState board = Testing::GeneratePosition(depth);

			// Do normal search to assess eval
			SearchInfo searchInfo = {};
			Value eval = Search::AlphaBetaSearch(table, board, searchInfo);

			// Find highest rated move
			MoveIterator moveItr = MoveIterator(board.GetValidMoveMask());
			BoardMask bestMove = {};
			int bestMoveEval = -INT_MAX;
			while (BoardMask move = moveItr.GetNext()) {
				int moveEval = Eval::RateMove(
					board.teams[board.turnSwitch],
					board.teams[!board.turnSwitch],
					board.winMasks[board.turnSwitch],
					move,
					board.moveCount
				);

				if (moveEval > bestMoveEval) {
					bestMoveEval = moveEval;
					bestMove = move;
				}
			}

			// If the move is the best move, the eval after the move will be equal to the original eval (but negative)
			BoardState nextBoard = board;
			nextBoard.FillMove(bestMove);
			Value nextEval = Search::AlphaBetaSearch(table, nextBoard, searchInfo);

			if (eval == -nextEval)
				numFound++;
		}

		double frac = (double)numFound / (double)numSamples;
		LOG(" > Depth " << depth << ", guessed " << (frac * 100) << "%");
	}

	LOG(" Done in " << timer.Elapsed() << "s");
}

void Testing::TestEfficiency(TranspositionTable* table, int numSamples) {
	LOG("Running overall efficiency test...");
	srand(0);
	Timer timer = {};
	table->Reset();

	constexpr double GOOD_BRANCHING_FACTOR = 1.6f;

	constexpr int DEPTHS[] = { 16, 20, 25 };

	for (int depth : DEPTHS) {
		SearchInfo searchInfo = {};

		int movesRemaining = MAX(BOARD_CELL_COUNT - depth  - 2, 1);
		uint64_t targetSearchCount = pow(GOOD_BRANCHING_FACTOR, movesRemaining);

		for (int i = 0; i < numSamples; i++) {
			BoardState board = Testing::GeneratePosition(depth);

			// Do normal search to assess eval
			Value eval = Search::AlphaBetaSearch(table, board, searchInfo);
		}

		uint64_t avgSearched = searchInfo.totalSearched / numSamples;
		double scoreFrac = (double)targetSearchCount / (double)avgSearched;
		LOG(" > Depth " << depth << ", score: " << scoreFrac << ", avg searched: " << Util::NumToStr(avgSearched) << ", table hit frac: " << searchInfo.GetTableHitFrac());
	}

	LOG(" Done in " << timer.Elapsed() << "s");
}