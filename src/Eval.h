#pragma once

#include "BoardState.h"
#include <numeric>

struct Value {
	int8_t val;
	uint8_t depth;

	constexpr Value(int8_t val = 0, uint8_t depth = 0) : val(val), depth(depth) {}

	constexpr operator int8_t() const {
		return val;
	}

	constexpr operator int8_t&() {
		return val;
	}

	constexpr Value operator-() const {
		return { -val, depth };
	}

	////////////////////////////////////

	friend std::ostream& operator<<(std::ostream& stream, const Value& eval) {
		if (eval.val != 0) {
			stream << ((eval.val > 0) ? "WINNING" : "LOSING") << "(" << (int)eval.depth << ")";
		} else {
			stream << "DRAW";
		}
		return stream;
	}
};

constexpr Value VALUE_INVALID = INT8_MIN;

namespace Eval {
	void Init();
	bool IsWonAfterMove(const BoardState& board);

	// Returns the eval if win/loss/draw, else returns VALUE_INVALID 
	// Modifies validMovesMask if moves are forced
	Value EvalValidMoves(BoardMask hbSelf, BoardMask hbOpp, BoardMask selfWinMask, BoardMask oppWinMask, BoardMask& validMovesMask);
	int RateMove(BoardMask hbSelf, BoardMask hbOpp, BoardMask moveMask);
}