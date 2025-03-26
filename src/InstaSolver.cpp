#include "InstaSolver.h"

using namespace InstaSolver;

uint8_t GetFirstBit(uint8_t val) {
	return val & -(int8_t)val;
}

// Detects and solves isolated columns
bool CheckIsolatedColumns(const BoardState& board, InstaSolver::Result& outResult) {

	struct Column {
		uint8_t teamThreats[2];
		uint8_t height;
	};

	// More columns than this will guarantee potential cross-column interference
	constexpr int MAX_COLUMNS = BOARD_SIZE_X / CONNECT_WIN_AMOUNT + 1;

	// Columns must be at least this far apart in spacing
	constexpr int MIN_COLUMN_SPACING = CONNECT_WIN_AMOUNT;

	auto combinedMask = board.GetCombinedMask();
	auto nextMoveMask = board.GetValidMoveMask();

	int openColumns = Util::BitCount64(nextMoveMask);
	if (openColumns == 0 || openColumns > MAX_COLUMNS)
		return false;
	
	// Collect columns
	Column columnBuffer[MAX_COLUMNS];
	int numColumns = 0;
	{
		int lastColumnX = -MIN_COLUMN_SPACING;
		for (int i = 0; i < BOARD_SIZE_X; i++) { // TODO: Iterate over bits of nextMoveMask instead
			auto columnMask = BoardMask::GetColumnMask(i);
			auto openSpace = (columnMask & ~combinedMask);
			bool isOpenColumn = openSpace != 0;

			if (!isOpenColumn)
				continue;

			if (i - lastColumnX < MIN_COLUMN_SPACING) {
				// Columns are too close
				return false;
			}

			// Construct the column
			auto column = Column{
				board.winMasks[0].GetColumn(i),
				board.winMasks[1].GetColumn(i),
				(uint8_t)Util::BitCount64(openSpace)
			};

			// Crop column to height
			for (int j = 0; j < 2; j++) {
				int startHeight = BOARD_SIZE_Y - column.height;
				column.teamThreats[j] >>= startHeight;
			}

			ASSERT(numColumns < MAX_COLUMNS, "Bad column count during search");
			columnBuffer[numColumns++] = column;

			lastColumnX = i;
		}
	}

	// TODO: Analyze columns

	bool anyThreats = false;
	for (int i = 0; i < numColumns; i++) {
		auto& column = columnBuffer[i];
		if (column.teamThreats[0] || column.teamThreats[1]) {
			anyThreats = true;
			break;
		}
	}

	if (!anyThreats) {
		// Draw
		outResult = Result{
			ResultType::EXACT,
			Value(0)
		};
		return true;
	}

	return false;
}

// Detects and solves single columns
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
		BoardMask(board.winMasks[0] & ~combinedMask & BoardMask::GetParityRows(true)).GetColumn(row),
		BoardMask(board.winMasks[1] & ~combinedMask & BoardMask::GetParityRows(false)).GetColumn(row)
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
			Value((winningTeam == (int)board.turnSwitch) ? 1 : -1, Util::BitCount64(~combinedMask & BoardMask::GetBoardMask())) // TODO: Wrong move count
		};

	} else {
		// Draw
		outResult = Result{
			ResultType::EXACT,
			Value(0, Util::BitCount64(~combinedMask & BoardMask::GetBoardMask()))
		};
	}

	return true;
}

// Detects and solves ClaimEven positions
// Ref: https://www.youtube.com/watch?v=mNj6Z5CbUB0
// TODO: Doesn't check for ClaimOdd
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
		(board.teams[0] | BoardMask::GetParityRows(true)) & ~board.teams[1],
		(board.teams[1] | BoardMask::GetParityRows(false)) & ~board.teams[0]
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
			Value(-1, Util::BitCount64(~combinedMask & BoardMask::GetBoardMask())) // TODO: The moves to win can be wrong
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
		|| CheckIsolatedColumns(board, result)
		) {}

	return result;
}
