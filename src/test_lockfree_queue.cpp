/*
MIT License
Copyright(c) 2019 fangcun
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files(the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions :
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#include <iostream>
#include <thread>
#include <lockfree_queue.h>
#include <set>
#include <atomic>
#include <queue>
#include <mutex>
#include <Windows.h>


std::queue<unsigned int> std_queue;
std::queue<unsigned int> order_std_queue;
std::mutex std_queue_mutex;

int main() {
	auto freelist = lockfree_create_freelist(4);
	auto queue = lockfree_create_queue(freelist);
	auto order_queue = lockfree_create_queue(freelist);
	std::thread *read_threads[10];
	std::thread *write_threads[10];
	bool is_write_end[10];

	for (unsigned int i = 0; i < 2000000; i++) {
		lockfree_queue_push(queue, &i);
	}
	for (unsigned int i = 0; i < 2000000; i++) {
		unsigned int val;
		lockfree_queue_pop(queue, &val);
	}

	std::cout << "Test std::queue with std::mutex" << std::endl;

	auto start_time = std::chrono::high_resolution_clock::now();
	for (unsigned int i = 0; i < 10; i++) {
		is_write_end[i] = false;
		write_threads[i] = new std::thread([&is_write_end = is_write_end[i], id = i, queue = queue, order_queue = order_queue]() {
			auto handle = GetCurrentThread();
			SetThreadAffinityMask(handle, (1<<2));
			std::cout << "start write:" << id << std::endl;
			for (unsigned int i = 0; i < 2000000; i++) {
				unsigned int val = id * 2000000 + i;
				std_queue_mutex.lock();
				std_queue.push(val);
				std_queue_mutex.unlock();
			}
			std::cout << "end write:" << id << std::endl;
			is_write_end = true;
		});
	}

	for (unsigned int i = 0; i < 10; i++) {
		read_threads[i] = new std::thread([is_write_end = is_write_end, id = i, queue = queue, order_queue = order_queue]() {
			auto handle = GetCurrentThread();
			SetThreadAffinityMask(handle, (1 << 2));
			std::cout << "start read:" << id << std::endl;
			int try_cnt = 0;
			for (; try_cnt < 20;) {
				std_queue_mutex.lock();
				if (std_queue.empty() == false) {
					auto val = std_queue.front();
					std_queue.pop();
					order_std_queue.push(val);
					try_cnt = 0;
				}
				
				std_queue_mutex.unlock();
				
				bool write_end = true;
				for (auto i = 0; i < 10; i++) {
					if (is_write_end[i] == false) {
						write_end = false;
						break;
					}
				}
				if (write_end)
					try_cnt++;
			}
			std::cout << "end read:" << id << std::endl;
		});
	}


	for (unsigned int i = 0; i < 10; i++) {
		read_threads[i]->join();
	}
	for (unsigned int i = 0; i < 10; i++) {
		write_threads[i]->join();
	}

	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start_time);

	std::cout << "sync" << std::endl;

	std::cout << "std_queue time:" << duration.count() << "ms" << std::endl;

	start_time = std::chrono::high_resolution_clock::now();
		for (unsigned int i = 0; i < 10; i++) {
			is_write_end[i] = false;
			write_threads[i] = new std::thread([&is_write_end = is_write_end[i], id = i, queue = queue, order_queue = order_queue]() {
				auto handle = GetCurrentThread();
				SetThreadAffinityMask(handle, (1 << 2));
				std::cout << "start write:" << id << std::endl;
				for (unsigned int i = 0; i < 2000000; i++) {
					unsigned int val = id * 2000000 + i;
					lockfree_queue_push(queue, &val);
				}
				std::cout << "end write:" << id << std::endl;
				is_write_end = true;
			});
		}

		for (unsigned int i = 0; i < 10; i++) {
			read_threads[i] = new std::thread([is_write_end=is_write_end, id = i, queue = queue, order_queue = order_queue]() {
				auto handle = GetCurrentThread();
				SetThreadAffinityMask(handle, (1 << 2));
				std::cout << "start read:" << id << std::endl;
				int try_cnt = 0;
				for (; try_cnt<20;) {
					unsigned int val;
					bool ret = lockfree_queue_pop(queue, &val);
					if (ret) {
						try_cnt = 0;
						lockfree_queue_push(order_queue, &val);
						continue;
					}
					bool write_end = true;
					for (auto i = 0; i < 10; i++) {
						if (is_write_end[i]==false) {
							write_end = false;
							break;
						}
					}
					if (write_end)
						try_cnt++;
				}
				std::cout << "end read:" << id << std::endl;
			});
		}


		for (unsigned int i = 0; i < 10; i++) {
			read_threads[i]->join();
		}
		for (unsigned int i = 0; i < 10; i++) {
			write_threads[i]->join();
		}

		duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start_time);

		std::cout << "sync" << std::endl;

		std::set<unsigned int> ss;
		unsigned int val;
		unsigned int cnt = 0;

		unsigned int cnt1 = 0;
		while (lockfree_queue_pop(queue, &val)) {
			ss.insert(val);
			cnt1++;
		}
		unsigned int cnt2 = 0;
		while (lockfree_queue_pop(order_queue, &val)) {
			ss.insert(val);
			cnt2++;
		}
		lockfree_freelist_node *top = freelist->top;
		while (top) {
			top = top->next;
			cnt++;
		}
		std::cout << "lockfree queue time:" << duration.count()<<"ms" << std::endl;
		std::cout << "set size:" << ss.size() << std::endl;
		std::cout << "node count:" << cnt<<","<<cnt1<<","<<cnt2 << std::endl;
	
	lockfree_destroy_queue(queue);
	lockfree_destroy_queue(order_queue);
	lockfree_destroy_freelist(freelist);

	return 0;
}