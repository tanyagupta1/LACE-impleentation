#include "cotton.h"
#include "cotton-runtime.h"

using namespace std;

const int no_of_threads = cotton_runtime::get_no_of_workers();
pthread_t *threads = new pthread_t[no_of_threads - 1];
volatile bool shutdown = false;
volatile int finish_counter = 0;
cotton_runtime::circular_queue *task_queue = new cotton_runtime::circular_queue[no_of_threads]; // global task queue
pthread_mutex_t *queue_mutexes = new pthread_mutex_t[no_of_threads];
pthread_mutex_t finish_mutex = PTHREAD_MUTEX_INITIALIZER;
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
    pthread_mutex_lock(&queue_mutexes[thread_no]); // lock the queue for current thread

    if (task_queue[thread_no].is_empty())    // if current thread's queue is empty 
    {
        pthread_mutex_unlock(&queue_mutexes[thread_no]);  //release the lock

        int steal_thread = thread_no;                   
        while (steal_thread == thread_no)               //find another queue randomly
            steal_thread = rand() % no_of_threads;
        pthread_mutex_lock(&queue_mutexes[steal_thread]); //get that queue's lock
        task *t = task_queue[steal_thread].steal();             //else steal task from this queue
        pthread_mutex_unlock(&queue_mutexes[steal_thread]);     //release lock
        if(t)
        {
        t->lambda();                                            //execute task
        delete t;
        pthread_mutex_lock(&finish_mutex);
        finish_counter--;                                       //decrement fininsh counter at end of task execution
        pthread_mutex_unlock(&finish_mutex);
        }
        return;
    }

    task *t = task_queue[thread_no].pop();           // else dequeue and get a task
    pthread_mutex_unlock(&queue_mutexes[thread_no]); // unlock queue after dequeue
    t->lambda();                                     // execute the task
    delete t;                                        // free memory of task after execution
    pthread_mutex_lock(&finish_mutex);
    finish_counter--; // take lock on finish counter and decrement it
    pthread_mutex_unlock(&finish_mutex);

    // cout<<get_index()<<' '<<pthread_self()<<'\n';
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
// implementation of the global fifo queue,using a struct array
cotton_runtime::circular_queue::circular_queue()
{
    front = -1;
    rear = -1;
}

bool cotton_runtime::circular_queue::is_full()
{
    return (((front == 0) && (rear == QUEUE_SIZE-1))||(front == rear+1));
}

int cotton_runtime::circular_queue::get_count()
{
    if(front==-1) return 0;
    else if(rear>=front) return (rear-front+1);
    else return (QUEUE_SIZE-(front-rear-1));
}
bool cotton_runtime::circular_queue::is_empty()
{
    if (front == -1) return true;
    return false;
}

bool cotton_runtime::circular_queue::push(task *t)
{
    if (is_full()) return false; // if queue is full, enqueue unsuccessful
    if(front==-1) tasks[0] = t; // push the task to at the rear location
    else tasks[(rear + 1) % QUEUE_SIZE] = t;
    if (front == -1) // this means the queue was empty till now
    {
        front = 0;
        rear = 0;
    }
    else // if queue wasn't empty circular increment the rear counter
    {
        rear = (rear + 1) % QUEUE_SIZE;
    }
    return true;     // enqueue was succesful
}

cotton_runtime::task *cotton_runtime::circular_queue::steal()
{
    if (get_count()<2) return NULL;
    task *t = tasks[front]; // taking the front task
    front = (front + 1) % QUEUE_SIZE;
    return t;
}
cotton_runtime::task *cotton_runtime::circular_queue::pop()
{
    if (is_empty()) return NULL;
    task *t = tasks[rear];      //take the the task from the end for the cur thread
    if (front == rear)          //if queue is now empty
    {
        front = -1;
        rear = -1;
    }
    else if (rear == 0)  rear = QUEUE_SIZE - 1;  //if rear was pointing to 0, it must point to the the last element
    else rear--;
    return t;
}
