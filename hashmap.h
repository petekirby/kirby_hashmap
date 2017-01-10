#ifndef KIRBY_HASHMAP_H
#define KIRBY_HASHMAP_H

#include <tuple>
#include <vector>
#include <cinttypes>

/* Copyright 2017 Peter Kirby

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions :

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
IN THE SOFTWARE. */

namespace kirby {
	template <class Key>
	struct hash_function;

	template <>
	struct hash_function<int32_t> {
		std::size_t operator()(const int32_t& k) const {
			uint32_t h1 = k; // MurmurHash3
			h1 ^= h1 >> 16;
			h1 *= 0x85ebca6b;
			h1 ^= h1 >> 13;
			h1 *= 0xc2b2ae35;
			h1 ^= h1 >> 16;
			return h1;
		}
	};

	template <class Key>
	struct alt_hash_function;

	template <>
	struct alt_hash_function<int32_t> {
		std::size_t operator()(const int32_t& k) const {
			uint32_t h2 = k; // Thomas Wang
			h2 += ~(h2 << 15);
			h2 ^= (h2 >> 10);
			h2 += (h2 << 3);
			h2 ^= (h2 >> 6);
			h2 += ~(h2 << 11);
			h2 ^= (h2 >> 16);
			return h2;
		}
	};

	template <typename Derived, class Key, class T, class Hash = hash_function<Key> >
	class base_hashmap
	{
	public:
		typedef uint32_t size_type;
		typedef std::pair<const Key, T> value_type;
		typedef std::pair<Key, T> internal_type;

		struct bucket {
			size_type h;
			internal_type kv;
		};

		class iterator {
		public:
			friend class base_hashmap::const_iterator;
			iterator(bucket* _ptr) : _ptr(_ptr) {}
			bool operator==(const iterator& rhs) const { return _ptr == rhs._ptr; }
			bool operator!=(const iterator& rhs) const { return _ptr != rhs._ptr; }
			iterator& operator++() { _increment(); return *this; };
			iterator operator++(int) { iterator temp(_ptr); _increment(); return temp; }
			value_type& operator*() const { return (value_type&)(_ptr->kv); }
			value_type* operator->() const { return (value_type*)(&_ptr->kv); }
		private:
			bucket* _ptr;
			void _increment() { if (_ptr->h != _last) ++_ptr;  while (_ptr->h > _last) ++_ptr; }
		};
		typedef std::pair<iterator, bool> itb_type;

		class const_iterator {
		public:
			const_iterator(iterator it) : _ptr(it._ptr) {}
			const_iterator(bucket* _ptr) : _ptr(_ptr) {}
			bool operator==(const const_iterator& rhs) const { return _ptr == rhs._ptr; }
			bool operator!=(const const_iterator& rhs) const { return _ptr != rhs._ptr; }
			const_iterator& operator++() { _increment(); return *this; };
			const_iterator operator++(int) { const_iterator temp(_ptr); _increment(); return temp; }
			value_type const& operator*() const { return reinterpret_cast<value_type const&>(_ptr->kv); }
			value_type const* operator->() const { return (value_type const*)(&_ptr->kv); }
		private:
			const bucket* _ptr;
			void _increment() { if (_ptr->h != _last) ++_ptr;  while (_ptr->h > _last) ++_ptr; }
		};

		bool empty() const noexcept { return _size == 0; }
		size_type size() const noexcept { return _size; }
		size_type max_size() const noexcept { return _absolute_max_capacity; }
		iterator begin() noexcept { return _begin_it(); }
		const_iterator begin() const noexcept { return const_iterator(_begin_it()); }
		const_iterator cbegin() const noexcept { return begin(); }
		iterator end() noexcept { return _end_it; }
		const_iterator end() const noexcept { return const_iterator(_end_it); }
		const_iterator cend() const noexcept { return end(); }
		void clear() noexcept { _resize_and_init(_max_size); }
		iterator find(const Key& k) { return iterator(find_without_inserting(k)); }
		const_iterator find(const Key& k) const { return const_iterator(find_without_inserting(k)); }
		size_type count(const Key& k) const { bucket* ptr = find_without_inserting(k); return ptr != _end; }
		T& operator[](const Key& k) { return find_while_trying(k)->kv.second; }
		T& operator[](Key&& k) { return find_while_trying(std::move(k))->kv.second; }

