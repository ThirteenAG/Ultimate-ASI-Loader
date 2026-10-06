// This exact translation unit is compiled by MSVC and cxxsnippets.
#include <cstdint>
extern "C"
{
    int corpus_integer(int a, int b)
    {
        return ((a + b) * 3 - (a - b) * 2) ^ (a & b);
    }
    int corpus_control(int n)
    {
        int result = 0;
        for (int i = 0; i < n; ++i)
        {
            switch (i % 4)
            {
            case 0:
                result += i;
                break;
            case 1:
                result -= 2;
                break;
            default:
                result += 3;
            }
            if (i == 9)
                break;
        }
        return result;
    }
    uint64_t corpus_wide(uint64_t a, uint64_t b)
    {
        return (a * b + 123) / ((b & 15) + 1) ^ (a >> 9);
    }
    double corpus_float(double a, double b)
    {
        return (a + b) * 2.25 - a / (b + 1.0);
    }
    int corpus_array(int x)
    {
        int values[5] = {2, 3, 5, 7, 11};
        int *p = values;
        int total = 0;
        for (int i = 0; i < 5; ++i)
            total += *(p + i);
        return total + values[x % 5];
    }
}
