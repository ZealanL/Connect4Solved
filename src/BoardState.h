#pragma once

#include "Framework.h"

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

	operator uint64_t() const { return val64; }
	operator uint64_t&() { return val64; }

	static constexpr BoardMask GetBoardMask() {
		BoardMask result = {};
		for (int x = 0; x < BOARD_SIZE_X; x++)
			for (int y = 0; y < BOARD_SIZE_X; y++)
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

struct BoardState {
	bool turnSwitch = false;
	uint8_t lastMoveX = 0, lastMoveY = 0;
	BoardMask teams[2];

	constexpr BoardState() {
		teams[0] = {};
		teams[1] = {};
	}

	constexpr BoardState(BoardMask team0, BoardMask team1) {
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
		return (combined << 1) & BoardMask::GetBoardMask() & ~(combined);
	}

	uint8_t GetNextY(int x) const {
		uint8_t column = teams[0].columns[x] | teams[1].columns[x];

		return BITCOUNT32(column);
	}

	// Doesn't change the board for invalid moves
	void DoMove(int x) {
		uint8_t y = GetNextY(x);
		teams[turnSwitch].columns[x] |= 1 << y;
		lastMoveX = x;
		lastMoveY = y;
		turnSwitch = !turnSwitch;
	}

	friend std::ostream& operator<<(std::ostream& stream, const BoardState& boardState);
};