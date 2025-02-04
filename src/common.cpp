#if defined(GB_SYSTEM_UNIX)
// Required for intrinsics on GCC
#include <xmmintrin.h>
#endif

#if defined(GB_COMPILER_MSVC)
#include <intrin.h>
#endif

#if defined(GB_SYSTEM_WINDOWS)

#define NOMINMAX            1
#include <windows.h>
#undef NOMINMAX
#endif

#define GB_WINDOWS_H_INCLUDED
#define GB_IMPLEMENTATION
#include "gb/gb.h"

#include <wchar.h>
#include <stdio.h>

#if defined(GB_COMPILER_MSVC)
#include <psapi.h>
#endif

#include <math.h>
#include <string.h>
#include <atomic> // Because I wanted the C++11 memory order semantics, of which gb.h does not offer (because it was a C89 library)


#if defined(GB_SYSTEM_WINDOWS)
	struct BlockingMutex {
		SRWLOCK srwlock;
	};
	void mutex_init(BlockingMutex *m) {
	}
	void mutex_destroy(BlockingMutex *m) {
	}
	void mutex_lock(BlockingMutex *m) {
		AcquireSRWLockExclusive(&m->srwlock);
	}
	bool mutex_try_lock(BlockingMutex *m) {
		return !!TryAcquireSRWLockExclusive(&m->srwlock);
	}
	void mutex_unlock(BlockingMutex *m) {
		ReleaseSRWLockExclusive(&m->srwlock);
	}
#else
	typedef gbMutex BlockingMutex;
	void mutex_init(BlockingMutex *m) {
		gb_mutex_init(m);
	}
	void mutex_destroy(BlockingMutex *m) {
		gb_mutex_destroy(m);
	}
	void mutex_lock(BlockingMutex *m) {
		gb_mutex_lock(m);
	}
	bool mutex_try_lock(BlockingMutex *m) {
		return !!gb_mutex_try_lock(m);
	}
	void mutex_unlock(BlockingMutex *m) {
		gb_mutex_unlock(m);
	}
#endif

struct RecursiveMutex {
	gbMutex mutex;
};
void mutex_init(RecursiveMutex *m) {
	gb_mutex_init(&m->mutex);
}
void mutex_destroy(RecursiveMutex *m) {
	gb_mutex_destroy(&m->mutex);
}
void mutex_lock(RecursiveMutex *m) {
	gb_mutex_lock(&m->mutex);
}
bool mutex_try_lock(RecursiveMutex *m) {
	return !!gb_mutex_try_lock(&m->mutex);
}
void mutex_unlock(RecursiveMutex *m) {
	gb_mutex_unlock(&m->mutex);
}



gb_inline void zero_size(void *ptr, isize len) {
	memset(ptr, 0, len);
}

#define zero_item(ptr) zero_size((ptr), gb_size_of(ptr))


i32 next_pow2(i32 n);
i64 next_pow2(i64 n);
isize next_pow2_isize(isize n);
void debugf(char const *fmt, ...);

template <typename U, typename V>
gb_inline U bit_cast(V &v) { return reinterpret_cast<U &>(v); }

template <typename U, typename V>
gb_inline U const &bit_cast(V const &v) { return reinterpret_cast<U const &>(v); }


gb_inline i64 align_formula(i64 size, i64 align) {
	if (align > 0) {
		i64 result = size + align-1;
		return result - result%align;
	}
	return size;
}
gb_inline isize align_formula_isize(isize size, isize align) {
	if (align > 0) {
		isize result = size + align-1;
		return result - result%align;
	}
	return size;
}
gb_inline void *align_formula_ptr(void *ptr, isize align) {
	if (align > 0) {
		uintptr result = (cast(uintptr)ptr) + align-1;
		return (void *)(result - result%align);
	}
	return ptr;
}


GB_ALLOCATOR_PROC(heap_allocator_proc);

gbAllocator heap_allocator(void) {
	gbAllocator a;
	a.proc = heap_allocator_proc;
	a.data = nullptr;
	return a;
}


GB_ALLOCATOR_PROC(heap_allocator_proc) {
	void *ptr = nullptr;
	gb_unused(allocator_data);
	gb_unused(old_size);



// TODO(bill): Throughly test!
	switch (type) {
#if defined(GB_COMPILER_MSVC)
	#if 0
	case gbAllocation_Alloc:
		ptr = _aligned_malloc(size, alignment);
		if (flags & gbAllocatorFlag_ClearToZero) {
			zero_size(ptr, size);
		}
		break;
	case gbAllocation_Free:
		_aligned_free(old_memory);
		break;
	case gbAllocation_Resize:
		ptr = _aligned_realloc(old_memory, size, alignment);
		break;
	#else
	case gbAllocation_Alloc: {
		isize aligned_size = align_formula_isize(size, alignment);
		// TODO(bill): Make sure this is aligned correctly
		ptr = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, aligned_size);
	} break;
	case gbAllocation_Free:
		HeapFree(GetProcessHeap(), 0, old_memory);
		break;
	case gbAllocation_Resize: {
		isize aligned_size = align_formula_isize(size, alignment);
		ptr = HeapReAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, old_memory, aligned_size);
	} break;
	#endif

