#ifndef HALF_FP_HPP
#define HALF_FP_HPP

#include "half.hpp"

inline uint16_t float32_to_float16(float f) {
	half_float::half h = static_cast<half_float::half>(f);
	return *reinterpret_cast<uint16_t *>(&h);
}

inline float float16_to_float32(uint16_t f) {
	return half_float::half(f).operator float();
}

inline uint16_t float32_to_bfloat16(float f) {
	uint32_t floatInt = *reinterpret_cast<uint32_t *>(&f);
	return static_cast<uint16_t>(floatInt >> 16);
}

union float32_storage {
	float f;
	uint32_t i;
};

inline float bfloat16_to_float32(uint16_t f) {
	float32_storage result;
	result.i = f;
	result.i <<= 16;
	return result.f;
}

#endif // HALF_FP_HPP