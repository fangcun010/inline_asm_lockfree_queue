add_library(lockfree_queue lockfree_queue.cpp)
target_include_directories(lockfree_queue PRIVATE ../include)
target_sources(lockfree_queue PRIVATE ../include/lockfree_queue.h)

add_executable(test_lockfree_queue test_lockfree_queue.cpp)
target_include_directories(test_lockfree_queue PRIVATE ../include)
target_link_libraries(test_lockfree_queue lockfree_queue)