		T& at(const Key& k) {
			bucket* ptr = find_without_inserting(k);
			if (ptr != _end)
				return ptr->kv.second;
			throw std::out_of_range("invalid hash map<K, T> key");
		}

		const T& at(const Key& k) const {
			const bucket* ptr = find_without_inserting(k);
			if (ptr != _end)
				return ptr->kv.second;
			throw std::out_of_range("invalid hash map<K, T> key");
		}

		itb_type insert(const value_type& obj) {
			bool is_not_found = true;
			bucket* ptr = find_while_trying(obj.first, is_not_found);
			if (is_not_found) {
				ptr->kv.second = obj.second;
			}
			return itb_type(iterator(ptr), is_not_found);
		}

		itb_type insert(value_type&& obj) {
			bool is_not_found = true;
			bucket* ptr = find_while_trying(std::move(obj.first), is_not_found);
			if (is_not_found) {
				ptr->kv.second = std::move(obj.second);
			}
			return itb_type(iterator(ptr), is_not_found);
		}

		size_type erase(const Key& k) {
			bucket* ptr = find_without_inserting(k);
			bool is_found = ptr != _end;
			if (is_found) {
				remove(ptr);
			}
			return is_found;
		}

	private:
		iterator _begin_it() const {
			iterator ans(_start);
			if (_start->h >= _tombstone)
				++ans;
			return ans;
		}

		void _resize_and_init(size_type n) {
			_table.assign(n + _overflow_area_size + 1, { _empty, internal_type() });
			_table.resize(n + _overflow_area_size + 1);
			_init();
		}

		void _rehash() {
			size_type new_total_size = _next_size_up(_max_size + 1) + _overflow_area_size + 1;
			std::vector<bucket> temp_table(new_total_size, { _empty, internal_type() });
			_table.swap(temp_table);
			_init();
			for (auto&& old : temp_table) {
				if (old.h < _code) {
					bool is_not_found;
					bucket* ptr = find_while_trying(std::move(old.kv.first), is_not_found, old.h);
					ptr->kv.second = std::move(old.kv.second);
				}
			}
		}

		static size_type _next_size_up(size_type n) {
			size_type ans = _absolute_max_capacity;
			size_type growth_factor = (n >= _slow_growth_at) ? _slow_growth_factor : _fast_growth_factor;
			if (n <= (size_type(1) << _bit_shift)) {
				ans = _initial_default_size;
				while (ans < n) {
					ans <<= growth_factor;
				}
			}
			return ans;
		}

		void _init() {
			bool is_not_overloaded = (_true_size < _max_true_size);
			_size = 0;
			_true_size = 0;
			_max_size = _table.size() - _overflow_area_size - 1;
			_table[_max_size + _overflow_area_size].h = _last;
			_start = &_table[0];
			_end = _start + _max_size + _overflow_area_size;
			_end_it = iterator(_end);
			_mask = _max_size - 1;
			_half_mask = _mask >> 1; // and zero out neighborhood lower bits
			_half_mask &= ~((size_type(1) << _neighborhood) - 1);
			_half_start = _start + (_max_size >> 1);
			size_type new_max_true_size = size_type(float(_max_size) * _max_load_factor);
			if (is_not_overloaded || new_max_true_size > _max_true_size)
				_max_true_size = new_max_true_size;
			else // increase by half, ensure it is a multiple of 8
				_max_true_size += ((_max_size - _max_true_size) >> 4) << 3;
			// blow up under certain size conditions to prevent mysterious bugs
		}

	private:
		std::vector<bucket> _table;
		bucket* _start;
		bucket* _end;
		bucket* _half_start;
		size_type _mask;
		size_type _half_mask;
		size_type _size = 0;
		size_type _true_size = 0;
		size_type _max_size = 0;
		size_type _max_true_size = 0;
		size_type _overflow_area_size = 0;
		size_type _neighborhood = 0;
		Hash _m_hash;
		float _max_load_factor = 0.51f;
		iterator _end_it{ nullptr };

