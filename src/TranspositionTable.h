#pragma once
#include "BoardState.h"

#define TEST_HASH_COLLISION 0

struct TranspositionTable {
	// Mostly from https://github.com/ZealanL/BoardMouse/blob/main/src/Engine/Transpos/Transpos.h

	struct __declspec(align(1)) Entry {
		uint64_t fullHash;
		int eval;
		uint16_t depthRemaining;
		uint64_t time;

#if TEST_HASH_COLLISION
		BoardMask teams[2];
#endif

		Entry() {
			// I would allow fullHash to be uninitialized, but there's a risk that the memory could have been previously another hash.
			// To fix this, I could zero transposition table memory whenever resized, but what if another engine's memory is used or something?
			fullHash = NULL;
			time = 0;
		}

		bool IsValid() const {
			return fullHash != NULL;
		}

		void Reset() {
			fullHash = NULL;
		}
	};

	// Amount of entries per bucket
	constexpr static int BUCKET_SIZE = 4;

	struct Bucket {
		Entry entries[BUCKET_SIZE];

		Bucket() {
			for (auto& entry : entries)
				entry = {};
		}

		Entry& operator[](size_t index) {
			ASSERT(index < BUCKET_SIZE, STR("Bucket index (" << index << ")"));
			return entries[index];
		}
	};

	// Number of buckets for every megabyte of memory
	constexpr static int BUCKET_COUNT_MB = 1000'000 / sizeof(Bucket);

	// Decrease in depth for old buckets from a pervious search
	constexpr static int OLD_DEPTH_DECREASE = 3;

	////////////////////////////////////////////////////////////////////////

	std::vector<Bucket> buckets = {};
	uint64_t timeCounter = 0;

	TranspositionTable(int memoryMbs) {
		int numBuckets = memoryMbs * BUCKET_COUNT_MB;
		buckets.resize(numBuckets);
		Reset();
	}

	static uint64_t HashBoard(const BoardState& board) {
		// TODO: Not sure how good this is
		constexpr uint64_t GIANT_PRIME = 8494297527100746157;
		return board.teams[0].val64 * GIANT_PRIME + board.teams[1].val64;
	}

	void Reset() {
		for (Bucket& bucket : buckets)
			bucket = Bucket();
	}

	Entry* Find(uint64_t hash);
};