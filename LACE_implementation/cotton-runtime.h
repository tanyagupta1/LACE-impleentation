#include <iostream>
#define QUEUE_SIZE 1000000
namespace cotton_runtime
{
    struct task
    {
        std::function<void()> lambda;
    };
    
    class deque
    {
        private:
            task* tasks[QUEUE_SIZE];
            volatile int head;
            volatile int tail;
            volatile int split;
            bool allstolen;
            bool splitreq;
        public:
            deque();
            bool push(task*);
            task* steal();
            task* pop();
            void grow_shared();
            void shrink_shared();
    };


    void* worker_routine(void*);
    void find_and_execute_task();
    int get_no_of_workers();
    int get_index(void);
    void index_key_destruct(void *value);

}