	protected:
		static constexpr size_type _initial_default_size = 8;
		static constexpr size_type _slow_growth_at = 256 * 1024;
		static constexpr size_type _slow_growth_factor = 1;
		static constexpr size_type _fast_growth_factor = 3;
		static constexpr size_type _size_type_max = UINT32_MAX;
		static constexpr size_type _bit_shift = 31;
		static constexpr size_type _empty = _size_type_max;
		static constexpr size_type _tombstone = _size_type_max - 1;
		static constexpr size_type _last = _size_type_max - 2;
		static constexpr size_type _code = _size_type_max - 3;
		static constexpr size_type _absolute_max_capacity = (size_type(1) << _bit_shift);
		static constexpr size_type _max_hash = _absolute_max_capacity - 1;
		bucket* _start_ptr() { return _start; }
		bucket* _end_ptr() { return _end; }
		bucket* _half_start_ptr() { return _half_start; }
		const bucket* _start_ptr() const { return _start; }
		const bucket* _end_ptr() const { return _end; }
		const bucket* _half_start_ptr() const { return _half_start; }
		size_type _bit_mask() const { return _mask; }
		size_type _half_bit_mask() const { return _half_mask; }

		explicit base_hashmap(size_type n, const Hash& hf = Hash(), size_type overflow = 0, size_type neighborhood = 0)
			: _table(_next_size_up(n) + overflow + 1, { _empty, internal_type() }),
			_m_hash(hf), _overflow_area_size(overflow), _neighborhood(neighborhood) {
			_init();
		}

		size_type _calc_hash(const Key& k) const {
			return _m_hash(k) & _max_hash;
		}

		template<class FwdKey>
		bucket* _insert(bucket* ptr, FwdKey&& k, size_type h) {
			if (++_true_size <= _max_true_size) {
				++_size;
				ptr->kv.first = std::forward<FwdKey>(k);
				ptr->h = h;
				return ptr;
			}
			return _insert_while_full(ptr, std::forward<FwdKey>(k), h);
		}

		template<class FwdKey>
		bucket* _insert_while_full(bucket* ptr, FwdKey&& k, size_type h) {
			*ptr = { _empty, internal_type() };
			_rehash();
			bool is_not_found;
			return find_while_trying(std::forward<FwdKey>(k), is_not_found, h);
		}

		template<class FwdKey>
		bucket* find_while_trying(FwdKey&& k) {
			bool is_not_found;
			return find_while_trying(std::forward<FwdKey>(k), is_not_found);
		}

		template<class FwdKey>
		bucket* find_while_trying(FwdKey&& k, bool &is_not_found) {
			size_type h = _calc_hash(k);
			return find_while_trying(std::forward<FwdKey>(k), is_not_found, h);
		}

		void _remove_for_tombstone(bucket* ptr) {
			--_size;
			ptr->h = _tombstone;
			ptr->kv = internal_type();
		}

		void _remove_for_empty(bucket* ptr) {
			--_size;
			--_true_size;
			ptr->h = _empty;
			ptr->kv = internal_type();
		}

		bucket* find_without_inserting(const Key& k) const {
			return static_cast<Derived const*>(this)->d_find_without_inserting(k);
		}

		template<class FwdKey>
		bucket* find_while_trying(FwdKey&& k, bool &is_not_found, const size_type h) {
			return static_cast<Derived*>(this)->d_find_while_trying(
				std::forward<FwdKey>(k), is_not_found, h);
		}

		void remove(bucket* ptr) {
			static_cast<Derived*>(this)->d_remove(ptr);
		}
	};

