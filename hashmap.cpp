#include <iostream>
#include <ctime>
#include <cstdlib>
#include <unordered_map>
#include <algorithm>
#include <string>
using namespace std;

#include "hashmap.h"
using namespace kirby;

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

template<class Hashmap>
void stuff(vector<int> &v, Hashmap &basic, bool iterator_test = false, bool erase_test = false, bool at_test = false, bool find_test = false) {
	constexpr int test = 1024 * 1024;

	constexpr float LOAD = 0.50;

	basic.clear();

	if (!basic.empty())
		cout << "We cheated clear test." << endl;

	for (int i = 0; i < test * float(LOAD); ++i) {
		basic[v[i]] = v[i] + 10;
		if (basic.size() != i + 1)
			cout << "We cheated insertion test." << endl;
		if (at_test) {
			if (basic.at(v[i]) != v[i] + 10)
				cout << "If we cheated at, exception should be thrown." << endl;
		}
		int lim = 32 < i ? 32 : i;
		for (int j = 0; j < lim; ++j) {
			if (basic.count(v[j]) == 0)
				cout << "We cheated; wrote over a previous insertion." << endl;
		}
	}

	for (int i = 0; i < test * float(LOAD); ++i) {
		if (++basic[v[i]] != v[i] + 11)
			cout << "We cheated increment test (not correctly incrementing uniquely once)." << endl;
	}

	if (find_test) {
		for (int i = 0; i < test * float(LOAD); ++i) {
			if (basic.find(v[i] + 99000000) != basic.end())
				cout << "We cheated find test (found element not present)." << endl;
		}
	}

	if (iterator_test) {
		int total = 0;
		for (const auto& kv : basic) {
			++total;
			if (basic.find(kv.first) == basic.end())
				cout << "We cheated the iterator-key-not-found test." << endl;
			if (kv.second != kv.first + 11)
				cout << "We cheated the iterator-value-not-correct test: " << kv.second << " " << kv.first << endl;
		}
		if (total - 1 != (int)(test * float(LOAD)))
			cout << "We cheated iterator count test." << endl;
	}

	if (erase_test) {
		for (int i = 0; i < test * float(LOAD); ++i) {
			if (basic.erase(v[i]) != 1)
				cout << "We cheated erase test (erase returned 0): " << v[i] << ", i = " << i << endl;
			if (basic.count(v[i]) != 0)
				cout << "We cheated erase test (Count returned 1): " << v[i] << ", i = " << i << endl;
			if (i < test * float(LOAD) - 40) {
				for (int j = 4; j < 36; ++j) {
					if (basic.count(v[test * float(LOAD) - j]) == 0)
						cout << "We cheated; erased prematurely." << endl;
				}
			}
		}

		if (!basic.empty())
			cout << "We cheated countdown to 0 test." << endl;
	}
}

template<class Hashmap>
void time_stuff(vector<int> &v, Hashmap &h, string s) {
	clock_t startTime;
	double secondsPassed;
	int trials;
	const int all_trials = 1;
	const int test = 1024 * 1024;
	startTime = clock();
	trials = all_trials;
	do {
		stuff<Hashmap>(v, h, false, true, true);
	} while (--trials);
	secondsPassed = double(clock() - startTime) / double(CLOCKS_PER_SEC);
	cout << s << " Algo Time: " << secondsPassed << " seconds." << endl;
}

int main() {
	const int test = 1024 * 1024;
	unordered_map<int, int> original(test);
	lin_hashmap<int, int> lin(test);
	quad_hashmap<int, int> quad(test);
	rh_hashmap<int, int> rh(test);
	cc_hashmap<int, int> cc(test);

	vector<int> v(4 * 1024 * 1024);
	for (int i = 0; i < 4 * 1024 * 1024; ++i)
		v[i] = i;

	random_shuffle(v.begin(), v.end());

	time_stuff<unordered_map<int, int>>(v, original, "Original");
	time_stuff<lin_hashmap<int, int>>(v, lin, "Linear");
	time_stuff<quad_hashmap<int, int>>(v, quad, "Quadratic");
	time_stuff<rh_hashmap<int, int>>(v, rh, "Robin Hood");
	time_stuff<cc_hashmap<int, int>>(v, cc, "Cuckoo");

	return 0;
}