#elif defined(GB_SYSTEM_LINUX)
	// TODO(bill): *nix version that's decent
	case gbAllocation_Alloc: {
		ptr = aligned_alloc(alignment, (size + alignment - 1) & ~(alignment - 1));
		// ptr = malloc(size+alignment);

		if (flags & gbAllocatorFlag_ClearToZero) {
			zero_size(ptr, size);
		}
		break;
	}

	case gbAllocation_Free: {
		free(old_memory);
		break;
	}

	case gbAllocation_Resize: {
		// ptr = realloc(old_memory, size);
		ptr = gb_default_resize_align(heap_allocator(), old_memory, old_size, size, alignment);
		break;
	}
#else
	// TODO(bill): *nix version that's decent
	case gbAllocation_Alloc: {
		posix_memalign(&ptr, alignment, size);

		if (flags & gbAllocatorFlag_ClearToZero) {
			zero_size(ptr, size);
		}
		break;
	}

	case gbAllocation_Free: {
		free(old_memory);
		break;
	}

	case gbAllocation_Resize: {
		ptr = gb_default_resize_align(heap_allocator(), old_memory, old_size, size, alignment);
		break;
	}
#endif

	case gbAllocation_FreeAll:
		break;
	}

	return ptr;
}

#include "unicode.cpp"
#include "array.cpp"
#include "string.cpp"
#include "queue.cpp"

#define for_array(index_, array_) for (isize index_ = 0; index_ < (array_).count; index_++)

#include "range_cache.cpp"

u32 fnv32a(void const *data, isize len) {
	u8 const *bytes = cast(u8 const *)data;
	u32 h = 0x811c9dc5;
	for (isize i = 0; i < len; i++) {
		u32 b = cast(u32)bytes[i];
		h = (h ^ b) * 0x01000193;
	}
	return h;
}

u64 fnv64a(void const *data, isize len) {
	u8 const *bytes = cast(u8 const *)data;
	u64 h = 0xcbf29ce484222325ull;
	for (isize i = 0; i < len; i++) {
		u64 b = cast(u64)bytes[i];
		h = (h ^ b) * 0x100000001b3ull;
	}
	return h;
}

u64 u64_digit_value(Rune r) {
	if ('0' <= r && r <= '9') {
		return r - '0';
	} else if ('a' <= r && r <= 'f') {
		return r - 'a' + 10;
	} else if ('A' <= r && r <= 'F') {
		return r - 'A' + 10;
	}
	return 16; // NOTE(bill): Larger than highest possible
}


u64 u64_from_string(String string) {
	u64 base = 10;
	bool has_prefix = false;
	if (string.len > 2 && string[0] == '0') {
		switch (string[1]) {
		case 'b': base = 2;  has_prefix = true; break;
		case 'o': base = 8;  has_prefix = true; break;
		case 'd': base = 10; has_prefix = true; break;
		case 'z': base = 12; has_prefix = true; break;
		case 'x': base = 16; has_prefix = true; break;
		case 'h': base = 16; has_prefix = true; break;
		}
	}

	u8 *text = string.text;
	isize len = string.len;
	if (has_prefix) {
		text += 2;
		len -= 2;
	}

	u64 result = 0ull;
	for (isize i = 0; i < len; i++) {
		Rune r = cast(Rune)text[i];
		if (r == '_') {
			continue;
		}
		u64 v = u64_digit_value(r);
		if (v >= base) {
			break;
		}
		result *= base;
		result += v;
	}
	return result;
}

gb_global char const global_num_to_char_table[] =
	"0123456789"
	"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
	"abcdefghijklmnopqrstuvwxyz"
	"@$";

String u64_to_string(u64 v, char *out_buf, isize out_buf_len) {
	char buf[32] = {0};
	isize i = gb_size_of(buf);

	u64 b = 10;
	while (v >= b) {
		buf[--i] = global_num_to_char_table[v%b];
		v /= b;
	}
	buf[--i] = global_num_to_char_table[v%b];

	isize len = gb_min(gb_size_of(buf)-i, out_buf_len);
	gb_memmove(out_buf, &buf[i], len);
	return make_string(cast(u8 *)out_buf, len);
}
String i64_to_string(i64 a, char *out_buf, isize out_buf_len) {
	char buf[32] = {0};
	isize i = gb_size_of(buf);
	bool negative = false;
	if (a < 0) {
		negative = true;
		a = -a;
	}

	u64 v = cast(u64)a;
	u64 b = 10;
	while (v >= b) {
		buf[--i] = global_num_to_char_table[v%b];
		v /= b;
	}
	buf[--i] = global_num_to_char_table[v%b];

	if (negative) {
		buf[--i] = '-';
	}

	isize len = gb_min(gb_size_of(buf)-i, out_buf_len);
	gb_memmove(out_buf, &buf[i], len);
	return make_string(cast(u8 *)out_buf, len);
}


