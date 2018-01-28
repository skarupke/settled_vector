/*
This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or
distribute this software, either in source code form or as a compiled
binary, for any purpose, commercial or non-commercial, and by any
means.

In jurisdictions that recognize copyright laws, the author or authors
of this software dedicate any and all copyright interest in the
software to the public domain. We make this dedication for the benefit
of the public at large and to the detriment of our heirs and
successors. We intend this dedication to be an overt act of
relinquishment in perpetuity of all present and future rights to this
software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

For more information, please refer to <http://unlicense.org/>
 */
// despite that it would be nice if you give credit to Malte Skarupke
#include "settled_vector.h"
#include <sys/mman.h>
#include <unistd.h>
#include <cstddef>

namespace detail
{
static constexpr size_t default_num_bytes = 4L * 1024L * 1024L * 1024L;
#ifndef DONT_ACTUALLY_GROW
	static const size_t page_size = getpagesize();
#endif
MMappedMemory::~MMappedMemory()
{
	if (ptr) munmap(ptr, default_num_bytes);
}
void MMappedMemory::grow(size_t bytes_per_chunk, size_t num_chunks)
{
	size_t bytes = bytes_per_chunk * num_chunks;
	if (!bytes) return;
#define DONT_ACTUALLY_GROW
#ifdef DONT_ACTUALLY_GROW
	if (!ptr)
	{
		ptr = mmap(nullptr, default_num_bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		capacity_ptr = static_cast<char *>(ptr) + (default_num_bytes / bytes_per_chunk) * bytes_per_chunk;
	}
#else
	if (!ptr)
	{
		ptr = capacity_ptr = mmap(nullptr, default_num_bytes, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	}
	char * next_page_start = static_cast<char *>(capacity_ptr);
	next_page_start += (page_size - (reinterpret_cast<uintptr_t>(capacity_ptr) % page_size)) % page_size;
	bytes += (page_size - (bytes % page_size)) % page_size;
	mprotect(next_page_start, bytes, PROT_READ | PROT_WRITE);
	char * start_ptr = static_cast<char *>(ptr);
	capacity_ptr = start_ptr + ((next_page_start + bytes - start_ptr) / bytes_per_chunk) * bytes_per_chunk;
#endif
}
}

#ifdef ENABLE_GTEST
#include <gtest/gtest.h>
TEST(settled_vector, simple)
{
	SettledVector<int> a = { 1, 2, 3 };
	a.pop_back();
	ASSERT_EQ(SettledVector<int>({ 1, 2 }), a);
	a.push_back(5);
	ASSERT_EQ(SettledVector<int>({ 1, 2, 5 }), a);
}
TEST(settled_vector, insert_erase)
{
	SettledVector<int> a = { 1, 2, 3 };
	a.insert(a.begin(), 0);
	ASSERT_EQ(SettledVector<int>({ 0, 1, 2, 3 }), a);
	a.insert(a.end(), 5);
	ASSERT_EQ(SettledVector<int>({ 0, 1, 2, 3, 5 }), a);
	a.insert(a.end() - 1, 4);
	ASSERT_EQ(SettledVector<int>({ 0, 1, 2, 3, 4, 5 }), a);
	a.erase(a.begin());
	ASSERT_EQ(SettledVector<int>({ 1, 2, 3, 4, 5 }), a);
	a.erase(a.begin() + 1);
	ASSERT_EQ(SettledVector<int>({ 1, 3, 4, 5 }), a);
	a.erase(a.end() - 2);
	ASSERT_EQ(SettledVector<int>({ 1, 3, 5 }), a);
	a.erase(a.end() - 1);
	ASSERT_EQ(SettledVector<int>({ 1, 3 }), a);
}
#	ifndef DONT_ACTUALLY_GROW
	TEST(settled_vector, odd_size)
	{
		struct thirty_six_bytes
		{
			int a;
			int b;
			int c;
			int d;
			int e;
			int f;
			int g;
			int h;
			int i;
			thirty_six_bytes()
				: a(0), b(1), c(2), d(3), e(4), f(5), g(6), h(7), i(8)
			{
			}
		};
		static_assert(sizeof(thirty_six_bytes) == 36, "for testing 36 bytes would be a good number because it gives odd results for the capacity");
		SettledVector<thirty_six_bytes> a(detail::page_size / sizeof(thirty_six_bytes));
		ASSERT_EQ(detail::page_size / sizeof(thirty_six_bytes), a.capacity());
		a.push_back({});
		ASSERT_EQ((2 * detail::page_size) / sizeof(thirty_six_bytes), a.capacity());
	}
#	endif
#endif