	template <typename Derived, class Key, class T, class Hash = hash_function<Key> >
	class probing_hashmap : public base_hashmap<probing_hashmap<Derived, Key, T, Hash>, Key, T, Hash>
	{
	public:
		typedef typename kirby::base_hashmap<probing_hashmap<Derived, Key, T, Hash>, Key, T, Hash> Base;
		typedef typename Base::size_type size_type;
		typedef typename Base::value_type value_type;
		typedef typename Base::internal_type internal_type;
		typedef typename Base::itb_type itb_type;
		typedef struct Base::bucket bucket;
		typedef class Base::iterator iterator;
		probing_hashmap() : probing_hashmap(base_hashmap<probing_hashmap<Derived, Key, T, Hash>, Key, T, Hash>::_initial_default_size) {}
		explicit probing_hashmap(size_type n, const Hash& hf = Hash())
			: base_hashmap<probing_hashmap<Derived, Key, T, Hash>, Key, T, Hash>::base_hashmap(n, hf) {}

		bucket* d_find_without_inserting(const Key& k) const {
			constexpr size_type empty = this->_empty;
			const size_type mask = this->_bit_mask();
			const size_type h = this->_calc_hash(k);
			const bucket* const start = this->_start_ptr();
			size_type iteration = 0;
			size_type index = h;     // & mask;
			const bucket* ptr;       // = start + index;
			do {
				index &= mask;
				ptr = start + index;
				if (ptr->h == h && ptr->kv.first == k) {
					return const_cast<bucket*>(ptr);
				}
				index += Derived::probe(++iteration, h);
			} while (ptr->h != empty);
			return const_cast<bucket*>(this->_end_ptr());
		}

		template<class FwdKey>
		bucket* d_find_while_trying(FwdKey&& k, bool &is_not_found, const size_type h) {
			constexpr size_type empty = this->_empty;
			const size_type mask = this->_bit_mask();
			bucket* const start = this->_start_ptr();
			size_type iteration = 0;
			size_type index = h; // & mask;
			bucket* ptr;         // = start + index;
			do {
				index &= mask;
				ptr = start + index;
				if (ptr->h == empty) {
					return this->_insert(ptr, std::forward<FwdKey>(k), h);
				}
				index += Derived::probe(++iteration, h);
			} while (ptr->h != h || ptr->kv.first != k);
			is_not_found = false;
			return ptr;
		}

		void d_remove(bucket* ptr) {
			static_cast<Derived*>(this)->dd_remove(ptr);
		}
	};

	template <class Key, class T, class Hash = hash_function<Key> >
	class lin_hashmap : public probing_hashmap<lin_hashmap<Key, T, Hash>, Key, T, Hash>
	{
	public:
		typedef typename kirby::probing_hashmap<lin_hashmap<Key, T, Hash>, Key, T, Hash> Parent;
		typedef typename Parent::Base Base;
		typedef typename Base::size_type size_type;
		typedef typename Base::bucket bucket;
		lin_hashmap() : lin_hashmap(Base::_initial_default_size) {}
		explicit lin_hashmap(size_type n, const Hash& hf = Hash()) : Parent::probing_hashmap(n, hf) {}

		void dd_remove(bucket* ptr) {
			constexpr size_type empty = this->_empty;
			const size_type mask = this->_bit_mask();
			bucket* const start = this->_start_ptr();
			size_type i = ptr - start;
			size_type j = i;
			do {
				++j;
				j &= mask;
				bucket* j_ptr = start + j;
				if (j_ptr->h == empty) {
					break;
				}
				// Skip swap "if r lies cyclically between i and j." -Knuth, 3, 6.4
				size_type r = j_ptr->h & mask;
				if (i < j ? (r <= i || r > j) : (r <= i && r > j)) {
					*ptr = std::move(*j_ptr);
					ptr = j_ptr;
					i = j;
				}
			} while (true);
			this->_remove_for_empty(ptr);
		}

		static size_type probe(size_type iteration, size_type hash) {
			return 1;
		}
	};

	template <class Key, class T, class Hash = hash_function<Key> >
	class quad_hashmap : public probing_hashmap<quad_hashmap<Key, T, Hash>, Key, T, Hash>
	{
	public:
		typedef typename kirby::probing_hashmap<quad_hashmap<Key, T, Hash>, Key, T, Hash> Parent;
		typedef typename Parent::Base Base;
		typedef typename Base::size_type size_type;
		typedef typename Base::bucket bucket;
		quad_hashmap() : quad_hashmap(Base::_initial_default_size) {}
		explicit quad_hashmap(size_type n, const Hash& hf = Hash()) : Parent::probing_hashmap(n, hf) {}

