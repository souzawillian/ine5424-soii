#include <iostream>

template<unsigned n>
struct fibonacci
{
    enum { RET = fibonacci<n-1>::RET + fibonacci<n-2>::RET };
};

template<>
struct fibonacci<1>
{
    enum { RET = 1 };
};

template<>
struct fibonacci<0>
{
    enum { RET = 0 };
};



int main() {
    std::cout << fibonacci<13>::RET << "\n";
    std::cout << fibonacci<20>::RET << "\n";
}