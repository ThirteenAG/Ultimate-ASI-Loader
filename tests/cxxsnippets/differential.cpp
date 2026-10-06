#include "cxxsnippets.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
extern "C"
{
    int corpus_integer(int, int);
    int corpus_control(int);
    uint64_t corpus_wide(uint64_t, uint64_t);
    double corpus_float(double, double);
    int corpus_array(int);
}
void Differential()
{
    std::vector<cxxsnippets::Diagnostic> errors;
    auto m = cxxsnippets::CompileFile(std::string(CXXSNIPPETS_TEST_ROOT) + "/corpus.cpp", {}, errors);
    for (auto &e : errors)
        std::fprintf(stderr, "%s\n", e.ToString().c_str());
    if (!m)
        std::exit(1);
    auto integer = (int (*)(int, int))m->Find("corpus_integer");
    auto control = (int (*)(int))m->Find("corpus_control");
    auto wide = (uint64_t (*)(uint64_t, uint64_t))m->Find("corpus_wide");
    auto floating = (double (*)(double, double))m->Find("corpus_float");
    auto array = (int (*)(int))m->Find("corpus_array");
    if (!integer || !control || !wide || !floating || !array)
        std::exit(1);
    uint64_t seed = 1234567;
    int checks = 0;
    for (int i = 0; i < 1000; ++i)
    {
        seed = seed * 6364136223846793005ULL + 1;
        int a = (int)(seed % 2000) - 1000, b = (int)((seed >> 32) % 2000) - 1000;
        bool ok = integer(a, b) == corpus_integer(a, b) && control(i % 30) == corpus_control(i % 30) &&
                  wide(seed, seed >> 19) == corpus_wide(seed, seed >> 19) &&
                  std::abs(floating(a, b + 1001.0) - corpus_float(a, b + 1001.0)) < 1e-9 && array(i) == corpus_array(i);
        if (!ok)
        {
            std::fprintf(stderr, "FAIL: differential input %d\n", i);
            std::exit(1);
        }
        checks += 5;
    }
    std::printf("PASS: %d MSVC differential comparisons\n", checks);
}