gb_global i64 const signed_integer_mins[] = {
	0,
	-128ll,
	-32768ll,
	0,
	-2147483648ll,
	0,
	0,
	0,
	(-9223372036854775807ll - 1ll),
};
gb_global i64 const signed_integer_maxs[] = {
	0,
	127ll,
	32767ll,
	0,
	2147483647ll,
	0,
	0,
	0,
	9223372036854775807ll,
};
gb_global u64 const unsigned_integer_maxs[] = {
	0,
	255ull,
	65535ull,
	0,
	4294967295ull,
	0,
	0,
	0,
	18446744073709551615ull,
};


bool add_overflow_u64(u64 x, u64 y, u64 *result) {
	*result = x + y;
	return *result < x || *result < y;
}

bool sub_overflow_u64(u64 x, u64 y, u64 *result) {
	*result = x - y;
	return *result > x;
}

void mul_overflow_u64(u64 x, u64 y, u64 *lo, u64 *hi) {
#if defined(GB_COMPILER_MSVC) && defined(GB_ARCH_64_BIT)
	*lo = _umul128(x, y, hi);
#else
	// URL(bill): https://stackoverflow.com/questions/25095741/how-can-i-multiply-64-bit-operands-and-get-128-bit-result-portably#25096197
	u64 u1, v1, w1, t, w3, k;

	u1 = (x & 0xffffffff);
	v1 = (y & 0xffffffff);
	t = (u1 * v1);
	w3 = (t & 0xffffffff);
	k = (t >> 32);

	x >>= 32;
	t = (x * v1) + k;
	k = (t & 0xffffffff);
	w1 = (t >> 32);

	y >>= 32;
	t = (u1 * y) + k;
	k = (t >> 32);

	*hi = (x * y) + w1 + k;
	*lo = (t << 32) + w3;
#endif
}



gb_global String global_module_path = {0};
gb_global bool global_module_path_set = false;

// Arena from Per Vognsen
#define ALIGN_DOWN(n, a)     ((n) & ~((a) - 1))
#define ALIGN_UP(n, a)       ALIGN_DOWN((n) + (a) - 1, (a))
#define ALIGN_DOWN_PTR(p, a) (cast(void *)ALIGN_DOWN(cast(uintptr)(p), (a)))
#define ALIGN_UP_PTR(p, a)   (cast(void *)ALIGN_UP(cast(uintptr)(p), (a)))

typedef struct Arena {
	u8 *        ptr;
	u8 *        end;
	u8 *        prev;
	Array<u8 *> blocks;
	gbAllocator backing;
	isize       block_size;
	gbMutex     mutex;
	isize total_used;
	bool   use_mutex;
} Arena;

#define ARENA_MIN_ALIGNMENT 16
#define ARENA_DEFAULT_BLOCK_SIZE (8*1024*1024)


gb_global Arena permanent_arena = {};

void arena_init(Arena *arena, gbAllocator backing, isize block_size=ARENA_DEFAULT_BLOCK_SIZE) {
	arena->backing = backing;
	arena->block_size = block_size;
	arena->use_mutex = true;
	array_init(&arena->blocks, backing, 0, 2);
	gb_mutex_init(&arena->mutex);
}

void arena_grow(Arena *arena, isize min_size) {
	if (arena->use_mutex) {
		gb_mutex_lock(&arena->mutex);
	}

	isize size = gb_max(arena->block_size, min_size);
	size = ALIGN_UP(size, ARENA_MIN_ALIGNMENT);
	void *new_ptr = gb_alloc(arena->backing, size);
	arena->ptr = cast(u8 *)new_ptr;
	// zero_size(arena->ptr, size); // NOTE(bill): This should already be zeroed
	GB_ASSERT(arena->ptr == ALIGN_DOWN_PTR(arena->ptr, ARENA_MIN_ALIGNMENT));
	arena->end = arena->ptr + size;
	array_add(&arena->blocks, arena->ptr);

	if (arena->use_mutex) {
		gb_mutex_unlock(&arena->mutex);
	}
}

void *arena_alloc(Arena *arena, isize size, isize alignment) {
	if (arena->use_mutex) {
		gb_mutex_lock(&arena->mutex);
	}

	arena->total_used += size;

	if (size > (arena->end - arena->ptr)) {
		arena_grow(arena, size);
		GB_ASSERT(size <= (arena->end - arena->ptr));
	}

	isize align = gb_max(alignment, ARENA_MIN_ALIGNMENT);
	void *ptr = arena->ptr;
	arena->prev = arena->ptr;
	arena->ptr = cast(u8 *)ALIGN_UP_PTR(arena->ptr + size, align);
	GB_ASSERT(arena->ptr <= arena->end);
	GB_ASSERT(ptr == ALIGN_DOWN_PTR(ptr, align));
	// zero_size(ptr, size);

	if (arena->use_mutex) {
		gb_mutex_unlock(&arena->mutex);
	}
	return ptr;
}

