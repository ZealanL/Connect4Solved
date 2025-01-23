#pragma once
#include "Framework.h"

#ifdef _MSC_VER
#include <intrin.h>
#endif

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

	inline uint64_t BitCount64(uint64_t val) {
#ifdef _MSC_VER
		return __popcnt64(val);
#else
		return __builtin_popcountll(val);
#endif
	}

	inline bool MultipleBitsSet(uint64_t val) {
		// From https://forum.arduino.cc/t/solved-easiest-way-to-check-if-more-than-one-bit-set-in-byte/669962/7
		return (val & (val - 1)) != 0;
	}
}