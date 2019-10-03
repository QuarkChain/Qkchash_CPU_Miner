#include <climits>
#include <cstdlib>

#include <chrono>
#include <random>
#include <set>

#include <ext/pb_ds/assoc_container.hpp>
#include <ext/pb_ds/tree_policy.hpp>

#include "qkchash_llrb.h"

using namespace __gnu_pbds;

typedef
tree<
    uint64_t,
    null_type,
    std::less<uint64_t>,
    rb_tree_tag,
    tree_order_statistics_node_update>
ordered_set_t;

class ClockPrinter {
public:
    ClockPrinter(std::string prefix)
    : prefix_(prefix),
      startTime_(std::chrono::steady_clock::now()) {}

    ~ClockPrinter() {
        auto usedTime = std::chrono::steady_clock::now() - startTime_;
        std::cout << prefix_
                  << " used_time: "
                  << std::chrono::duration<double, std::milli>(usedTime).count()
                  << std::endl;
    }

private:
    std::string prefix_;
    std::chrono::time_point<std::chrono::steady_clock> startTime_;
};

void comp_perf() {
    std::cout << std::endl;
    std::cout << "================" << std::endl;
    std::cout << "Performance test" << std::endl;

    uint32_t key_size = 64 * 1024;

    std::uniform_int_distribution<uint64_t>
        dist64(0, std::numeric_limits<uint64_t>::max());
    std::default_random_engine generator(
        std::chrono::steady_clock::now().time_since_epoch().count());
    std::vector<uint64_t> data_to_insert;
    for (uint32_t i = 0; i < key_size; i++) {
        data_to_insert.push_back(dist64(generator));
    }

    ordered_set_t set0;
    {
        ClockPrinter printer("insert");
        for (uint64_t v : data_to_insert) {
            set0.insert(v);
        }
    }

    ordered_set_t set1;
    {
        ClockPrinter printer("copy");
        set1 = set0;
    }

    {
        ClockPrinter printer("erase/insert");
        for (uint32_t i = 0; i < key_size; i++) {
            uint64_t p = dist64(generator) % set1.size();
            set1.erase(data_to_insert[p]);
            data_to_insert[p] = dist64(generator);
            set1.insert(data_to_insert[p]);
        }
    }

    void *arena0 = malloc(key_size * org::quarkchain::LLRB<uint64_t>::getNodeSize());
    void *arena1 = malloc(key_size * org::quarkchain::LLRB<uint64_t>::getNodeSize());
    org::quarkchain::LLRB<uint64_t> tree0(
        (uintptr_t)arena0, key_size * org::quarkchain::LLRB<uint64_t>::getNodeSize());

    std::cout << "Node size " << org::quarkchain::LLRB<uint64_t>::getNodeSize() << std::endl;
    {
        ClockPrinter printer("insert");
        for (uint64_t v : data_to_insert) {
            tree0.insert(v);
        }
    }

    {
        ClockPrinter printer("copy");
        org::quarkchain::LLRB<uint64_t> tree1 = tree0.copy((uintptr_t)arena1);
    }

    {
        ClockPrinter printer("erase/insert");
        for (uint32_t i = 0; i < key_size; i++) {
            uint64_t p = dist64(generator) % tree0.size();
            tree0.erase(data_to_insert[p]);
            data_to_insert[p] = dist64(generator);
            tree0.insert(data_to_insert[p]);
        }
    }

    {
        ClockPrinter printer("hash");
        std::cout << tree0.hash() << std::endl;
    }

    std::cout << "Passed" << std::endl;
}

void comp_perf_order() {
    std::cout << std::endl;
    std::cout << "======================" << std::endl;
    std::cout << "Order performance test" << std::endl;
    uint32_t key_size = 64 * 1024;

    std::uniform_int_distribution<uint64_t>
        dist64(0, std::numeric_limits<uint64_t>::max());
    std::default_random_engine generator(
        std::chrono::steady_clock::now().time_since_epoch().count());
    std::vector<uint64_t> data_to_insert;
    for (uint32_t i = 0; i < key_size; i++) {
        data_to_insert.push_back(dist64(generator));
    }

    ordered_set_t set0;
    {
        ClockPrinter printer("insert");
        for (uint64_t v : data_to_insert) {
            set0.insert(v);
        }
    }

    {
        ClockPrinter printer("copy and destroy");
        ordered_set_t copy_set1 = set0;
    }


    ordered_set_t set1(set0);
    {
        ClockPrinter printer("erase/insert");
        for (uint32_t i = 0; i < key_size; i++) {
            uint64_t p = dist64(generator) % set1.size();
            auto it = set1.find_by_order(p);
            set1.erase(it);
            uint64_t nd = dist64(generator);
            set1.insert(nd);
        }
    }

    void *arena0 = malloc(key_size * org::quarkchain::LLRB<uint64_t>::getNodeSize());
    void *arena1 = malloc(key_size * org::quarkchain::LLRB<uint64_t>::getNodeSize());
    org::quarkchain::LLRB<uint64_t> tree0(
        (uintptr_t)arena0, key_size * org::quarkchain::LLRB<uint64_t>::getNodeSize());

    std::cout << "Node size " << org::quarkchain::LLRB<uint64_t>::getNodeSize() << std::endl;
    {
        ClockPrinter printer("insert");
        for (uint64_t v : data_to_insert) {
            tree0.insert(v);
        }
    }

    {
        ClockPrinter printer("copy");
        org::quarkchain::LLRB<uint64_t> tree1 = tree0.copy((uintptr_t)arena1);
    }

    {
        ClockPrinter printer("erase/insert");
        for (uint32_t i = 0; i < key_size; i++) {
            uint64_t p = dist64(generator) % tree0.size();
            tree0.eraseByOrder(p);
            uint64_t nd = dist64(generator);
            tree0.insert(nd);
        }

        CHECK(tree0.check());
    }

    {
        ClockPrinter printer("hash");
        std::cout << tree0.hash() << std::endl;
    }

    std::cout << "Passed" << std::endl;
}

