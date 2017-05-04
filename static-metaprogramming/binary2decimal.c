#include <iostream>

template<int i, int n>
struct binToDecimal
{
    // (n % 10)x2^i + ... + (n % 10)x2^1 + (n % 10)x2^0
    enum { RET = binToDecimal<i+1, n/10>::RET + (n % 10) * (1 << i) };
};

template<int i>
struct binToDecimal<i, 0>
{
    enum { RET = 0 };
};

template<int n>
struct bin2decimal
{
    enum { RET = binToDecimal<0, n>::RET };
};

// int binary_to_decimal(int num)
// {
//     int res = 0;
//     for(int i = 0; num > 0; ++i)
//     {
//         res += ((num % 10) << i);
//         num = num / 10;
//     }

//     return res;
// }

int main() {
    // std::cout << binary_to_decimal(1100);
    std::cout << bin2decimal<1010>::RET << "\n";
    std::cout << bin2decimal<1100>::RET << "\n";
}