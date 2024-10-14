#include "ekjson.h"

#define STR2U32(A, B, C, D) ((A) | ((B) << 8) | ((C) << 16) | ((D) << 24))

typedef struct state {
	const char *base, *src;
	ejtok_t *tbase, *tend, *t;
} state_t;

static inline uint32_t ldu32_unaligned(const void *const buf) {
	const uint8_t *const bytes = buf;
	return (uint32_t)bytes[0]
		| (uint32_t)bytes[1] << 8
		| (uint32_t)bytes[2] << 16
		| (uint32_t)bytes[3] << 24;
}

// Bit twiddling hacks - https://graphics.stanford.edu/~seander/bithacks.html
#define haszero(v) (((v) - 0x0101010101010101ull) & ~(v) & 0x8080808080808080ull)
#define hasvalue(x,n) (haszero((x) ^ (~0ull/255 * (n))))
#define hasless(x,n) (((x)-~0UL/255*(n))&~(x)&~0UL/255*128)

static inline uint64_t ldu64_unaligned(const void *const buf) {
	const uint8_t *const bytes = buf;
	return (uint64_t)bytes[0] | (uint64_t)bytes[1] << 8
		| (uint64_t)bytes[2] << 16 | (uint64_t)bytes[3] << 24
		| (uint64_t)bytes[4] << 32 | (uint64_t)bytes[5] << 40
		| (uint64_t)bytes[6] << 48 | (uint64_t)bytes[7] << 56;
}

static const char *whitespace(const char *src) {
	for (; *src == ' ' || *src == '\t'
		|| *src == '\r' || *src == '\n'; src++);
	return src;
}

static ejtok_t *addtok(state_t *const state, int type) {
	*state->t = (ejtok_t){
		.type = type,
		.len = 1,
		.start = state->src - state->base,
	};
	ejtok_t *const t = state->t;
	state->t += state->t != state->tend;
	return t;
}

#define FILLCODESPACE(I) \
	I, I, I, I, I, I, I, I, I, I, I, I, I, I, I, I, \
	I, I, I, I, I, I, I, I, I, I, I, I, I, I, I, I, \
	I, I, I, I, I, I, I, I, I, I, I, I, I, I, I, I, \
	I, I, I, I, I, I, I, I, I, I, I, I, I, I, I, I, \
	I, I, I, I, I, I, I, I, I, I, I, I, I, I, I, I, \
	I, I, I, I, I, I, I, I, I, I, I, I, I, I, I, I, \
	I, I, I, I, I, I, I, I, I, I, I, I, I, I, I, I, \
	I, I, I, I, I, I, I, I, I, I, I, I, I, I, I, I,

static ejtok_t *string(state_t *const state, int type) {
#if EKJSON_SPACE_EFFICENT
	static const uint8_t groups[256] = {
		['\\'] = 1,
		['u'] = 2,
		['0'] = 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
		['a'] = 3, 3, 3, 3, 3, 3,
		['A'] = 3, 3, 3, 3, 3, 3,
		['"'] = 4,
		['\0'] = 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
		5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
	};
	static const uint8_t transitions[][8] = {
		{ 0, 1, 0, 0, 7, 6 }, // Normal string
		{ 0, 0, 2, 0, 0, 6 }, // Found escape char '\\'
		{ 6, 6, 6, 3, 6, 6 }, // utf hex
		{ 6, 6, 6, 4, 6, 6 }, // utf hex
		{ 6, 6, 6, 5, 6, 6 }, // utf hex
		{ 6, 6, 6, 0, 6, 6 }, // utf hex
	};
#else
	static const uint8_t transitions[][256] = {
		// Normal string
		{
			['\0'] = 6,
			['\\'] = 1,
			['"'] = 7,
		},
		// Escape
		{
			6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
			6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
			6, 6, 0, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 0,
			6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
			6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
			6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 0, 6, 6, 6,
			6, 6, 0, 6, 6, 6, 0, 6, 6, 6, 6, 6, 6, 6, 0, 6,
			6, 6, 0, 6, 0, 2, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
			FILLCODESPACE(6)
		},
		// UTF Hex Digit 1
		{
			6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
			6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
			6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
			3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 6, 6, 6, 6, 6, 6,
			6, 3, 3, 3, 3, 3, 3, 6, 6, 6, 6, 6, 6, 6, 6, 6,
			6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
			6, 3, 3, 3, 3, 3, 3, 6, 6, 6, 6, 6, 6, 6, 6, 6,
			6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
			FILLCODESPACE(6)
		},
		// UTF Hex Digit 2
		{
			6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
			6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
			6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
			4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 6, 6, 6, 6, 6, 6,
			6, 4, 4, 4, 4, 4, 4, 6, 6, 6, 6, 6, 6, 6, 6, 6,
			6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
			6, 4, 4, 4, 4, 4, 4, 6, 6, 6, 6, 6, 6, 6, 6, 6,
			6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
			FILLCODESPACE(6)
		},
		// UTF Hex Digit 3
		{
			6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
			6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
			6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
			5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6,
			6, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 6, 6, 6,
			6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
			6, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 6, 6, 6,
			6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
			FILLCODESPACE(6)
		},
		// UTF Hex Digit 4
		{
			6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
			6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
			6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6, 6, 6, 6, 6, 6,
			6, 0, 0, 0, 0, 0, 0, 6, 6, 6, 6, 6, 6, 6, 6, 6,
			6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
			6, 0, 0, 0, 0, 0, 0, 6, 6, 6, 6, 6, 6, 6, 6, 6,
			6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
			FILLCODESPACE(6)
		},
	};
#endif

	ejtok_t *const tok = addtok(state, type);
	const char *src = state->src + 1;

#if !EKJSON_NO_BITWISE
	uint64_t probe = ldu64_unaligned(src);
	while (!(hasless(probe, 0x20)
		|| hasvalue(probe, '"')
		|| hasvalue(probe, '\\'))) {
		src += 8;
		probe = ldu64_unaligned(src);
	}
#endif

	int s = 0;

	do {
#if EKJSON_SPACE_EFFICENT
		s = transitions[s][groups[(uint8_t)(*src++)]];
#else
		s = transitions[s][(uint8_t)(*src++)];
#endif
	} while (s < 6);

	state->src = src;
	return s == 7 ? tok : NULL;
}

