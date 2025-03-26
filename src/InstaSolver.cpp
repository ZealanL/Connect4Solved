#include "InstaSolver.h"

using namespace InstaSolver;

constexpr BoardMask EVEN_ROWS = 0x5555555555555555 & BoardMask::GetBoardMask();
constexpr BoardMask ODD_ROWS = ~EVEN_ROWS & BoardMask::GetBoardMask();

uint8_t GetFirstBit(uint8_t val) {
	return val & -(int8_t)val;
}

bool CheckSingleColumn(const BoardState& board, InstaSolver::Result& outResult) {
	if (board.moveCount < BOARD_CELL_COUNT - BOARD_SIZE_Y)
		return false; // Must be more than a column left

	auto combinedMask = board.GetCombinedMask();
	auto nextMoveMask = board.GetValidMoveMask();
	
	if (Util::BitCount64(nextMoveMask) != 1)
		return false;

	int nextMoveIdx = Util::BitMaskToIndex(nextMoveMask);
	int row = Util::BitMaskToIndex(nextMoveMask) / 8;
	bool nextMoveEven = Util::BitMaskToIndex(nextMoveMask) % 2;

	uint8_t winColumns[2] = {
		BoardMask(board.winMasks[0] & ~combinedMask & EVEN_ROWS).GetColumn(row),
		BoardMask(board.winMasks[1] & ~combinedMask & ODD_ROWS).GetColumn(row)
	};

	int winningTeam = -1;
	if (winColumns[0] || winColumns[1]) {
		// Win for someone
		
		if (winColumns[0] && winColumns[1]) {
			winningTeam = (GetFirstBit(winColumns[0]) <= GetFirstBit(winColumns[1])) ? 0 : 1;
		} else if (winColumns[0]) {
			winningTeam = 0;
		} else {
			winningTeam = 1;
		}

		outResult = Result{
			ResultType::EXACT,
			Value(winningTeam == board.turnSwitch ? 1 : -1, Util::BitCount64(~combinedMask)) // TODO: Wrong move count
		};

	} else {
		// Draw
		outResult = Result{
			ResultType::EXACT,
			Value(0, Util::BitCount64(~combinedMask))
		};
	}

	return true;
}

// https://www.youtube.com/watch?v=mNj6Z5CbUB0
// Doesn't check for ClaimOdd
bool CheckClaimEven(const BoardState& board, InstaSolver::Result& outResult) {
	if (board.turnSwitch != 0)
		return false;

	BoardMask combinedMask = board.GetCombinedMask();
	for (int i = 0; i < BOARD_SIZE_X; i++) {
		uint8_t column = combinedMask.GetColumn(i);

		// Check for uneven column
		if (Util::BitCount64(column) % 2 != 0)
			return false;
	}

	// Opponent can force this game to this end state
	BoardMask playables[2] = {
		(board.teams[0] | EVEN_ROWS) & ~board.teams[1],
		(board.teams[1] | ODD_ROWS) & ~board.teams[0]
	};

	BoardMask selfWin = playables[0] & playables[0].MakeWinMask();
	BoardMask oppWin = playables[1] & playables[1].MakeWinMask();
	
	for (int i = 0; i < BOARD_SIZE_X; i++) {
		uint8_t selfWinColumn = selfWin.GetColumn(i);
		uint8_t oppWinColumn = oppWin.GetColumn(i);

		if (selfWinColumn) {
			if (!oppWinColumn) {
				// We could win here
				return false;
			} else {
				if (GetFirstBit(selfWinColumn) <= GetFirstBit(oppWinColumn)) {
					// We would win first
					return false;
				}
			}
		}
	}

	if (oppWin) {
		// They can force a win
		outResult = Result{
			ResultType::EXACT,
			Value(-1, Util::BitCount64(~combinedMask)) // TODO: The moves to win can be wrong
		};
	} else {
		// They can force a draw
		outResult = Result{
			ResultType::UPPER_BOUND,
			Value(0)
		};
	}
	return true;
}

InstaSolver::Result InstaSolver::Solve(const BoardState& board) {
	
	Result result = { ResultType::NONE, VALUE_INVALID };

	if (
		CheckClaimEven(board, result)
		|| CheckSingleColumn(board, result)
		) {}
	return result;
}
