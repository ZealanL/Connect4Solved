#pragma once

#include "BoardState.h"
#include <numeric>

constexpr int WIN_BASE_VALUE = 99999;
constexpr int WIN_MIN_VALUE = 90000;

namespace Eval {
	void Init();
	bool IsWonAfterMove(const BoardState& board);
	int EvalBoard(const BoardState& board, bool winOnly, BoardMask validMoveMask);
	int EvalMove(const BoardState& board, BoardMask moveMask);

	std::string EvalToString(int eval, int addedDepth);
}