void arena_free_all(Arena *arena) {
	if (arena->use_mutex) {
		gb_mutex_lock(&arena->mutex);
	}

	for_array(i, arena->blocks) {
		gb_free(arena->backing, arena->blocks[i]);
	}
	array_clear(&arena->blocks);
	arena->ptr = nullptr;
	arena->end = nullptr;

	if (arena->use_mutex) {
		gb_mutex_unlock(&arena->mutex);
	}
}



GB_ALLOCATOR_PROC(arena_allocator_proc);

gbAllocator arena_allocator(Arena *arena) {
	gbAllocator a;
	a.proc = arena_allocator_proc;
	a.data = arena;
	return a;
}


GB_ALLOCATOR_PROC(arena_allocator_proc) {
	void *ptr = nullptr;
	Arena *arena = cast(Arena *)allocator_data;
	GB_ASSERT_NOT_NULL(arena);

	switch (type) {
	case gbAllocation_Alloc:
		ptr = arena_alloc(arena, size, alignment);
		break;
	case gbAllocation_Free:
		// GB_PANIC("gbAllocation_Free not supported");
		break;
	case gbAllocation_Resize:
		if (size == 0) {
			ptr = nullptr;
		} else if (size <= old_size) {
			ptr = old_memory;
		} else {
			ptr = arena_alloc(arena, size, alignment);
			gb_memmove(ptr, old_memory, old_size);
		}
		break;
	case gbAllocation_FreeAll:
		arena_free_all(arena);
		break;
	}

	return ptr;
}


gbAllocator permanent_allocator() {
	return arena_allocator(&permanent_arena);
	// return heap_allocator();
}



struct Temp_Allocator {
	u8 *data;
	isize len;
	isize curr_offset;
	gbAllocator backup_allocator;
	Array<void *> leaked_allocations;
	gbMutex mutex;
};

gb_global Temp_Allocator temporary_allocator_data = {};

void temp_allocator_init(Temp_Allocator *s, isize size) {
	s->backup_allocator = heap_allocator();
	s->data = cast(u8 *)gb_alloc_align(s->backup_allocator, size, 16);
	s->len = size;
	s->curr_offset = 0;
	s->leaked_allocations.allocator = s->backup_allocator;
	gb_mutex_init(&s->mutex);
}

void *temp_allocator_alloc(Temp_Allocator *s, isize size, isize alignment) {
	size = align_formula_isize(size, alignment);
	if (s->curr_offset+size <= s->len) {
		u8 *start = s->data;
		u8 *ptr = start + s->curr_offset;
		ptr = cast(u8 *)align_formula_ptr(ptr, alignment);
		// assume memory is zero

		isize offset = ptr - start;
		s->curr_offset = offset + size;
		return ptr;
	} else if (size <= s->len) {
		u8 *start = s->data;
		u8 *ptr = cast(u8 *)align_formula_ptr(start, alignment);
		// assume memory is zero

		isize offset = ptr - start;
		s->curr_offset = offset + size;
		return ptr;
	}

	void *ptr = gb_alloc_align(s->backup_allocator, size, alignment);
	array_add(&s->leaked_allocations, ptr);
	return ptr;
}

void temp_allocator_free_all(Temp_Allocator *s) {
	s->curr_offset = 0;
	for_array(i, s->leaked_allocations) {
		gb_free(s->backup_allocator, s->leaked_allocations[i]);
	}
	array_clear(&s->leaked_allocations);
	gb_zero_size(s->data, s->len);
}

GB_ALLOCATOR_PROC(temp_allocator_proc) {
	void *ptr = nullptr;
	Temp_Allocator *s = cast(Temp_Allocator *)allocator_data;
	GB_ASSERT_NOT_NULL(s);

	gb_mutex_lock(&s->mutex);
	defer (gb_mutex_unlock(&s->mutex));

	switch (type) {
	case gbAllocation_Alloc:
		return temp_allocator_alloc(s, size, alignment);
	case gbAllocation_Free:
		break;
	case gbAllocation_Resize:
		if (size == 0) {
			ptr = nullptr;
		} else if (size <= old_size) {
			ptr = old_memory;
		} else {
			ptr = temp_allocator_alloc(s, size, alignment);
			gb_memmove(ptr, old_memory, old_size);
		}
		break;
	case gbAllocation_FreeAll:
		temp_allocator_free_all(s);
		break;
	}

	return ptr;
}


gbAllocator temporary_allocator() {
	return permanent_allocator();
	// return {temp_allocator_proc, &temporary_allocator_data};
}




