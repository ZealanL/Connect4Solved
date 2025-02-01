#pragma once

#include "BoardState.h"
#include <numeric>

// If true, values with higher depth will be seen as worse
// This makes searching take longer, but also guarantees finding the fastest win or slowest loss
#define COMPARE_VALUE_DEPTH 0

struct Value {
	int8_t val;
	uint8_t depth;

	constexpr Value(int8_t val = 0, uint8_t depth = 0) : val(val), depth(depth) {}

	constexpr Value operator-() const {
		return { -val, depth };
	}

#if COMPARE_VALUE_DEPTH
	constexpr bool operator==(const Value& other) { return (val == other.val) && (depth == other.depth); }
	constexpr bool operator!=(const Value& other) { return !(*this == other); }

	constexpr bool operator<(const Value& other) { 
		if (val != other.val) {
			return (val < other.val);
		} else {
			return (depth > other.depth);
		}
	}
	constexpr bool operator<=(const Value& other) { return (*this < other) || (*this == other); }
	constexpr bool operator>(const Value& other) { return !(*this <= other); }
	constexpr bool operator>=(const Value& other) { return !(*this < other); }
#else
	constexpr bool operator==(const Value& other) { return val == other.val; }
	constexpr bool operator!=(const Value& other) { return val != other.val; }
	constexpr bool operator>(const Value& other) { return val > other.val; }
	constexpr bool operator>=(const Value& other) { return val >= other.val; }
	constexpr bool operator<(const Value& other) { return val < other.val; }
	constexpr bool operator<=(const Value& other) { return val <= other.val; }
#endif

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
	float RateMove(BoardMask hbSelf, BoardMask hbOpp, BoardMask selfWinMask, BoardMask moveMask, uint8_t moveCount);
}