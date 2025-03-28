#include "InstaSolver.h"

using namespace InstaSolver;

// Detects and solves isolated columns
bool CheckIsolatedColumns(const BoardState& board, InstaSolver::Result& outResult) {

	struct Column {
		uint8_t teamThreats[2];
		uint8_t height;
		uint64_t flags;

		bool HasThreats() const {
			return teamThreats[0] | teamThreats[1];
		}

		bool IsOdd() const {
			return height % 2;
		}

		// Doesn't do anything
		bool IsUseless() const {
			return !HasThreats() && !IsOdd();
		}
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
				(uint8_t)Util::BitCount64(openSpace),
				0
			};

			// Crop column to height
			for (int j = 0; j < 2; j++) {
				int startHeight = BOARD_SIZE_Y - column.height;
				column.teamThreats[j] >>= startHeight;
			}

			if (!column.IsUseless()) {
				ASSERT(numColumns < MAX_COLUMNS, "Bad column count during search");
				columnBuffer[numColumns++] = column;
			}

			lastColumnX = i;
		}
	}
	
	bool anyThreats = false;
	for (int i = 0; i < numColumns; i++) {
		auto& column = columnBuffer[i];
		if (columnBuffer[i].HasThreats()) {
			anyThreats = true;
			break;
		}
	}

	// No threats, draw
	if (!anyThreats) {
		outResult = Result{
			ResultType::EXACT,
			Value(0)
		};
		return true;
	}

	// Single useful column, evaluate
	if (numColumns == 1) {
		auto& column = columnBuffer[0];

		constexpr uint8_t FIRST_PLAYER_MASK = 0x55;
		constexpr uint8_t SECOND_PLAYER_MASK = ~FIRST_PLAYER_MASK;

		uint8_t firstPlayerThreats = column.teamThreats[1] & FIRST_PLAYER_MASK;
		uint8_t secondPlayerThreats = column.teamThreats[0] & SECOND_PLAYER_MASK;
		
		int winningPlayer = -1;
		if (firstPlayerThreats && secondPlayerThreats) {
			winningPlayer = (Util::GetByteFirstBit(firstPlayerThreats) <= Util::GetByteFirstBit(secondPlayerThreats)) ? 1 : 0;
		} else if (firstPlayerThreats) {
			winningPlayer = 1;
		} else if (secondPlayerThreats) {
			winningPlayer = 0;
		}

		if (winningPlayer != -1) {
			// Someone wins
			outResult = Result{
				ResultType::EXACT,
				Value(
					(winningPlayer == (int)board.turnSwitch) ? 1 : -1,
					Util::BitCount64(~combinedMask & BoardMask::GetBoardMask())
				) // TODO: Wrong move count
			};
		} else {
			// It's a draw
			outResult = Result{
				ResultType::EXACT,
				Value(0, 0)
			};
		}
		return true;
	}

	return false;
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
				if (Util::GetByteFirstBit(selfWinColumn) <= Util::GetByteFirstBit(oppWinColumn)) {
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
		|| CheckIsolatedColumns(board, result)
		) {}

	return result;
}