#include "string_map.cpp"
#include "map.cpp"
#include "ptr_set.cpp"
#include "string_set.cpp"
#include "priority_queue.cpp"
#include "thread_pool.cpp"


struct StringIntern {
	StringIntern *next;
	isize len;
	char str[1];
};

Map<StringIntern *> string_intern_map = {}; // Key: u64
Arena string_intern_arena = {};

char const *string_intern(char const *text, isize len) {
	u64 hash = gb_fnv64a(text, len);
	u64 key = hash ? hash : 1;
	StringIntern **found = map_get(&string_intern_map, hash_integer(key));
	if (found) {
		for (StringIntern *it = *found; it != nullptr; it = it->next) {
			if (it->len == len && gb_strncmp(it->str, (char *)text, len) == 0) {
				return it->str;
			}
		}
	}

	StringIntern *new_intern = cast(StringIntern *)arena_alloc(&string_intern_arena, gb_offset_of(StringIntern, str) + len + 1, gb_align_of(StringIntern));
	new_intern->len = len;
	new_intern->next = found ? *found : nullptr;
	gb_memmove(new_intern->str, text, len);
	new_intern->str[len] = 0;
	map_set(&string_intern_map, hash_integer(key), new_intern);
	return new_intern->str;
}

char const *string_intern(String const &string) {
	return string_intern(cast(char const *)string.text, string.len);
}

void init_string_interner(void) {
	map_init(&string_intern_map, heap_allocator());
	arena_init(&string_intern_arena, heap_allocator());
}




i32 next_pow2(i32 n) {
	if (n <= 0) {
		return 0;
	}
	n--;
	n |= n >> 1;
	n |= n >> 2;
	n |= n >> 4;
	n |= n >> 8;
	n |= n >> 16;
	n++;
	return n;
}
i64 next_pow2(i64 n) {
	if (n <= 0) {
		return 0;
	}
	n--;
	n |= n >> 1;
	n |= n >> 2;
	n |= n >> 4;
	n |= n >> 8;
	n |= n >> 16;
	n |= n >> 32;
	n++;
	return n;
}
isize next_pow2_isize(isize n) {
	if (n <= 0) {
		return 0;
	}
	n--;
	n |= n >> 1;
	n |= n >> 2;
	n |= n >> 4;
	n |= n >> 8;
	n |= n >> 16;
	#if defined(GB_ARCH_64_BIT)
		n |= n >> 32;
	#endif
	n++;
	return n;
}
u32 next_pow2_u32(u32 n) {
	if (n == 0) {
		return 0;
	}
	n--;
	n |= n >> 1;
	n |= n >> 2;
	n |= n >> 4;
	n |= n >> 8;
	n |= n >> 16;
	n++;
	return n;
}


i32 bit_set_count(u32 x) {
	x -= ((x >> 1) & 0x55555555);
	x = (((x >> 2) & 0x33333333) + (x & 0x33333333));
	x = (((x >> 4) + x) & 0x0f0f0f0f);
	x += (x >> 8);
	x += (x >> 16);

	return cast(i32)(x & 0x0000003f);
}

i64 bit_set_count(u64 x) {
	u32 a = *(cast(u32 *)&x);
	u32 b = *(cast(u32 *)&x + 1);
	return bit_set_count(a) + bit_set_count(b);
}

u32 floor_log2(u32 x) {
	x |= x >> 1;
	x |= x >> 2;
	x |= x >> 4;
	x |= x >> 8;
	x |= x >> 16;
	return cast(u32)(bit_set_count(x) - 1);
}

u64 floor_log2(u64 x) {
	x |= x >> 1;
	x |= x >> 2;
	x |= x >> 4;
	x |= x >> 8;
	x |= x >> 16;
	x |= x >> 32;
	return cast(u64)(bit_set_count(x) - 1);
}


u32 ceil_log2(u32 x) {
	i32 y = cast(i32)(x & (x-1));
	y |= -y;
	y >>= 32-1;
	x |= x >> 1;
	x |= x >> 2;
	x |= x >> 4;
	x |= x >> 8;
	x |= x >> 16;
	return cast(u32)(bit_set_count(x) - 1 - y);
}

u64 ceil_log2(u64 x) {
	i64 y = cast(i64)(x & (x-1));
	y |= -y;
	y >>= 64-1;
	x |= x >> 1;
	x |= x >> 2;
	x |= x >> 4;
	x |= x >> 8;
	x |= x >> 16;
	x |= x >> 32;
	return cast(u64)(bit_set_count(x) - 1 - y);
}


i32 prev_pow2(i32 n) {
	if (n <= 0) {
		return 0;
	}
	n |= n >> 1;
	n |= n >> 2;
	n |= n >> 4;
	n |= n >> 8;
	n |= n >> 16;
	return n - (n >> 1);
}
i64 prev_pow2(i64 n) {
	if (n <= 0) {
		return 0;
	}
	n |= n >> 1;
	n |= n >> 2;
	n |= n >> 4;
	n |= n >> 8;
	n |= n >> 16;
	n |= n >> 32;
	return n - (n >> 1);
}