		void dd_remove(bucket* ptr) {
			this->_remove_for_tombstone(ptr);
		}

		static size_type probe(size_type iteration, size_type hash) {
			return iteration;
		}
	};

	template <class Key, class T, class Hash = hash_function<Key> >
	class rh_hashmap : public base_hashmap<rh_hashmap<Key, T, Hash>, Key, T, Hash>
	{
	public:
		typedef typename kirby::base_hashmap<rh_hashmap<Key, T, Hash>, Key, T, Hash> Base;
		typedef typename Base::size_type size_type;
		typedef typename Base::value_type value_type;
		typedef typename Base::internal_type internal_type;
		typedef typename Base::itb_type itb_type;
		typedef struct Base::bucket bucket;
		typedef class Base::iterator iterator;
		rh_hashmap() : rh_hashmap(Base::_initial_default_size) {}
		explicit rh_hashmap(size_type n, const Hash& hf = Hash()) : Base::base_hashmap(n, hf, overflow_area_size) {}

		bucket* d_find_without_inserting(const Key& k) const {
			constexpr size_type code = this->_code;
			const size_type h = this->_calc_hash(k);
			const bucket* ptr = this->_start_ptr() + (h & this->_bit_mask());
			while (ptr->h < code) {
				if (ptr->h == h && ptr->kv.first == k) {
					return const_cast<bucket*>(ptr);
				}
				++ptr;
			}
			return const_cast<bucket*>(this->_end_ptr());
		}

		static bool shift_buckets_forward(bucket* const ptr, const size_type empty, const bucket* const end) {
			bucket* freespace = ptr;
			do {
				if ((++freespace)->h == empty) {
					bucket* leader = freespace - 1;
					do {
						*(freespace--) = std::move(*(leader--));
					} while (freespace != ptr);
					return true;
				}
			} while (freespace != end);
			return false;
		}

		template<class FwdKey>
		bucket* d_find_while_trying(FwdKey&& k, bool &is_not_found, const size_type h)
		{
			constexpr size_type empty = this->_empty;
			const size_type mask = this->_bit_mask();
			const size_type starting_index = h & mask;
			bucket* const end = this->_end_ptr();
			for (bucket* ptr = this->_start_ptr() + starting_index; ptr != end; ++ptr) {
				if (ptr->h == h && ptr->kv.first == k) {
					is_not_found = false;
					return ptr;
				} else if (ptr->h == empty) {
					return this->_insert(ptr, std::forward<FwdKey>(k), h);
				} else if ((ptr->h & mask) > starting_index) {
					if (shift_buckets_forward(ptr, empty, end)) {
						return this->_insert(ptr, std::forward<FwdKey>(k), h);
					} else {
						return this->_insert_while_full(ptr, std::forward<FwdKey>(k), h);
					}
				}
			}
			return this->_insert_while_full(end, std::forward<FwdKey>(k), h);
		}

		static bucket* shift_buckets_back(bucket* ptr, size_type i, const size_type mask, const size_type code) {
			bucket* iPtr = ptr + 1;
			for (++i; iPtr->h < code && (iPtr->h & mask) < i; ++ptr, ++i, ++iPtr) {
				*ptr = std::move(*iPtr);
			}
			return ptr;
		}

		void d_remove(bucket* ptr) {
			ptr = shift_buckets_back(ptr, ptr - this->_start_ptr(), this->_bit_mask(), this->_code);
			this->_remove_for_empty(ptr);
		}

		static constexpr size_type overflow_area_size = 128;
	};

