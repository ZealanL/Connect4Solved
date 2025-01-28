#pragma once

#include "Framework.h"

struct DataStream {
	std::ofstream stream;
	DataStream(std::filesystem::path path) {
		stream = std::ofstream(path, std::ios::binary);
	}

	template<typename T>
	void Write(const T& val) {
		stream.write((char*)&val, sizeof(val));
	}

	void WriteRaw(const void* ptr, size_t size) {
		stream.write((char*)ptr, size);
	}
};