u16 f32_to_f16(f32 value) {
	union { u32 i; f32 f; } v;
	i32 i, s, e, m;

	v.f = value;
	i = (i32)v.i;

	s =  (i >> 16) & 0x00008000;
	e = ((i >> 23) & 0x000000ff) - (127 - 15);
	m =   i        & 0x007fffff;


	if (e <= 0) {
		if (e < -10) return cast(u16)s;
		m = (m | 0x00800000) >> (1 - e);

		if (m & 0x00001000)
			m += 0x00002000;

		return cast(u16)(s | (m >> 13));
	} else if (e == 0xff - (127 - 15)) {
		if (m == 0) {
			return cast(u16)(s | 0x7c00); /* NOTE(bill): infinity */
		} else {
			/* NOTE(bill): NAN */
			m >>= 13;
			return cast(u16)(s | 0x7c00 | m | (m == 0));
		}
	} else {
		if (m & 0x00001000) {
			m += 0x00002000;
			if (m & 0x00800000) {
				m = 0;
				e += 1;
			}
		}

		if (e > 30) {
			float volatile f = 1e12f;
			int j;
			for (j = 0; j < 10; j++) {
				f *= f; /* NOTE(bill): Cause overflow */
			}

			return cast(u16)(s | 0x7c00);
		}

		return cast(u16)(s | (e << 10) | (m >> 13));
	}
}

f32 f16_to_f32(u16 value) {
	typedef union { u32 u; f32 f; } fp32;
	fp32 v;

	fp32 magic = {(254u - 15u) << 23};
	fp32 inf_or_nan = {(127u + 16u) << 23};

	v.u = (value & 0x7fffu) << 13;
	v.f *= magic.f;
	if (v.f >= inf_or_nan.f) {
		v.u |= 255u << 23;
	}
	v.u |= (value & 0x8000u) << 16;
	return v.f;
}

f64 gb_sqrt(f64 x) {
	return sqrt(x);
}





// Doubly Linked Lists

#define DLIST_SET(curr_element, next_element)  do { \
	(curr_element)->next = (next_element);             \
	(curr_element)->next->prev = (curr_element);       \
	(curr_element) = (curr_element)->next;             \
} while (0)

#define DLIST_APPEND(root_element, curr_element, next_element) do { \
	if ((root_element) == nullptr) { \
		(root_element) = (curr_element) = (next_element); \
	} else { \
		DLIST_SET(curr_element, next_element); \
	} \
} while (0)



#if defined(GB_SYSTEM_WINDOWS)

wchar_t **command_line_to_wargv(wchar_t *cmd_line, int *_argc) {
	u32 i, j;

	u32 len = cast(u32)string16_len(cmd_line);
	i = ((len+2)/2)*gb_size_of(void *) + gb_size_of(void *);

	wchar_t **argv = cast(wchar_t **)GlobalAlloc(GMEM_FIXED, i + (len+2)*gb_size_of(wchar_t));
	wchar_t *_argv = cast(wchar_t *)((cast(u8 *)argv)+i);

	u32 argc = 0;
	argv[argc] = _argv;
	bool in_quote = false;
	bool in_text = false;
	bool in_space = true;
	i = 0;
	j = 0;

	for (;;) {
		wchar_t a = cmd_line[i];
		if (a == 0) {
			break;
		}
		if (in_quote) {
			if (a == '\"') {
				in_quote = false;
			} else {
				_argv[j++] = a;
			}
		} else {
			switch (a) {
			case '\"':
				in_quote = true;
				in_text = true;
				if (in_space) argv[argc++] = _argv+j;
				in_space = false;
				break;
			case ' ':
			case '\t':
			case '\n':
			case '\r':
				if (in_text) _argv[j++] = '\0';
				in_text = false;
				in_space = true;
				break;
			default:
				in_text = true;
				if (in_space) argv[argc++] = _argv+j;
				_argv[j++] = a;
				in_space = false;
				break;
			}
		}
		i++;
	}
	_argv[j] = '\0';
	argv[argc] = nullptr;

	if (_argc) *_argc = argc;
	return argv;
}

#endif


#if defined(GB_SYSTEM_WINDOWS)
	bool path_is_directory(String path) {
		gbAllocator a = heap_allocator();
		String16 wstr = string_to_string16(a, path);
		defer (gb_free(a, wstr.text));

		i32 attribs = GetFileAttributesW(wstr.text);
		if (attribs < 0) return false;

		return (attribs & FILE_ATTRIBUTE_DIRECTORY) != 0;
	}

#else
	bool path_is_directory(String path) {
		gbAllocator a = heap_allocator();
		char *copy = cast(char *)copy_string(a, path).text;
		defer (gb_free(a, copy));

		struct stat s;
		if (stat(copy, &s) == 0) {
			return (s.st_mode & S_IFDIR) != 0;
		}
		return false;
	}
