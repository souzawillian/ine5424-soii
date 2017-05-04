#include <iostream>

template<int n, int m>
struct gcd
{
    enum { RET = gcd<m, n % m>::RET };
};

template<int n>
struct gcd<n, 0>
{
    enum { RET = n };
};

int main() {
    std::cout << gcd<48, 180>::RET << "\n";
}