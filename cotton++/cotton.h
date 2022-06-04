#include <pthread.h>
#include <functional>
#include <iostream>
#include <string.h>
#include <stdexcept>

namespace cotton
{
    void init_runtime();
    void async(std::function<void()> &&lambda);
    void start_finish();
    void end_finish();
    void finalize_runtime();
}