#include <iostream>
#define QUEUE_SIZE 1000000

namespace cotton_runtime
{
    struct task
    {
        std::function<void()> lambda;
    };
    
    class circular_queue
    {
        private:
            task* tasks[QUEUE_SIZE];
            volatile int front;
            volatile int rear;
            volatile int count;
        public:
            circular_queue();
            bool push(task*);
            task* steal();
            task* pop();
            bool is_empty();
            bool is_full();
            int get_count();
    };


    void* worker_routine(void*);
    void find_and_execute_task();
    int get_no_of_workers();
    int get_index(void);
    void index_key_destruct(void *value);

}