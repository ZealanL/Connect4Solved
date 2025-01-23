#pragma once

#include "Framework.h"
#include "Util.h"

constexpr int BOARD_SIZE_X = 7, BOARD_SIZE_Y = 6;
constexpr int CONNECT_WIN_AMOUNT = 4;

struct BoardMask {
	union {
		uint64_t val64;
		uint8_t columns[BOARD_SIZE_X];
	};

	constexpr BoardMask(uint64_t val64 = 0) : val64(val64) {}

	constexpr bool Get(int x, int y) const {
		return (columns[x] & (1 << y)) > 0;
	}

	constexpr void Set(int x, int y, bool val) {
		uint8_t& column = columns[x];
		if (val) {
			columns[x] |= 1 << y;
		} else {
			columns[x] &= ~(1 << y);
		}
	}

	constexpr operator uint64_t() const { return val64; }
	constexpr operator uint64_t&() { return val64; }

	static constexpr BoardMask GetWinMask(BoardMask mask, bool oneMinus = false) {
		int loopAmount = oneMinus ? CONNECT_WIN_AMOUNT - 1 : CONNECT_WIN_AMOUNT;

		constexpr auto fnLoopMask = [](BoardMask& mask, int shift, int startShift, int loopAmount) -> BoardMask {

			constexpr auto fnShiftMask = [](BoardMask& mask, int shift) -> BoardMask {
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

		BoardMask winMask = 0;
		// Vertical
		winMask |= fnLoopMask(mask, 1, 0, loopAmount);

		for (int i = 0; i < CONNECT_WIN_AMOUNT; i++) {
			// Horizontal
			winMask |= fnLoopMask(mask, 8, -i, loopAmount);

			// Diag 1 & 2
			winMask |= fnLoopMask(mask, 9, -i, loopAmount);
			winMask |= fnLoopMask(mask, 7, -i, loopAmount);
		}

		return winMask;
	}

	static constexpr BoardMask GetBoardMask() {
		BoardMask result = {};
		for (int x = 0; x < BOARD_SIZE_X; x++)
			for (int y = 0; y < BOARD_SIZE_Y; y++)
				result.Set(x, y, true);
		return result;
	}

	static constexpr BoardMask GetBottomMask() {
		BoardMask result = {};
		for (int x = 0; x < BOARD_SIZE_X; x++)
			result.Set(x, 0, true);
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
	BoardMask teams[2];

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

	BoardMask GetWinMask(bool team, bool oneMinus = false) const {
		return BoardMask::GetWinMask(teams[team], oneMinus);
	}

	uint8_t GetNextY(int x) const {
		uint8_t column = teams[0].columns[x] | teams[1].columns[x];

		return Util::BitCount64(column);
	}

	void FillMove(BoardMask moveMask) {
		teams[turnSwitch] |= moveMask;
		turnSwitch = !turnSwitch;
	}

	// Doesn't change the board for invalid moves
	void DoMove(int x) {
		uint8_t y = GetNextY(x);
		teams[turnSwitch].columns[x] |= 1 << y;
		turnSwitch = !turnSwitch;
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

	friend std::ostream& operator<<(std::ostream& stream, const BoardState& boardState);
};