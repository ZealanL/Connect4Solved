#include "TranspositionTable.h"

TranspositionTable::Entry* TranspositionTable::Find(uint64_t hash) {
	// TODO: Maybe force bucketCount to a power of two and store the number of bits
	//	That way, we can bit shift instead of expensive modulo on two runtime values
	size_t bucketIndex = hash % buckets.size();

	Bucket& bucket = buckets[bucketIndex];

	// Try to find matching entry
	for (size_t i = 0; i < BUCKET_SIZE; i++) {
		Entry& entry = bucket[i];
		if (entry.fullHash == hash)
			return &entry;
	}

	// No entry exists for this position, find the oldest entry to replace
	// TODO: Maybe merge this with the first loop for efficiency (??)
	size_t toReplaceIndex = 0;
	uint16_t lowestTime = bucket[0].time;
	for (size_t i = 1; i < BUCKET_SIZE; i++) {
		Entry& entry = bucket[i];
		if (!entry.IsValid())
			return &entry; // Found empty, use it

		if (entry.time < lowestTime) {
			lowestTime = entry.time;
			toReplaceIndex = i;
		}
	}

	timeCounter++;

	return &(bucket[toReplaceIndex]);
}