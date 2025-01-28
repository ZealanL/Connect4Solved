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
	bool computerOnly = true;

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

		int chosenMoveIndex;
		if (!humansTurn) {
			auto searchResult = Search::Search(table, board, true);

			int idx = Util::BitMaskToIndex(searchResult.move);
			chosenMoveIndex = idx / 8;
			LOG("Playing move: " << chosenMoveIndex + 1);
		} else {
			// Human can move
			while (true) {
				std::cout << "Your move index: ";
				std::string line;
				std::getline(std::cin, line);
				bool invalid = false;
				try {
					chosenMoveIndex = std::stoi(line) - 1;
				} catch (std::exception& e) {
					LOG("Invalid move (cannot parse)");
					continue;
				}

				if (chosenMoveIndex < 0 || chosenMoveIndex >= BOARD_SIZE_X) {
					LOG("Invalid move (out of range)");
					continue;
				}

				if (!board.IsMoveValid(chosenMoveIndex)) {
					LOG("Invalid move (column full)");
					continue;
				}

				break;
			}
		}

		movesStr += '1' + chosenMoveIndex;
		board.DoMove(chosenMoveIndex);
	}

	return EXIT_SUCCESS;
}