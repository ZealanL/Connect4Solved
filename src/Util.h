#pragma once
#include "Framework.h"

namespace Util {
	inline std::string NumToStr(int64_t num) {
		std::stringstream stream;
		stream << std::setprecision(5);
		if (abs(num) >= 1'000'000'000) {
			stream << (num / 1'000'000'000.0) << "b";
		} else if (abs(num) >= 1'000'000) {
			stream << (num / 1'000'000.0) << "m";
		} else if (abs(num) >= 10'000) {
			stream << (num / 1'000.0) << "k";
		} else {
			stream << num;
		}

		return stream.str();
	}

	constexpr uint64_t BitCount64(uint64_t val) {
		return std::popcount(val);
	}

	// Returns the max value if there is no mask
	constexpr uint64_t BitMaskToIndex(uint64_t mask) {
		return std::countr_zero(mask);
	}

	template<size_t MIN_BITS>
	constexpr bool HasMinBitsSet(uint64_t val) {
		// Ref: https://nimrod.blog/posts/algorithms-behind-popcount/
		for (size_t i = 0; i < (MIN_BITS - 1); i++)
			val &= val - 1;
		return val;
	}

	constexpr uint64_t FastHash(uint64_t val, bool alt = false) {
		// Ref: https://github.com/aappleby/smhasher/blob/master/src/MurmurHash3.cpp

		constexpr uint64_t CONST_1 = 0xff51afd7ed558ccd;
		constexpr uint64_t CONST_2 = 0xc4ceb9fe1a85ec53;

		val ^= val >> 33ull;
		val *= alt ? CONST_2 : CONST_1;
		val ^= val >> 33ull;
		val *= alt ? CONST_1 : CONST_2;
		val ^= val >> 33ull;
		return val;
	}
}