	template <class Key, class T, class Hash = hash_function<Key>, class AltHash = alt_hash_function<Key> >
	class cc_hashmap : public base_hashmap<cc_hashmap<Key, T, Hash>, Key, T, Hash >
	{
	public:
		typedef typename kirby::base_hashmap<cc_hashmap<Key, T, Hash>, Key, T, Hash> Base;
		typedef typename Base::size_type size_type;
		typedef typename Base::value_type value_type;
		typedef typename Base::internal_type internal_type;
		typedef typename Base::itb_type itb_type;
		typedef struct Base::bucket bucket;
		typedef class Base::iterator iterator;
		cc_hashmap() : cc_hashmap(Base::_initial_default_size) {}
		explicit cc_hashmap(size_type n, const Hash& hf = Hash(), const AltHash& ahf = AltHash())
			: Base::base_hashmap(n, hf), m_alt_hash(ahf) {}

		size_type calc_alt_hash(const Key& k) const {
			return m_alt_hash(k) & this->_max_hash;
		}

		bucket* d_find_without_inserting(const Key& k) const {
			const size_type half_mask = this->_half_bit_mask();
			const size_type h = this->_calc_hash(k);
			const bucket* ptr = this->_start_ptr() + (h & half_mask);
			if (ptr->h == h && ptr->kv.first == k) {
				return const_cast<bucket*>(ptr);
			}
			const size_type ah = this->calc_alt_hash(k);
			const bucket* aptr = this->_half_start_ptr() + (ah & half_mask);
			if (aptr->h == ah && aptr->kv.first == k) {
				return const_cast<bucket*>(aptr);
			}
			return const_cast<bucket*>(this->_end_ptr());
		}

		template<class FwdKey>
		bucket* cuckoo_insert(bucket* first_ptr, FwdKey&& k, size_type first_h) {
			constexpr size_type empty = this->_empty;
			bucket* kick_list[max_search] = { nullptr };
			size_type h, half_mask = this->_half_bit_mask();
			bucket* const start = this->_start_ptr();
			bucket* const half_start = this->_half_start_ptr();
			bucket* ptr = first_ptr;
			int depth = 0;
			kick_list[depth++] = ptr;
			do {
				h = calc_alt_hash(ptr->kv.first);
				ptr = half_start + (h & half_mask);
				kick_list[depth++] = ptr;
				if (ptr->h == empty) {
					break;
				}
				ptr->h = h;
				h = this->_calc_hash(ptr->kv.first);
				ptr = start + (h & half_mask);
				kick_list[depth++] = ptr;
				if (ptr->h == empty) {
					break;
				}
				ptr->h = h;
				if (depth >= max_search - 1) {
					return this->_insert_while_full(this->_end_ptr(), std::forward<FwdKey>(k), first_h);
				}
			} while (true);
			ptr->h = h;
			for (int i = depth - 1; i; --i) {
				kick_list[i]->kv = std::move(kick_list[i - 1]->kv);
			}
			return this->_insert(first_ptr, std::forward<FwdKey>(k), first_h);
		}

		template<class FwdKey>
		bucket* d_find_while_trying(FwdKey&& k, bool &is_not_found, const size_type unused_h) {
			constexpr size_type empty = this->_empty;
			const size_type half_mask = this->_half_bit_mask();
			const size_type h = this->_calc_hash(k);
			bucket* ptr = this->_start_ptr() + (h & half_mask);
			if (ptr->h == empty) {
				return this->_insert(ptr, std::forward<FwdKey>(k), h);
			}
			if (ptr->h == h && ptr->kv.first == k) {
				is_not_found = false;
				return ptr;
			}
			const size_type ah = this->calc_alt_hash(k);
			bucket* aptr = this->_half_start_ptr() + (ah & half_mask);
			if (aptr->h == empty) {
				return this->_insert(aptr, std::forward<FwdKey>(k), ah);
			}
			if (aptr->h == ah && aptr->kv.first == k) {
				is_not_found = false;
				return aptr;
			}
			return cuckoo_insert(ptr, std::forward<FwdKey>(k), h);
		}

		void d_remove(bucket* ptr) {
			this->_remove_for_empty(ptr);
		}

	private:
		static constexpr int max_search = 128;
		AltHash m_alt_hash;
	};
}
#endif