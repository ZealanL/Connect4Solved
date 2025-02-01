#pragma once

#include "BoardState.h"
#include "Eval.h"
#include "Timer.h"
#include "TranspositionTable.h"

#include "Util.h"

constexpr int MAX_DEPTH = BOARD_CELL_COUNT;

struct SearchInfo {
	BoardMask bestMove[MAX_DEPTH] = {};

	uint64_t totalSearched = 0;
	uint64_t totalTableSeaches = 0;
	uint64_t totalTableHits = 0;
	uint64_t totalPruned = 0; // Times we pruned due to beta

	double GetTableHitFrac() const {
		return (totalTableSeaches > 0) ? (double)totalTableHits / (double)totalTableSeaches : 0;
	}
};

struct SearchCache {
	Value min = { -1, MAX_DEPTH };
	Value max = { 1, MAX_DEPTH };
	uint8_t depthElapsed = 0;

	SearchCache ProgressDepth() const {
		return SearchCache{
			-max,
			-min,
			(uint8_t)(depthElapsed + 1)
		};
	}
};

struct SearchResult {
	BoardMask move = 0;
	Value eval;
};

namespace Search {
	uint64_t PerfTest(const BoardState& board, int depth, int depthElapsed = 0);
	Value AlphaBetaSearch(TranspositionTable* table, const BoardState& board, SearchInfo& outInfo, SearchCache cache = {});
	std::vector<BoardMask> FindPVFromTable(TranspositionTable* table, const BoardState& board, BoardMask firstMove);
	SearchResult Search(TranspositionTable* table, const BoardState& board, bool log);
}