#endif


String path_to_full_path(gbAllocator a, String path) {
	gbAllocator ha = heap_allocator();
	char *path_c = gb_alloc_str_len(ha, cast(char *)path.text, path.len);
	defer (gb_free(ha, path_c));

	char *fullpath = gb_path_get_full_name(a, path_c);
	String res = string_trim_whitespace(make_string_c(fullpath));
#if defined(GB_SYSTEM_WINDOWS)
	for (isize i = 0; i < res.len; i++) {
		if (res.text[i] == '\\') {
			res.text[i] = '/';
		}
	}
#endif
	return res;
}



struct FileInfo {
	String name;
	String fullpath;
	i64    size;
	bool   is_dir;
};

enum ReadDirectoryError {
	ReadDirectory_None,

	ReadDirectory_InvalidPath,
	ReadDirectory_NotExists,
	ReadDirectory_Permission,
	ReadDirectory_NotDir,
	ReadDirectory_Empty,
	ReadDirectory_Unknown,

	ReadDirectory_COUNT,
};

i64 get_file_size(String path) {
	char *c_str = alloc_cstring(heap_allocator(), path);
	defer (gb_free(heap_allocator(), c_str));

	gbFile f = {};
	gbFileError err = gb_file_open(&f, c_str);
	defer (gb_file_close(&f));
	if (err != gbFileError_None) {
		return -1;
	}
	return gb_file_size(&f);
}


