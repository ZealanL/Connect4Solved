#pragma once

#include "Search.h"

namespace Testing {
	BoardState GeneratePosition(int numMoves);

	void TestMoveEval(TranspositionTable* table, int numSamples = 50);
	void TestEfficiency(TranspositionTable* table, int numSamples = 50);
}