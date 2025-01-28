#pragma once

#include "Framework.h"

#include "Search.h"
#include "DataStream.h"
#include "Testing.h"

int main(int argc, char* argv[]) {

	bool doTesting = false;

	// Parse args
	for (int i = 1; i < argc; i++) {
		std::string arg = argv[i];
		if (arg == "-t")
			doTesting = true;
	}

	Eval::Init();

	BoardState board = {};

	bool computerOnly = false;

	auto table = new TranspositionTable();

	if (doTesting) {
		Testing::TestEfficiency(table);
		Testing::TestMoveEval(table);
		return EXIT_SUCCESS;
	}

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
			auto searchResult = Search::Search(table, board, false, true);

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