static ejtok_t *number(state_t *const state) {
#if EKJSON_SPACE_EFFICENT
	static const uint8_t groups[256] = {
		['-'] = 1, ['0'] = 2, ['1'] = 3, 3, 3, 3, 3, 3, 3, 3, 3,
		['.'] = 4, ['e'] = 5, ['E'] = 5, ['+'] = 6,
	};
	static const uint8_t transitions[][8] = {
		{  9,  1,  2,  3,  9,  9,  9 }, // Initial checks

		{  9,  9,  2,  3,  9,  9,  9 }, // Negative sign
		{ 10,  9,  9,  9,  4,  6,  9 }, // Initial zero
		{ 10,  9,  3,  3,  4,  6,  9 }, // Digits

		{  9,  9,  5,  5,  9,  9,  9 }, // Fraction (first part)
		{ 10,  9,  5,  5,  9,  6,  9 }, // Fraction (second part)

		{  9,  9,  7,  7,  9,  9,  7 }, // Exponent (+/-)
		{  9,  9,  8,  8,  9,  9,  9 }, // Exponent (first digit)
		{ 10,  9,  8,  8,  9,  9,  9 }, // Exponent (rest of digits)
	};
#else
	static const uint8_t transitions[][256] = {
		// Initial checks
		{
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x1, 0x9, 0x9,
			0x2, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3,
			0x3, 0x3, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			FILLCODESPACE(0x9)
		},
		// Negative sign
		{
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x2, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3,
			0x3, 0x3, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			FILLCODESPACE(0x9)
		},
		// Initial zero
		{
			0xA, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0xA, 0xA, 0x9, 0x9, 0xA, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0xA, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0xA, 0x9, 0x4, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x6, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0xA, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0xA, 0x9, 0x9,
			FILLCODESPACE(0x9)
		},
		// Digits
		{
			0xA, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0xA, 0xA, 0x9, 0x9, 0xA, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0xA, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0xA, 0x9, 0x4, 0x9,
			0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3,
			0x3, 0x3, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x6, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0xA, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x6, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0xA, 0x9, 0x9,
			FILLCODESPACE(0x9)
		},
		// Fraction (first part)
		{
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x5, 0x5, 0x5, 0x5, 0x5, 0x5, 0x5, 0x5,
			0x5, 0x5, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			FILLCODESPACE(0x9)
		},
		// Fraction (second part)
		{
			0xA, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0xA, 0xA, 0x9, 0x9, 0xA, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0xA, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0xA, 0x9, 0x4, 0x9,
			0x5, 0x5, 0x5, 0x5, 0x5, 0x5, 0x5, 0x5,
			0x5, 0x5, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x6, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0xA, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x6, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0xA, 0x9, 0x9,
			FILLCODESPACE(0x9)
		},
		// Exponent (+/-)
		{
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x7, 0x9, 0x7, 0x9, 0x9,
			0x8, 0x8, 0x8, 0x8, 0x8, 0x8, 0x8, 0x8,
			0x8, 0x8, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			FILLCODESPACE(0x9)
		},
		// Exponent (first digit)
		{
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x8, 0x8, 0x8, 0x8, 0x8, 0x8, 0x8, 0x8,
			0x8, 0x8, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			FILLCODESPACE(0x9)
		},
		// Exponent (rest of digits)
		{
			0xA, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0xA, 0xA, 0x9, 0x9, 0xA, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0xA, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0xA, 0x9, 0x9, 0x9,
			0x8, 0x8, 0x8, 0x8, 0x8, 0x8, 0x8, 0x8,
			0x8, 0x8, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0xA, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9,
			0x9, 0x9, 0x9, 0x9, 0x9, 0xA, 0x9, 0x9,
			FILLCODESPACE(0x9)
		},
	};
#endif

	ejtok_t *const tok = addtok(state, EJNUM);
	const char *src = state->src;

	int s = 0;
	while (s < 9) {
#if EKJSON_SPACE_EFFICENT
		s = transitions[s][groups[(uint8_t)(*src++)]];
#else
		s = transitions[s][(uint8_t)(*src++)];
#endif
	}

	state->src = src - 1;
	return s == 10 ? tok : NULL;
}

