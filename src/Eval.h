#pragma once

#include "BoardState.h"
#include <numeric>

constexpr int WIN_BASE_VALUE = 99999;
constexpr int WIN_MIN_VALUE = 90000;

namespace Eval {
	void Init();
	int EvalBoard(const BoardState& state, bool winOnly);
	int EvalMove(const BoardState& state, int move);

	std::string EvalToString(int eval, int addedDepth);
}