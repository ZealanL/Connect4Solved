#pragma once
#include "BoardState.h"
#include "Eval.h"

// Detects and solves special gamestates that can be instantly solved in O(1) time
namespace InstaSolver {
	enum ResultType {
		NONE = 0, // No solution found

		LOWER_BOUND, // Turn player can guarantee at least this outcome
		UPPER_BOUND, // Opponent player can guarantee at least this outcome

		EXACT // Guaranteed outcome of the state assuming perfect play
	};

	struct Result {
		ResultType type;
		Value eval;
	};

	Result Solve(const BoardState& board);
}