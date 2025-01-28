#pragma once

#include "Framework.h"
#include "Util.h"

constexpr int BOARD_SIZE_X = 7, BOARD_SIZE_Y = 6; // Size of the board
constexpr int CONNECT_WIN_AMOUNT = 4; // Number of connections in a row to win
constexpr int BOARD_CELL_COUNT = BOARD_SIZE_X * BOARD_SIZE_Y; // Number of connections in a row to win

static_assert(MAX(BOARD_SIZE_X, BOARD_SIZE_Y) < 8, "Board size must be less than 7 width or height");
static_assert(CONNECT_WIN_AMOUNT >= 3, "Connect win amount must be at least 3");

struct BoardMask {
	uint64_t val64;

	constexpr BoardMask(uint64_t val64 = 0) : val64(val64) {}

	constexpr bool Get(int x, int y) const {
		return (val64 & (1ull << (y + x * 8))) != 0;
	}

	constexpr void Set(int x, int y, bool val) {
		uint64_t bitMask = (1ull << (y + x * 8));
		val64 |= bitMask;
	}

	constexpr uint8_t GetColumn(int x) {
		return (uint8_t)(val64 >> (x * 8));
	}

	constexpr operator uint64_t() const { return val64; }
	constexpr operator uint64_t&() { return val64; }

	constexpr static BoardMask MakeWinMask(BoardMask mask) {
		BoardMask winMask = 0;

		if constexpr (CONNECT_WIN_AMOUNT == 4) {
			// Fast 4-specific solution based on https://github.com/PascalPons/connect4/blob/master/Position.hpp#L300

			// Vertical
			// Only needs to check upward since pieces can't float
			winMask |= (mask << 1) & (mask << 2) & (mask << 3);

			constexpr auto fnShiftMask = [](BoardMask mask, int amount) -> BoardMask {
				if (amount > 0) {
					return mask << amount;
				} else {
					return mask >> -amount;
				}
			};

			constexpr auto fnCheckDir = [fnShiftMask](BoardMask mask, int dx, int dy) -> BoardMask {
				int shift = dx * 8 + dy;
				BoardMask twoFilled = fnShiftMask(mask, shift) & fnShiftMask(mask, shift * 2);
				return (twoFilled & fnShiftMask(mask, -shift)) | (twoFilled & fnShiftMask(mask, shift * 3));
			};

			winMask |= fnCheckDir(mask, 1, 0);
			winMask |= fnCheckDir(mask, -1, 0);

			winMask |= fnCheckDir(mask, -1, -1);
			winMask |= fnCheckDir(mask, -1, 1);
			winMask |= fnCheckDir(mask, 1, -1);
			winMask |= fnCheckDir(mask, 1, 1);

			return winMask & BoardMask::GetBoardMask();
		} else {
			// Slower generic solution
			// TODO: This is very slow because it does unnecessary computations

			constexpr auto fnLoopMask = [](BoardMask mask, int shift, int startShift, int loopAmount) -> BoardMask {

				constexpr auto fnShiftMask = [](BoardMask mask, int shift) -> BoardMask {
					return (shift < 0) ? (mask >> -shift) : (mask << shift);
				};

				BoardMask result = ~0ull;

				for (int shiftItr = 0; shiftItr < loopAmount; shiftItr++) {
					int shiftScale = shiftItr + startShift;
					if (shiftScale == 0)
						continue;
					result &= fnShiftMask(mask, shift * shiftScale);
				}
				return result;
			};

			// Vertical
			winMask |= fnLoopMask(mask, 1, 0, CONNECT_WIN_AMOUNT);

			for (int i = 0; i < CONNECT_WIN_AMOUNT; i++) {
				// Horizontal
				winMask |= fnLoopMask(mask, 8, -i, CONNECT_WIN_AMOUNT);

				// Diag 1 & 2
				winMask |= fnLoopMask(mask, 9, -i, CONNECT_WIN_AMOUNT);
				winMask |= fnLoopMask(mask, 7, -i, CONNECT_WIN_AMOUNT);
			}
		}

		return winMask & GetBoardMask();
	}

