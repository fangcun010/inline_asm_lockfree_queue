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
#include <malloc.h>
#include <lockfree_queue.h>

extern "C" {
	lockfree_freelist * __cdecl lockfree_create_freelist(
		unsigned int node_size) {
		__asm {
			push LOCKFREE_ALIGN_SIZE
			push TYPE lockfree_freelist
			call _aligned_malloc
			add esp, 8

			cmp eax,0
			
			jne alloc_malloc_ok

			jmp exit_function

			alloc_malloc_ok:
			mov ebx,0
			mov [eax],ebx
			mov ebx,node_size
			mov [eax+4],ebx

			exit_function:
		}
	}

	void __cdecl lockfree_destroy_freelist(lockfree_freelist *freelist) {
		__asm {
			push freelist
			call lockfree_freelist_clear
			add esp,4

			push freelist
			call _aligned_free
			add esp,4
		}
	}

	void __cdecl lockfree_freelist_push(lockfree_freelist *freelist, void *node) {
		__asm {
			mov ebx,freelist
			mov ecx,node
			mov eax, [ebx]
			cas_loop_start:
			mov [ecx],eax
			lock cmpxchg [ebx],ecx
			jnz cas_loop_start
		}
	}

	void *__cdecl lockfree_freelist_pop(lockfree_freelist *freelist) {
		__asm {
			mov ebx,freelist
			mov eax, [ebx]
			cas_loop_start:
			cmp eax,0
			je exit_function
			mov ecx,[eax]
			lock cmpxchg [ebx],ecx
			jnz cas_loop_start
			exit_function:
		}
	}

	void * __cdecl lockfree_freelist_alloc(lockfree_freelist *freelist) {
		__asm {
			push freelist
			call lockfree_freelist_pop
			add esp,4
			cmp eax,0
			jne exit_function
			mov eax,freelist
			mov ebx,[eax+4]
			add ebx,8
			push LOCKFREE_ALIGN_SIZE
			push ebx
			call _aligned_malloc
			add esp,8

			exit_function:
		}
	}

	void __cdecl lockfree_freelist_free(lockfree_freelist *freelist, void *node) {
		__asm {
			push node
			push freelist
			call lockfree_freelist_push
			add esp,8
		}
	}

	void __cdecl lockfree_freelist_clear(lockfree_freelist *freelist) {
		__asm {
			clear_loop_start:
			push freelist
			call lockfree_freelist_pop
			add esp,4
			cmp eax,0
			je exit_function
			push eax
			call _aligned_free
			add esp,4
			jmp clear_loop_start
			exit_function:
		}
	}

	int __cdecl lockfree_freelist_empty(lockfree_freelist *freelist) {
		__asm {
			mov ebx,freelist
			mov eax,[ebx]
			cmp eax,0
			je return_true
			mov eax,0
			jmp exit_function
			return_true:
			mov eax,1
			exit_function:
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
			mov[eax+4],ebx
			mov[eax+12],ebx
			mov ebx, freelist
			mov[eax + 16], ebx
			
			push eax

			push ebx
			call lockfree_freelist_alloc
			add esp,4

			pop ebx
			mov[ebx],eax
			mov[ebx+8],eax
			mov ecx,0
			mov[eax],ecx
			mov[eax+4],ecx
			mov eax,ebx

			exit_function :
		}
	}

	void __cdecl lockfree_destroy_queue(lockfree_queue *queue) {
		__asm {
			push queue
			call lockfree_queue_clear
			add esp, 4

			mov ebx,queue
			push [ebx]
			call _aligned_free
			add esp,4

			push queue
			call _aligned_free
			add esp, 4
		}
	}

	void __cdecl lockfree_queue_push(lockfree_queue *queue, void *value) {
		unsigned long long new_ptr;
		unsigned long long tail;
		unsigned long long next;
		unsigned long long node;
		__asm {
			mov ebx,queue
			mov edx,[ebx+16]
			mov ecx,[edx+4]

			push ecx

			push edx
			call lockfree_freelist_alloc
			add esp,4

			lea ebx,node
			mov[ebx],eax
			mov ecx,0
			mov[eax],ecx
			mov[eax+4],ecx

			pop ecx
			mov esi,value
			mov edi,eax
			add edi,8

			rep movsb

			loop_start:
			
			mov edi, queue
			add edi, 8
			mov edx, 0
			mov eax, 0
			mov ecx, 0
			mov ebx, 0
				
			lock cmpxchg8b [edi]
			lea esi, tail
			mov [esi],eax
			mov[esi+4], edx

			mov edi, eax
			mov edx, 0
			mov eax, 0

			lock cmpxchg8b[edi]
			lea esi,next
			mov[esi],eax
			mov[esi+4],edx

			mov edx,queue
			lea eax,tail
			lea ebx,next

			mov ecx,[eax]
			mov edi,[edx+8]

			cmp ecx,edi
			jne loop_start

			mov ecx,[eax+4]
			mov edi,[edx+12]
			
			cmp ecx,edi
			jne loop_start
			
			mov ecx,[ebx]
			cmp ecx,0
			jne else_label
				
			lea ecx,new_ptr
			lea esi,node

			mov edi,[esi]
			mov[ecx],edi
			mov edi,[ebx+4]
			inc edi
			mov [ecx+4],edi

			mov edi,[eax]
			
			mov eax,[ebx]
			mov edx,[ebx+4]
			lea esi,new_ptr
			mov ebx,[esi]
			mov ecx,[esi+4]

			lock cmpxchg8b [edi]
			jnz loop_start

			lea ecx,new_ptr
			lea esi,node
			mov edi,[esi]
			mov [ecx],edi
			lea esi,tail
			mov edi,[esi+4]
			inc edi
			mov [ecx+4],edi

			mov edi,queue
			add edi,8

			mov eax,[esi]
			mov edx,[esi+4]
			lea esi,new_ptr
			mov ebx,[esi]
			mov ecx,[esi+4]

			lock cmpxchg8b [edi]

			jmp exit_function
			else_label:
			lea esi,new_ptr
			mov ecx,[ebx]
			mov [esi],ecx
			mov ecx,[eax+4]
			inc ecx
			mov [esi+4],ecx

			mov edi, queue
			add edi, 8

			mov edx, [eax + 4]
			mov eax, [eax]
			mov ebx, [esi]
			mov ecx, [esi + 4]

			lock cmpxchg8b [edi]

			jmp loop_start
			exit_function:
		}
	}

	int __cdecl lockfree_queue_pop(lockfree_queue *queue, void *value) {
		unsigned long long new_ptr;
		unsigned long long head;
		unsigned long long tail;
		unsigned long long next;
		unsigned long long node_size;
		__asm {
			mov ebx,queue
			mov ecx,[ebx+16]
			mov edx,[ecx+4]
			lea ebx,node_size
			mov [ebx],edx
			loop_start:
			
			mov edi,queue
			mov edx,0
			mov eax,0
			mov ecx,0
			mov ebx,0

			lock cmpxchg8b [edi]
			lea esi,head
			mov [esi],eax
			mov [esi+4],edx

			mov edi,queue
			add edi,8
			mov edx,0
			mov eax,0

			lock cmpxchg8b [edi]
			lea esi,tail
			mov [esi],eax
			mov[esi+4],edx

			lea esi,head
			mov edi,[esi]
			mov edx,0
			mov eax,0

			lock cmpxchg8b[edi]
			lea esi,next
			mov [esi],eax
			mov [esi+4],edx

			mov esi,queue
			lea eax,head
			lea ebx,tail
			lea ecx,next

			mov edx,[eax]
			mov edi,[esi]

			cmp edx,edi
			jne loop_start

			mov edx,[eax+4]
			mov edi,[esi+4]
			cmp edx,edi
			jne loop_start
			
			mov edx,[eax]
			mov edi,[ebx]
			cmp edx,edi
			jne else_label

			mov edx,[ecx]
			cmp edx,0
			jne fail_return_false
			mov eax,0
			jmp exit_function
			fail_return_false:
			
			lea edx,new_ptr
			mov edi,[ecx]
			mov [edx],edi
			mov edi,[ebx+4]
			inc edi
			mov [edx+4],edi

			mov edi,esi
			add edi,8
			
			mov eax,[ebx]
			mov edx,[ebx+4]
			lea esi,new_ptr
			mov ebx,[esi]
			mov ecx,[esi+4]

			lock cmpxchg8b [edi]

			jmp loop_start

			else_label:

			mov edi,value

			mov esi,[ecx]
			add esi,8

			lea edx,node_size
			mov ecx,[edx]

			rep movsb

			lea esi,new_ptr
			lea ecx,next
			mov edx,[ecx]
			mov [esi],edx
			mov edx,[eax+4]
			inc edx
			mov [esi+4],edx

			mov ebx,[esi]
			mov ecx,[esi+4]
			lea esi,head
			mov eax,[esi]
			mov edx,[esi+4]

			mov edi, queue

			lock cmpxchg8b [edi]
			jnz loop_start

			mov ebx,[edi+16]
			lea ecx,head

			push [ecx]
			push ebx
			call lockfree_freelist_free
			add esp,8

			mov eax,1

			exit_function:
		}
	}

	void __cdecl lockfree_queue_clear(lockfree_queue *queue) {
		unsigned int node_size = queue->freelist->node_size;
		unsigned char *node =(unsigned char *) _aligned_malloc(node_size, LOCKFREE_ALIGN_SIZE);
		while (lockfree_queue_pop(queue, node));
		_aligned_free(node);
	}

	int __cdecl lockfree_queue_empty(lockfree_queue *queue) {
		__asm {
			mov eax,queue
			mov ebx,[eax]
			mov ecx,[eax+8]
			cmp ebx,ecx
			jne return_false
			mov ebx,[eax+4]
			mov ecx,[eax+12]
			cmp ebx,ecx
			jne return_false
			mov eax,1
			jmp exit_function
			return_false:
			mov eax,0
			exit_function:
		}
	}
}