#pragma once

#include "BoardState.h"
#include "Eval.h"
#include "Timer.h"
#include "TranspositionTable.h"

#include "Util.h"

struct AccumSearchInfo {
	uint64_t totalSearched = 0;
	uint64_t totalTableSeaches = 0;
	uint64_t totalTableHits = 0;
	uint64_t totalWins = 0;
	uint64_t totalPruned = 0; // Times we pruned due to beta

	double GetTableHitFrac() const {
		return (totalTableSeaches > 0) ? (double)totalTableHits / (double)totalTableSeaches : 0;
	}

	double GetWinFrac() const {
		return (double)totalWins / (double)totalSearched;
	}
};

struct SearchCache {
	Value alpha = -1;
	Value beta = 1;
	uint8_t depthElapsed = 0;

	SearchCache ProgressDepth() {
		return SearchCache{
			-beta,
			-alpha,
			(uint8_t)(depthElapsed + 1)
		};
	}
};

struct SearchResult {
	Value evals[BOARD_SIZE_X];
};

namespace Search {
	uint64_t PerfTest(const BoardState& board, int depth, int depthElapsed = 0);
	Value AlphaBetaSearch(TranspositionTable* table, const BoardState& board, AccumSearchInfo& outInfo, SearchCache cache = {});

	SearchResult Search(TranspositionTable* table, const BoardState& board, bool perft, bool log);
}