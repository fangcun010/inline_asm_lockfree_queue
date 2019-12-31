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

#include <Windows.h>
int main() {
	auto freelist = lockfree_create_freelist(4);
	auto queue = lockfree_create_queue(freelist);
	auto order_queue = lockfree_create_queue(freelist);
	std::thread *read_threads[10];
	std::thread *write_threads[10];
	std::atomic<bool> is_write_end[2];

	for (unsigned int test_cnt=0;test_cnt<20;test_cnt++) {

		for (unsigned int i = 0; i < 2; i++) {
			is_write_end[i] = false;
			write_threads[i] = new std::thread([&is_write_end=is_write_end[i],id = i, queue = queue, order_queue = order_queue]() {
				std::cout << "start write:" <<id<< std::endl;
				for (unsigned int i = 0; i < 5000000; i++) {
					lockfree_queue_push(queue, &i);
				}
				std::cout << "end write:" << id << std::endl;
				is_write_end = true;
			});
		}

		for (unsigned int i = 0; i < 5; i++) {
			read_threads[i] = new std::thread([&is_write_end1=is_write_end[0],&is_write_end2=is_write_end[1]
				,id = i, queue = queue, order_queue = order_queue]() {
				std::cout << "start read:" << id << std::endl;
				for (; is_write_end1==false || is_write_end2==false || lockfree_queue_empty(queue)==false;) {
					unsigned int val;
					bool ret = lockfree_queue_pop(queue, &val);
					if (ret)
						lockfree_queue_push(order_queue, &val);
				}
				std::cout << "end read:" << id << std::endl;
			});
		}


		for (unsigned int i = 0; i < 5; i++) {
			read_threads[i]->join();
		}
		for (unsigned int i = 0; i < 2; i++) {
			write_threads[i]->join();
		}
		std::cout << "sync" << std::endl;

		std::set<unsigned int> ss;
		unsigned int val;
		unsigned int cnt = 0;
		while (lockfree_queue_pop(order_queue, &val)) {

			ss.insert(val);
			cnt++;
		}
		std::cout << "test cnt:" << test_cnt + 1 << std::endl;
		std::cout <<"set size:"<< ss.size() << std::endl;
		std::cout <<"node count:"<< cnt << std::endl;
	}
	lockfree_destroy_queue(queue);
	lockfree_destroy_queue(order_queue);
	lockfree_destroy_freelist(freelist);

	return 0;
}