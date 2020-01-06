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
#include <stdlib.h>
#include <malloc.h>
#include <lockfree_queue.h>

extern "C" {
	static unsigned int sync_zero;

	lockfree_freelist * __cdecl lockfree_create_freelist(
		unsigned int node_size) {
		__asm {
			push LOCKFREE_ALIGN_SIZE
			push TYPE lockfree_freelist
			call _aligned_malloc
			add esp, 8

			cmp eax, 0

			jne alloc_malloc_ok

			jmp exit_function

			alloc_malloc_ok :
			mov ebx, 0
				mov[eax], ebx
				mov ebx, node_size
				mov[eax + 4], ebx

				exit_function :
		}
	}

	void __cdecl lockfree_destroy_freelist(lockfree_freelist *freelist) {
		__asm {
			push freelist
			call lockfree_freelist_clear
			add esp, 4

			push freelist
			call _aligned_free
			add esp, 4
		}
	}

	void __cdecl lockfree_freelist_push(lockfree_freelist *freelist, void *node) {
		__asm {
			
			mov ebx, freelist
			
			mov ecx, node
			
			mov eax, [ebx]
			
			cas_loop_start:
			
			mov[ecx], eax
				
				lock cmpxchg[ebx], ecx
				jnz cas_loop_start
			
		}
	}

	void *__cdecl lockfree_freelist_pop(lockfree_freelist *freelist) {
		__asm {
			mov ebx, freelist
			
			mov eax, [ebx]
			
			cas_loop_start:
			cmp eax, 0
				je exit_function
				
				mov ecx, [eax]
				
				lock cmpxchg[ebx], ecx
				jnz cas_loop_start
				exit_function :
			
		}
	}

	void * __cdecl lockfree_freelist_alloc(lockfree_freelist *freelist) {
		__asm {
			push freelist
			call lockfree_freelist_pop
			add esp, 4
			cmp eax, 0
			jne exit_function
			mov eax, freelist
			
			mov ebx, [eax + 4]
			add ebx, 8
			push LOCKFREE_ALIGN_SIZE
			push ebx
			call _aligned_malloc
			add esp, 8
			cmp eax,0
			jne exit_function
			call abort
			exit_function:
			
		}
	}

	void __cdecl lockfree_freelist_free(lockfree_freelist *freelist, void *node) {
		__asm {
			push node
			push freelist
			call lockfree_freelist_push
			add esp, 8
		}
	}

	void __cdecl lockfree_freelist_clear(lockfree_freelist *freelist) {
		__asm {
		clear_loop_start:
			push freelist
				call lockfree_freelist_pop
				add esp, 4
				cmp eax, 0
				je exit_function
				push eax
				call _aligned_free
				add esp, 4
				jmp clear_loop_start
				exit_function :
		}
	}

	int __cdecl lockfree_freelist_empty(lockfree_freelist *freelist) {
		__asm {
			mov ebx, freelist
			
			mov eax, [ebx]
			cmp eax, 0
			je return_true
			mov eax, 0
			jmp exit_function
			return_true :
			mov eax, 1
				exit_function :
		}
	}

	lockfree_queue * __cdecl lockfree_create_queue(lockfree_freelist *freelist) {
		__asm {
			push LOCKFREE_ALIGN_SIZE
			push TYPE lockfree_queue
			call _aligned_malloc
			add esp, 8

			cmp eax, 0

			jne alloc_malloc_ok

			jmp exit_function

			alloc_malloc_ok :
			mov ebx, 0
				mov[eax + 4], ebx
				mov[eax + 12], ebx
				mov ebx, freelist
				mov[eax + 16], ebx

				push eax

				push ebx
				call lockfree_freelist_alloc
				add esp, 4

				pop ebx
				mov[ebx], eax
				mov[ebx + 8], eax
				mov ecx, 0
				mov[eax], ecx
				mov[eax + 4], ecx
				mov eax, ebx

				exit_function :
		}
	}

	void __cdecl lockfree_destroy_queue(lockfree_queue *queue) {
		__asm {
			push queue
			call lockfree_queue_clear
			add esp, 4

			mov ebx, queue
			
			push[ebx]
			call _aligned_free
			add esp, 4

			push queue
			call _aligned_free
			add esp, 4
		}
	}

	void __cdecl lockfree_queue_push(lockfree_queue *queue, void *value) {
#define			register_queue				mm2
#define			register_value				mm3
#define			register_new_ptr			mm4
#define			register_tail				mm5
#define			register_next				mm6
#define			register_node				mm7
		__asm {
			
			mov ebx, queue
			
			mov edx, [ebx + 16]
			
			mov ecx, [edx + 4]

			push ecx

			push edx
			call lockfree_freelist_alloc
			add esp, 4

			
			movd register_queue, queue
			
			movd register_value, value
			
			movd register_node, eax
			
			pop ecx
			movd esi, register_value
			mov edi, eax
			add edi, 8

			rep movsb

			lock add sync_zero,0

			mov ecx, 0
			mov [eax], ecx

			
			loop_start :
			
			movd ebx, register_queue
				
				movq register_tail, [ebx + 8]
				

				movd ebx, register_tail
				
				movq register_next, [ebx]
				

				movq mm0, register_tail
				movd ebx, register_queue
				
				
				movq mm1, [ebx + 8]
				
				movd eax, mm0
				movd ebx, mm1

				cmp eax, ebx

				jne loop_start
				
				psrlq mm0, 32
				psrlq mm1, 32

				movd eax, mm0
				movd ebx, mm1

				cmp eax, ebx


				jne loop_start
				
				movd eax, register_next
				
				cmp eax, 0
				
				jne else_label

				movd edi, register_tail

				movq mm0, register_next
				movd eax, mm0
				psrlq mm0, 32
				movd edx, mm0
				movd ebx, register_node
				movd ecx, edx
				inc ecx
				
				lock cmpxchg8b[edi]

				jnz loop_start
					
				movd edi, register_queue
				add edi, 8

				movq mm0, register_tail
				movd eax, mm0
				psrlq mm0, 32
				movd edx, mm0
				mov ecx, edx
				inc ecx
				movd ebx, register_node
					
				lock cmpxchg8b[edi]

				jmp exit_function
				else_label :
				
			movd edi, register_queue
				add edi, 8

				movq mm0, register_tail
				movd eax, mm0
				psrlq mm0, 32
				movd edx, mm0
				mov ecx, edx
				inc ecx
				movd ebx, register_next
				
				lock cmpxchg8b[edi]

				jmp loop_start
				exit_function :
			
			emms
				
		}
#undef			register_queue
#undef			register_value
#undef			register_new_ptr
#undef			register_tail
#undef			register_next
#undef			register_node
	}

	int __cdecl lockfree_queue_pop(lockfree_queue *queue, void *value) {
#define			register_queue					mm2
#define			register_value					mm3
#define			register_head					mm4
#define			register_tail					mm5
#define			register_next					mm6
#define			register_node_size				mm7
		__asm {
			
			mov ebx, queue
			movd register_queue,ebx
			
			mov ecx, [ebx + 16]
			
			mov edx, [ecx + 4]
			movd register_node_size, edx

			loop_start :
			
			movd ebx, register_queue
				
				movq register_head, [ebx]
				
				movq register_tail, [ebx + 8]
				
				movd ebx, register_head
				
				movq register_next, [ebx]
				
				movq mm0, register_head
				movd ebx, register_queue
				
				movq mm1, [ebx]

				movd eax, mm0
				movd ebx, mm1

				cmp eax,ebx

				jne loop_start

				psrlq mm0, 32
				psrlq mm1,32

				movd eax,mm0
				movd ebx,mm1

				cmp eax,ebx

				jne loop_start

				movd eax,register_head
				movd ebx,register_tail

				cmp eax,ebx
				
				jne else_label

				movd eax,register_next

				cmp eax,0

				jne fail_return_false

				mov eax, 0

				emms
				jmp exit_function
				
				fail_return_false :
				
				
				movd edi,register_queue
				add edi,8

				movq mm0,register_tail
				movd eax,mm0
				psrlq mm0, 32
				movd edx,mm0
				mov ecx,edx
				inc edx
				movd ebx,register_next
				
				lock cmpxchg8b[edi]

				jmp loop_start

				else_label:
				
				
				movd esi,register_next

				
				add esi, 8
				
				mov edi,value
				movd ecx,register_node_size
					
				rep movsb
				lock add sync_zero,0
				movd edi,register_queue

				movq mm0,register_head
				movd eax,mm0
				psrlq mm0,32
				movd edx,mm0
				mov ecx,edx
				inc ecx
				movd ebx,register_next
				
				lock cmpxchg8b[edi]
				jnz loop_start

				movd ebx,register_queue
				
				mov edx,[ebx+16]
				movd ecx, register_head
				
				emms

				push ecx
				push edx
				call lockfree_freelist_free
				add esp, 8
				
				mov eax, 1

					exit_function:
				lock add sync_zero,0
		}
#undef			register_queue
#undef			register_value
#undef			register_head
#undef			register_tail
#undef			register_next
#undef			register_node_size
	}

	void __cdecl lockfree_queue_clear(lockfree_queue *queue) {
		unsigned int node_size = queue->freelist->node_size;
		unsigned char *node = (unsigned char *)_aligned_malloc(node_size, LOCKFREE_ALIGN_SIZE);
		while (lockfree_queue_pop(queue, node));
		_aligned_free(node);
	}

	int __cdecl lockfree_queue_empty(lockfree_queue *queue) {
		__asm {
			
			mov eax, queue
			
			mov ebx, [eax]
			
			mov ecx, [eax + 8]
			cmp ebx, ecx
			jne return_false
			
			mov ebx, [eax + 4]
			
			mov ecx, [eax + 12]
			cmp ebx, ecx
			jne return_false
			mov eax, 1
			jmp exit_function
			return_false :
			mov eax, 0
				exit_function :
		}
	}
}