#if defined(GB_SYSTEM_WINDOWS)
ReadDirectoryError read_directory(String path, Array<FileInfo> *fi) {
	GB_ASSERT(fi != nullptr);

	gbAllocator a = heap_allocator();

	while (path.len > 0) {
		Rune end = path[path.len-1];
		if (end == '/') {
			path.len -= 1;
		} else if (end == '\\') {
			path.len -= 1;
		} else {
			break;
		}
	}

	if (path.len == 0) {
		return ReadDirectory_InvalidPath;
	}
	{
		char *c_str = alloc_cstring(a, path);
		defer (gb_free(a, c_str));

		gbFile f = {};
		gbFileError file_err = gb_file_open(&f, c_str);
		defer (gb_file_close(&f));

		switch (file_err) {
		case gbFileError_Invalid:    return ReadDirectory_InvalidPath;
		case gbFileError_NotExists:  return ReadDirectory_NotExists;
		// case gbFileError_Permission: return ReadDirectory_Permission;
		}
	}

	if (!path_is_directory(path)) {
		return ReadDirectory_NotDir;
	}


	char *new_path = gb_alloc_array(a, char, path.len+3);
	defer (gb_free(a, new_path));

	gb_memmove(new_path, path.text, path.len);
	gb_memmove(new_path+path.len, "/*", 2);
	new_path[path.len+2] = 0;

	String np = make_string(cast(u8 *)new_path, path.len+2);
	String16 wstr = string_to_string16(a, np);
	defer (gb_free(a, wstr.text));

	WIN32_FIND_DATAW file_data = {};
	HANDLE find_file = FindFirstFileW(wstr.text, &file_data);
	if (find_file == INVALID_HANDLE_VALUE) {
		return ReadDirectory_Unknown;
	}
	defer (FindClose(find_file));

	array_init(fi, a, 0, 100);

	do {
		wchar_t *filename_w = file_data.cFileName;
		i64 size = cast(i64)file_data.nFileSizeLow;
		size |= (cast(i64)file_data.nFileSizeHigh) << 32;
		String name = string16_to_string(a, make_string16_c(filename_w));
		if (name == "." || name == "..") {
			gb_free(a, name.text);
			continue;
		}

		String filepath = {};
		filepath.len = path.len+1+name.len;
		filepath.text = gb_alloc_array(a, u8, filepath.len+1);
		defer (gb_free(a, filepath.text));
		gb_memmove(filepath.text, path.text, path.len);
		gb_memmove(filepath.text+path.len, "/", 1);
		gb_memmove(filepath.text+path.len+1, name.text, name.len);

		FileInfo info = {};
		info.name = name;
		info.fullpath = path_to_full_path(a, filepath);
		info.size = size;
		info.is_dir = (file_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
		array_add(fi, info);
	} while (FindNextFileW(find_file, &file_data));

	if (fi->count == 0) {
		return ReadDirectory_Empty;
	}

	return ReadDirectory_None;
}
#elif defined(GB_SYSTEM_LINUX) || defined(GB_SYSTEM_OSX) || defined(GB_SYSTEM_FREEBSD)

#include <dirent.h>

ReadDirectoryError read_directory(String path, Array<FileInfo> *fi) {
	GB_ASSERT(fi != nullptr);

	gbAllocator a = heap_allocator();

	char *c_path = alloc_cstring(a, path);
	defer (gb_free(a, c_path));

	DIR *dir = opendir(c_path);
	if (!dir) {
		switch (errno) {
		case ENOENT:
			return ReadDirectory_NotExists;
		case EACCES:
			return ReadDirectory_Permission;
		case ENOTDIR:
			return ReadDirectory_NotDir;
		default:
			// ENOMEM: out of memory
			// EMFILE: per-process limit on open fds reached
			// ENFILE: system-wide limit on total open files reached
			return ReadDirectory_Unknown;
		}
		GB_PANIC("unreachable");
	}

	array_init(fi, a, 0, 100);

	for (;;) {
		struct dirent *entry = readdir(dir);
		if (entry == nullptr) {
			break;
		}

		String name = make_string_c(entry->d_name);
		if (name == "." || name == "..") {
			continue;
		}

		String filepath = {};
		filepath.len = path.len+1+name.len;
		filepath.text = gb_alloc_array(a, u8, filepath.len+1);
		defer (gb_free(a, filepath.text));
		gb_memmove(filepath.text, path.text, path.len);
		gb_memmove(filepath.text+path.len, "/", 1);
		gb_memmove(filepath.text+path.len+1, name.text, name.len);
		filepath.text[filepath.len] = 0;


		struct stat dir_stat = {};

		if (stat((char *)filepath.text, &dir_stat)) {
			continue;
		}

		if (S_ISDIR(dir_stat.st_mode)) {
			continue;
		}

		i64 size = dir_stat.st_size;

		FileInfo info = {};
		info.name = name;
		info.fullpath = path_to_full_path(a, filepath);
		info.size = size;
		array_add(fi, info);
	}

	if (fi->count == 0) {
		return ReadDirectory_Empty;
	}

	return ReadDirectory_None;
}
#else
#error Implement read_directory
#endif



#define USE_DAMERAU_LEVENSHTEIN 1

isize levenstein_distance_case_insensitive(String const &a, String const &b) {
	isize w = a.len+1;
	isize h = b.len+1;
	isize *matrix = gb_alloc_array(temporary_allocator(), isize, w*h);
	for (isize i = 0; i <= a.len; i++) {
		matrix[i*w + 0] = i;
	}
	for (isize i = 0; i <= b.len; i++) {
		matrix[0*w + i] = i;
	}

	for (isize i = 1; i <= a.len; i++) {
		char a_c = gb_char_to_lower(cast(char)a.text[i-1]);
		for (isize j = 1; j <= b.len; j++) {
			char b_c = gb_char_to_lower(cast(char)b.text[j-1]);
			if (a_c == b_c) {
				matrix[i*w + j] = matrix[(i-1)*w + j-1];
			} else {
				isize remove = matrix[(i-1)*w + j] + 1;
				isize insert = matrix[i*w + j-1] + 1;
				isize substitute = matrix[(i-1)*w + j-1] + 1;
				isize minimum = remove;
				if (insert < minimum) {
					minimum = insert;
				}
				if (substitute < minimum) {
					minimum = substitute;
				}
				// Damerau-Levenshtein (transposition extension)
				#if USE_DAMERAU_LEVENSHTEIN
				if (i > 1 && j > 1) {
					isize transpose = matrix[(i-2)*w + j-2] + 1;
					if (transpose < minimum) {
						minimum = transpose;
					}
				}
				#endif

				matrix[i*w + j] = minimum;
			}
		}
	}

	return matrix[a.len*w + b.len];
}


struct DistanceAndTarget {
	isize distance;
	String target;
};

struct DidYouMeanAnswers {
	Array<DistanceAndTarget> distances;
	String key;
};

enum {MAX_SMALLEST_DID_YOU_MEAN_DISTANCE = 3-USE_DAMERAU_LEVENSHTEIN};

DidYouMeanAnswers did_you_mean_make(gbAllocator allocator, isize cap, String const &key) {
	DidYouMeanAnswers d = {};
	array_init(&d.distances, allocator, 0, cap);
	d.key = key;
	return d;
}
void did_you_mean_destroy(DidYouMeanAnswers *d) {
	array_free(&d->distances);
}
void did_you_mean_append(DidYouMeanAnswers *d, String const &target) {
	if (target.len == 0 || target == "_") {
		return;
	}
	DistanceAndTarget dat = {};
	dat.target = target;
	dat.distance = levenstein_distance_case_insensitive(d->key, target);
	array_add(&d->distances, dat);
}
Slice<DistanceAndTarget> did_you_mean_results(DidYouMeanAnswers *d) {
	gb_sort_array(d->distances.data, d->distances.count, gb_isize_cmp(gb_offset_of(DistanceAndTarget, distance)));
	isize count = 0;
	for (isize i = 0; i < d->distances.count; i++) {
		isize distance = d->distances[i].distance;
		if (distance > MAX_SMALLEST_DID_YOU_MEAN_DISTANCE) {
			break;
		}
		count += 1;
	}
	return slice_array(d->distances, 0, count);
}
