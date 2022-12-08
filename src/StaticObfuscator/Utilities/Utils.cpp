#include "Utils.h"



size_t utils::rand(size_t from, size_t to)
{
    /*https://en.cppreference.com/w/cpp/numeric/random */
        // Seed with a real random value, if available
    std::random_device r;

    // Choose a random mean between 1 and 6
    std::default_random_engine e1(r());
    std::uniform_int_distribution<int> uniform_dist(from, to);
    return  uniform_dist(e1);
}