static ejtok_t *boolean(state_t *const state) {
	ejtok_t *const tok = addtok(state, EJBOOL);
	const bool bfalse =
		(state->src[4] == 'e')
		& (ldu32_unaligned(state->src) == STR2U32('f', 'a', 'l', 's'));
	const bool bvalid = bfalse
		| (ldu32_unaligned(state->src) == STR2U32('t', 'r', 'u', 'e'));
	const uint64_t valid = bvalid * (uint64_t)(-1);

	state->src += 4 * bvalid + bfalse;
	return (ejtok_t *)((uint64_t)tok & valid);
}

static ejtok_t *null(state_t *const state) {
	ejtok_t *const tok = addtok(state, EJNULL);
	const bool bvalid =
		ldu32_unaligned(state->src) == STR2U32('n', 'u', 'l', 'l');
	const uint64_t valid = bvalid * (uint64_t)(-1);

	state->src += 4 * bvalid;
	return (ejtok_t *)((uint64_t)tok & valid);
}

static ejtok_t *value(state_t *const state, const int depth) {
	ejtok_t *tok = NULL;

	state->src = whitespace(state->src);
	switch (*state->src) {
	case '{':
		tok = addtok(state, EJOBJ);
		state->src = whitespace(state->src + 1);

		while (*state->src != '}') {
			ejtok_t *const key = string(state, EJKV);
			if (*state->src != ':') {
				state->src = whitespace(state->src);
				if (*state->src++ != ':') return NULL;
			} else {
				state->src++;
			}
			const ejtok_t *const val = value(state, depth + 1);
			if (!(key && val)) return NULL;
			key->len += val->len;
			tok->len += val->len + 1;
 			if (*state->src == ',') {
				state->src = whitespace(state->src + 1);
			}
		}

		state->src++;
		break;
	case '[':
		tok = addtok(state, EJARR);
		state->src = whitespace(state->src + 1);

		while (*state->src != ']') {
			const ejtok_t *const val = value(state, depth + 1);
			if (!val) return NULL;
			tok->len += val->len;
			if (*state->src == ',') state->src++;
		}

		state->src++;
		break;
	case '"':
		tok = string(state, EJSTR);
		break;
	case '-': case '0': case '1': case '2': case '3': case '4':
	case '5': case '6': case '7': case '8': case '9':
		tok = number(state);
		break;
	case 't': case 'f':
		tok = boolean(state);
		break;
	case 'n':
		tok = null(state);
		break;
	case '\0':
		if (depth) return NULL;
		else return (void *)1;
		break;
	default:
		return NULL;
	}

	state->src = whitespace(state->src);
	return tok;
}

ejresult_t ejparse(const char *src, ejtok_t *t, size_t nt) {
	state_t state = {
		.base = src, .src = src,
		.tbase = t, .tend = t + nt, .t = t,
	};
	
	const bool passed = value(&state, 0);
	if (!passed && state.src[-1] == '\0') state.src--;
	return passed && state.t != state.tend
		&& *state.src == '\0' ? (ejresult_t){
		.err = false,
		.loc = NULL,
		.ntoks = 0,
	} : (ejresult_t){
		.err = true,
		.loc = state.src,
		.ntoks = state.t - state.tbase,
	};
}

