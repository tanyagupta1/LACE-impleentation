#include "cotton.h"
#include "cotton-runtime.h"
using namespace std;

const int no_of_threads = cotton_runtime::get_no_of_workers();
pthread_t *threads = new pthread_t[no_of_threads - 1];
volatile bool shutdown = false;
volatile int finish_counter = 0;
cotton_runtime::deque *task_queue = new cotton_runtime::deque[no_of_threads]; // global task queue
pthread_mutex_t *queue_mutexes = new pthread_mutex_t[no_of_threads];
pthread_mutex_t finish_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t split_tail = PTHREAD_MUTEX_INITIALIZER;
pthread_key_t index_key;
int cotton_runtime::get_index(void)
{
    return *((int *)pthread_getspecific(index_key));
}

void cotton_runtime::index_key_destruct(void *value)
{
    free(value);
    pthread_setspecific(index_key, NULL);
}
void cotton::init_runtime()
{
    pthread_key_create(&index_key, &cotton_runtime::index_key_destruct);
    pthread_setspecific(index_key, new int(no_of_threads - 1));
    for (int i = 0; i < no_of_threads; i++)
        queue_mutexes[i] = PTHREAD_MUTEX_INITIALIZER;
    for (int i = 0; i < no_of_threads - 1; i++)
    {
        pthread_create(&threads[i], NULL, cotton_runtime::worker_routine, new int(i)); // create no_of_threads-1 workers
    }
}
void cotton::async(std::function<void()> &&lambda)
{
    pthread_mutex_lock(&finish_mutex);
    finish_counter++; // increment finish counter
    pthread_mutex_unlock(&finish_mutex);
    int thread_no = cotton_runtime::get_index();
    cotton_runtime::task *t;
    t = new cotton_runtime::task(); // create new task
    t->lambda = lambda;             // assign the respective function to the task
    if (!task_queue[thread_no].push(t)) // try to enqueue task to global queue
    {
        delete t;
        throw length_error("Cannot push task to Queue, Please increase QUEUE_SIZE and build again"); // throw error if queue is full
    }
}
void cotton::start_finish()
{
    finish_counter = 0; // initialise finish counter to 0
}
void cotton::end_finish()
{
    while (finish_counter != 0)
    {
        cotton_runtime::find_and_execute_task(); // while tasks are remaining, the main thread also executes tasks from global queue
    }
}
void cotton::finalize_runtime()
{
    shutdown = true;
    for (int i = 0; i < no_of_threads - 1; i++)
    {
        pthread_join(threads[i], NULL); // join all no_of_threads-1 workers
    }
    delete[] threads; // free worker memory
    delete[] task_queue;
    delete[] queue_mutexes;
}

void *cotton_runtime::worker_routine(void *arg)
{
    pthread_setspecific(index_key, arg); // set the index value for the current thread
    while (!shutdown)
    {
        cotton_runtime::find_and_execute_task();
    }
    return NULL;
}

void cotton_runtime::find_and_execute_task()
{
   int thread_no = get_index();               // get the index value for current thread
   task* t = task_queue[thread_no].pop();
    if (!t)    // if current thread's queue is empty 
    {
        int steal_thread = thread_no;                   
        while (steal_thread == thread_no)               //find another queue randomly
            steal_thread = rand() % no_of_threads;
        t = task_queue[steal_thread].steal();           //else steal task from this queue
        if(t)
        {
            t->lambda();                               //execute task
            delete t;
            pthread_mutex_lock(&finish_mutex);
            finish_counter--;                          //decrement fininsh counter at end of task execution
            pthread_mutex_unlock(&finish_mutex);
        }
    }
    else
    {
        t->lambda();                                  //execute task
        delete t;
        pthread_mutex_lock(&finish_mutex);
        finish_counter--;                            //decrement fininsh counter at end of task execution
        pthread_mutex_unlock(&finish_mutex);
    }

}

int cotton_runtime::get_no_of_workers() // get the number of workers from environment variable
{
    std::string key = "COTTON_WORKERS";
    char *env_var = getenv(key.c_str());
    if (env_var == NULL)
    {
        return 1; // default workers are 1
    }
    else
        return atoi(env_var); // return the environment variable value
}


cotton_runtime::deque::deque()
{
    head = 0;
    tail = 0;
    split=0;
    allstolen=true;
    splitreq=true;
}


bool cotton_runtime::deque::push(task *t)
{
    if(head==QUEUE_SIZE) return false;    //if deque is full return false
    tasks[head]=t;                                   //push task to deque
    head = head + 1;
    if (allstolen)             //if this is the first task make it shared
    {
        pthread_mutex_lock(&split_tail);
        tail=head-1;
        split = head;
        pthread_mutex_unlock(&split_tail);
        allstolen = false;
        if (splitreq) splitreq = false;
        
    }
    else if (splitreq) //if shared portion empty, grow the shared portion
    {
        grow_shared();
    }
    return true;
    
}
cotton_runtime::task *cotton_runtime::deque::steal()
{
    if(allstolen) return NULL;  // return if deque is empty
    task* t=NULL;
    pthread_mutex_lock(&split_tail); 
    if(tail<split)             //if tasks available in shared portion
    {
        t=tasks[tail];        //steal the task
        tail++;
    }
    else if(!splitreq)
    {
        splitreq=true;       //else set the splitreq flag to signal the owner
    }
    pthread_mutex_unlock(&split_tail);
    return t;
}

cotton_runtime::task * cotton_runtime::deque::pop()
{

   task* t=NULL;
   if((head==0) || allstolen) return NULL; // if deque empty return 
   if(split==head)                        // private portion is empty
    {
        shrink_shared();
    }
   if(split<head)                       //private portion has tasks
    {
        t=tasks[head-1];               //pop tasks
        head--;
    }
   return t;
}

void cotton_runtime::deque::shrink_shared()
{
    int prev_split = split;
    pthread_mutex_lock(&split_tail);
    split = (split+tail)/2;
    pthread_mutex_unlock(&split_tail);
    if(split>=prev_split)
    {
        allstolen=true;
    }
}
void cotton_runtime::deque::grow_shared()
{
    // pthread_mutex_lock(&split_tail);
    split =  (split +head+1)/2;
    // pthread_mutex_unlock(&split_tail);
    splitreq = false;
}