	constexpr BoardMask MakeWinMask() const {
		return MakeWinMask(*this);
	}

	constexpr static BoardMask _GetBoardMask() {
		BoardMask result = {};
		for (int x = 0; x < BOARD_SIZE_X; x++)
			for (int y = 0; y < BOARD_SIZE_Y; y++)
				result.Set(x, y, true);
		return result;
	}

	constexpr static BoardMask GetBoardMask() {
		constexpr BoardMask result = _GetBoardMask();
		return result;
	}

	constexpr static BoardMask _GetBottomMask() {
		BoardMask result = {};
		for (int x = 0; x < BOARD_SIZE_X; x++)
			result.Set(x, 0, true);
		return result;
	}

	static constexpr BoardMask GetBottomMask() {
		constexpr BoardMask result = _GetBottomMask();
		return result;
	}
};

struct MoveIterator {
	BoardMask remainingMovesMask;

	MoveIterator(BoardMask validMovesMask) : remainingMovesMask(validMovesMask) {}

	// Returns 0 if there are no remaining moves
	BoardMask GetNext() {
		BoardMask singleMoveMask = (remainingMovesMask & -(int64_t)remainingMovesMask);
		remainingMovesMask &= ~singleMoveMask;
		return singleMoveMask;
	}
};

struct BoardState {
	bool turnSwitch = false;
	int8_t moveCount = 0;
	BoardMask teams[2];
	BoardMask winMasks[2];

	constexpr BoardState(BoardMask team0 = 0, BoardMask team1 = 0) {
		teams[0] = team0;
		teams[1] = team1;
	}

	constexpr BoardMask GetCombinedMask() const {
		return teams[0] | teams[1];
	}

	constexpr bool IsMoveValid(int x) const {
		return !GetCombinedMask().Get(x, BOARD_SIZE_Y - 1);
	}

	constexpr BoardMask GetValidMoveMask() const {
		BoardMask combined = GetCombinedMask();
		return ((combined << 1) | BoardMask::GetBottomMask()) & BoardMask::GetBoardMask() & ~combined;
	}

	uint8_t GetNextY(int x) const {
		uint8_t column = GetCombinedMask().GetColumn(x);

		return Util::BitCount64(column);
	}

	void FillMove(BoardMask moveMask) {
		teams[turnSwitch] |= moveMask;
		winMasks[turnSwitch] = teams[turnSwitch].MakeWinMask();
		turnSwitch = !turnSwitch;
		moveCount++;
	}

	// Doesn't change the board for invalid moves
	void DoMove(int x) {
		uint8_t y = GetNextY(x);
		teams[turnSwitch].Set(x, y, true);
		winMasks[turnSwitch] = teams[turnSwitch].MakeWinMask();
		turnSwitch = !turnSwitch;
		moveCount++;
	}

	// Moves should be column digits starting at 1
	void PlayMoveString(std::string moves) {
		for (char c : moves) {
			if (isblank(c))
				continue;

			int moveIndex = c - '1';
			if (moveIndex < 0 || moveIndex >= BOARD_SIZE_X)
				ERR_CLOSE("PlayMoveString: Bad move character '" << c << "', should be a digit from 1 to " << BOARD_SIZE_X);
			
			if (!IsMoveValid(moveIndex))
				ERR_CLOSE("PlayMoveString: Invalid move index " << moveIndex);

			DoMove(moveIndex);
		}
	
	}

	constexpr bool operator==(const BoardState& other) {
		return teams[0] == other.teams[0] && teams[1] == other.teams[1] && turnSwitch == other.turnSwitch;
	}

	constexpr bool operator!=(const BoardState& other) {
		return !(*this == other);
	}

	friend std::ostream& operator<<(std::ostream& stream, const BoardState& boardState);
};