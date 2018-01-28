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
#pragma once

#include <memory>
#include <iterator>
#include <algorithm>
#include <cstddef>

namespace detail
{
struct MMappedMemory
{
	inline MMappedMemory() noexcept
		: ptr(nullptr), capacity_ptr(nullptr)
	{
	}
	inline MMappedMemory(MMappedMemory && other) noexcept
		: ptr(other.ptr), capacity_ptr(other.capacity_ptr)
	{
		other.ptr = other.capacity_ptr = nullptr;
	}
	inline MMappedMemory & operator=(MMappedMemory && other)
	{
		std::swap(ptr, other.ptr);
		std::swap(capacity_ptr, other.capacity_ptr);
		return *this;
	}
	~MMappedMemory();
	void grow(size_t bytes_per_chunk, size_t num_chunks);

	inline size_t capacity() const noexcept
	{
		return static_cast<char *>(capacity_ptr) - static_cast<char *>(ptr);
	}
	inline void * begin() const noexcept
	{
		return ptr;
	}
	inline void * end() const noexcept
	{
		return capacity_ptr;
	}

private:
	void * ptr;
	void * capacity_ptr;
};
}

template<typename T>
class SettledVector
{
public:
	typedef T value_type;
	typedef T & reference;
	typedef const T & const_reference;
	typedef T * pointer;
	typedef const T * const_pointer;
	typedef ptrdiff_t difference_type;
	typedef size_t size_type;
	typedef T * iterator;
	typedef const T * const_iterator;
	typedef std::reverse_iterator<iterator> reverse_iterator;
	typedef std::reverse_iterator<const_iterator> const_reverse_iterator;

	SettledVector() noexcept
		: memory(), end_ptr()
	{
	}
	SettledVector(size_type n)
		: memory(), end_ptr()
	{
		resize(n);
	}
	template<typename InputIterator>
	SettledVector(InputIterator first, InputIterator last)
		: memory(), end_ptr()
	{
		for (; first != last; ++first)
		{
			push_back(*first);
		}
	}
	SettledVector(const SettledVector & other)
		: memory(), end_ptr()
	{
		reserve(other.size());
		for (const value_type & value : other)
		{
			new (end_ptr++) value_type(value);
		}
	}
	SettledVector(SettledVector && other) noexcept
		: memory(std::move(other.memory)), end_ptr(std::move(other.end_ptr))
	{
		other.end_ptr = static_cast<pointer>(other.memory.begin());
	}
	SettledVector(std::initializer_list<value_type> list)
		 : memory(), end_ptr()
	{
		grow_if_necessary(list.size());
		for (const value_type & value : list)
		{
			new (end_ptr++) value_type(value);
		}
	}
	~SettledVector()
	{
		clear();
		while (!empty()) pop_back();
	}
	SettledVector & operator=(SettledVector other) noexcept
	{
		swap(other);
		return *this;
	}
	void swap(SettledVector & other) noexcept
	{
		std::swap(memory, other.memory);
		std::swap(end_ptr, other.end_ptr);
	}

	iterator begin() noexcept
	{
		return iterator(static_cast<pointer>(memory.begin()));
	}
	const_iterator begin() const noexcept
	{
		return const_iterator(static_cast<const_pointer>(memory.begin()));
	}
	const_iterator cbegin() const noexcept
	{
		return begin();
	}
	iterator end() noexcept
	{
		return iterator(end_ptr);
	}
	const_iterator end() const noexcept
	{
		return const_iterator(end_ptr);
	}
	const_iterator cend() const noexcept
	{
		return end();
	}
	reverse_iterator rbegin()
	{
		return reverse_iterator(end());
	}
	const_reverse_iterator rbegin() const
	{
		return const_reverse_iterator(end());
	}
	const_reverse_iterator crbegin() const
	{
		return rbegin();
	}
	reverse_iterator rend()
	{
		return reverse_iterator(begin());
	}
	const_reverse_iterator rend() const
	{
		return const_reverse_iterator(begin());
	}
	const_reverse_iterator crend() const
	{
		return rend();
	}

	size_type size() const noexcept
	{
		return end_ptr - static_cast<const_pointer>(memory.begin());
	}
	size_type capacity() const noexcept
	{
		return memory.capacity() / sizeof(value_type);
	}
	bool empty() const noexcept
	{
		return end_ptr == static_cast<const_pointer>(memory.begin());
	}

	template<typename... Args>
	void emplace(const_iterator position, Args &&... args)
	{
		if (position == cend()) emplace_back(std::forward<Args>(args)...);
		else
		{
			grow_if_necessary(1);
			iterator to_insert = begin() + (position - cbegin());
			new (end_ptr) value_type(std::move(*to_insert));
			to_insert->~value_type();
			new (&*to_insert) value_type(std::forward<Args>(args)...);
			auto end_it = end();
			for (++to_insert; to_insert != end_it; ++to_insert)
			{
				using std::swap;
				swap(*end_ptr, *to_insert);
			}
			++end_ptr;
		}
	}
	void insert(const_iterator position, value_type value)
	{
		emplace(position, std::move(value));
	}
	void push_back(value_type element)
	{
		emplace_back(std::move(element));
	}
	template<typename... Args>
	void emplace_back(Args &&... args)
	{
		grow_if_necessary(1);
		new (end_ptr) value_type(std::forward<Args>(args)...);
		++end_ptr;
	}
	void pop_back() noexcept
	{
		(--end_ptr)->~value_type();
	}
	iterator erase(const_iterator position)
	{
		size_type offset = position - cbegin();
		iterator end_it = end();
		iterator before = begin() + offset;
		for (iterator after = before + 1; after != end_it; ++after, ++before)
		{
			using std::swap;
			swap(*before, *after);
		}
		pop_back();
		return begin() + offset;
	}
	void clear() noexcept
	{
		while (!empty()) pop_back();
	}
	void resize(size_type n)
	{
		reserve(n);
		while (n < size()) pop_back();
		while (n > size()) new (end_ptr++) value_type;
	}
	void reserve(size_type n)
	{
		if (n > capacity()) grow_if_necessary(n - size());
	}

	reference front()
	{
		return *begin();
	}
	const_reference front() const
	{
		return *begin();
	}
	reference back()
	{
		return *(end() - 1);
	}
	const_reference back() const
	{
		return *(end() - 1);
	}
	reference operator[](size_type n) noexcept
	{
		return begin()[n];
	}
	const_reference operator[](size_type n) const noexcept
	{
		return begin()[n];
	}

	bool operator==(const SettledVector & other) const
	{
		if (size() != other.size()) return false;
		return std::equal(begin(), end(), other.begin());
	}


private:
	detail::MMappedMemory memory;
	pointer end_ptr;

	void grow_if_necessary(size_type n)
	{
		if (static_cast<size_type>(static_cast<const_pointer>(memory.end()) - end_ptr) >= n) return;
		memory.grow(sizeof(value_type), std::max(n, capacity()));
		if (!end_ptr) end_ptr = static_cast<pointer>(memory.begin());
	}
};
