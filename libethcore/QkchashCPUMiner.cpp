/*
	This file is part of cpp-ethereum.

	cpp-ethereum is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	cpp-ethereum is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with cpp-ethereum.  If not, see <http://www.gnu.org/licenses/>.
*/
/** @file QkchashCPUMiner.cpp
 * @author Gav Wood <i@gavwood.com>
 * @date 2014
 *
 * Determines the PoW algorithm.
 */

#include "QkchashCPUMiner.h"
#include <thread>
#include <chrono>
#include <boost/algorithm/string.hpp>
#include <random>

#include <climits>
#include <cstdint>
#include <cstring>

#include <array>
#include <chrono>
#include <iostream>
#include <random>
#include <set>


#include <stdint.h>


//#include "libethash/sha3.h"
#include <cryptopp/keccak.h>
#include <cryptopp/sha3.h>

//#include "qkchash/qkchash.h"


#include "qkchash_llrb.h"
#include "util.h"



#if ETH_CPUID || !ETH_TRUE
#define HAVE_STDINT_H
#include <libcpuid/libcpuid.h>
#endif
using namespace std;
using namespace dev;
using namespace eth;



namespace org {
namespace quarkchain {

const uint32_t FNV_PRIME_32 = 0x01000193;
const uint64_t FNV_PRIME_64 = 0x100000001b3ULL;
const uint32_t ACCESS_ROUND = 64;
const uint32_t INIT_SET_ENTRIES = 1024 * 64;
const uint32_t CACHE_ENTRY_CNT = 1024 * 64;



/*
 * A simplified version of generating initial set.
 * A more secure way is to use the cache generation in eth.
 */

void uint32_to_arrayUint8 (uint32_t v, std::array<uint8_t, 4>& b) {
    b[3] = (uint8_t)(v);
	b[2] = (uint8_t)(v >> 8);
	b[1] = (uint8_t)(v >> 16);
	b[0] = (uint8_t)(v >> 24);
}

void uint64_to_arrayUint8 (uint64_t v, std::array<uint8_t, 8>& b) {
    b[7] = (uint8_t)(v);
	b[6] = (uint8_t)(v >> 8);
	b[5] = (uint8_t)(v >> 16);
	b[4] = (uint8_t)(v >> 24);
	b[3] = (uint8_t)(v >> 32);
	b[2] = (uint8_t)(v >> 40);
	b[1] = (uint8_t)(v >> 48);
	b[0] = (uint8_t)(v >> 56);
}


void PutUint64(uint64_t v, byte* b)  {
    // early bounds check to guarantee safety of writes below
	(*(b + 0)) = (uint8_t)(v);
	(*(b + 1)) = (uint8_t)(v >> 8);
	(*(b + 2)) = (uint8_t)(v >> 16);
	(*(b + 3)) = (uint8_t)(v >> 24);
	(*(b + 4)) = (uint8_t)(v >> 32);
	(*(b + 5)) = (uint8_t)(v >> 40);
	(*(b + 6)) = (uint8_t)(v >> 48);
	(*(b + 7)) = (uint8_t)(v >> 56);
}



uint64_t array_to_uint64 (byte* seed, int offset) {
    return (uint64_t)(*(seed + offset + 0)) | (uint64_t)(*(seed + offset + 1))  << 8 | (uint64_t)(*(seed + offset + 2)) << 16 |
           (uint64_t)(*(seed + offset + 3)) << 24 | (uint64_t)(*(seed + offset + 4))  << 32 | (uint64_t)(*(seed + offset + 5)) << 40 |
           (uint64_t)(*(seed + offset + 6)) << 48 | (uint64_t)(*(seed + offset + 7))  << 56;
}



void generate_cache(std::set<uint64_t>& oset, std::vector<uint64_t>& ls, std::vector<uint8_t>& seed) {


    byte digest[64];
	std::array<uint8_t, 36> combine;
	for (int i = 0; i < seed.size(); ++i) {
			combine[i] = seed[i];
	} 
	//std::cout << "seed size:" << std::endl;
    //std::cout << seed.size() << std::endl;
    
    for (uint32_t i = 0; i < (uint32_t) (CACHE_ENTRY_CNT / 8); ++i) {
        std::array<uint8_t, 4> iBytes;

        uint32_to_arrayUint8(i, iBytes);

		for (int j = 0; j < 4; ++j) {
			combine[32 + j] = iBytes[j];
		}

		//CryptoPP::Keccak_512().CalculateDigest(digest, (const byte*) seed.data(), seed.size());

        CryptoPP::Keccak_512().CalculateDigest(digest, (const byte*) &combine, 36);
        
 
        for (int j = 0; j < 64; j += 8) {
            uint64_t ele = array_to_uint64(digest, j);
            if (oset.find(ele) == oset.end()) {
                oset.insert(ele);
                ls.push_back(ele);
            }

        }

        
    }


    std::sort(ls.begin(), ls.end());
    


    //std::cout << "Last element in the Dag: ";
	//for (int i = 0; i < 10; ++i) {
	//    std::cout << ls[i] << std::endl;
	//}

}



void qkc_hash_llrb(
        org::quarkchain::LLRB<uint64_t>& cache,
        std::array<uint64_t, 8>& seed,
        std::array<uint64_t, 4>& result) {
    std::array<uint64_t, 16> mix;
    for (uint32_t i = 0; i < mix.size(); i++) {
        mix[i] = seed[i % seed.size()];
    }

    for (uint32_t i = 0; i < ACCESS_ROUND; i ++) {
        std::array<uint64_t, 16> new_data;
        uint64_t p = fnv64(i ^ seed[0], mix[i % mix.size()]);
        for (uint32_t j = 0; j < mix.size(); j++) {
            // Find the pth element and remove it
            uint32_t idx = p % cache.size();
            new_data[j] = cache.eraseByOrder(idx);

            // Generate random data and insert it
            // if the vector doesn't contain it.
            p = fnv64(p, new_data[j]);
            cache.insert(p);

            // Find the next element index (ordered)
            p = fnv64(p, new_data[j]);
        }

        for (uint32_t j = 0; j < mix.size(); j++) {
            mix[j] = fnv64(mix[j], new_data[j]);
        }
    }

    /*
     * Compress
     */
    for (uint32_t i = 0; i < result.size(); i++) {
        uint32_t j = i * 4;
        result[i] = fnv64(fnv64(fnv64(mix[j], mix[j + 1]), mix[j + 2]), mix[j + 3]);
    }
}


} // quarkchain
} // org


extern "C" void *cache_create(uint64_t *cache_ptr,
                              uint32_t cache_size) {
    void *arena0 = malloc(org::quarkchain::INIT_SET_ENTRIES *
                          org::quarkchain::LLRB<uint64_t>::getNodeSize());
    org::quarkchain::LLRB<uint64_t>* tree0 = new org::quarkchain::LLRB<uint64_t>(
        (uintptr_t)arena0,
        org::quarkchain::INIT_SET_ENTRIES *
            org::quarkchain::LLRB<uint64_t>::getNodeSize());
    for (uint32_t i = 0; i < cache_size; i++) {
        tree0->insert(cache_ptr[i]);
    }
    return tree0;
}

extern "C" void *cache_copy(void *ptr) {
    return ptr;
}

extern "C" void cache_destroy(void *ptr) {
    org::quarkchain::LLRB<uint64_t>* tree0=(org::quarkchain::LLRB<uint64_t>*) ptr;
	free(tree0->getArenaBase());	
	delete tree0;
	  
}

extern "C" void qkc_hash(void *cache_ptr,
                         uint64_t* seed_ptr,
                         uint64_t* result_ptr) {
    org::quarkchain::LLRB<uint64_t>* tree0=(org::quarkchain::LLRB<uint64_t>*) cache_ptr;
    void *arena1 = malloc(org::quarkchain::INIT_SET_ENTRIES *
                          org::quarkchain::LLRB<uint64_t>::getNodeSize());
    org::quarkchain::LLRB<uint64_t> tree1 = tree0->copy((uintptr_t)arena1);

    std::array<uint64_t, 8> seed;
    std::array<uint64_t, 4> result;
    std::copy(seed_ptr, seed_ptr + seed.size(), seed.begin());


    org::quarkchain::qkc_hash_llrb(tree1, seed, result);

    std::copy(result.begin(), result.end(), result_ptr);
    free(arena1);
}

void display(byte val)
{
    const char f = cout.fill('0');
    const streamsize w = cout.width(2);
    cout << std::hex << (unsigned int)(val);
    cout.width(w);
    cout.fill(f);
}



//typedef struct ethash_h256 { uint8_t b[32]; } ethash_h256_t;

typedef struct qkchash_return_value {
	ethash_h256_t result;
	ethash_h256_t mix_hash;
    uint64_t* cache;
	bool success;
	void *cache_ptr;
} qkchash_return_value_t;


// main qkchash compute function
qkchash_return_value_t  qkchash_full_compute(uint64_t* cache, ethash_h256_t header_hash, uint64_t tryNonce, bool first, void *cache_ptr) {
    

    qkchash_return_value_t ret;
    ret.cache = cache;
    std::vector<uint8_t> header_bytes;

	for (int i = 0; i < 32; ++i) {
		header_bytes.push_back(header_hash.b[i]);
	}


	//std::cout << "header hash" << std::endl;
    //std::for_each(header_bytes.b, header_bytes.b + 32, display);
    //std::cout << "header hash result end" << std::endl;


    std::array<uint8_t, 8> tryNonce_bytes;
    org::quarkchain::uint64_to_arrayUint8(tryNonce, tryNonce_bytes);

    //std::cout << "tryNonce" << std::endl;
    //std::for_each(&tryNonce_bytes[0], &tryNonce_bytes[0] + 8, display);
    //std::cout << "tryNonce end" << std::endl;


    for (int i = 7; i >= 0; --i) {
        header_bytes.push_back(tryNonce_bytes[i]);
    }
    byte final_result[64];


    CryptoPP::Keccak_512().CalculateDigest(final_result, (const byte*) header_bytes.data(), header_bytes.size());



    std::array<uint64_t, 8> seedArray;
	for (int i = 0; i < 8; i++) {
		seedArray[i] = org::quarkchain::array_to_uint64(&final_result[i * 8], 0);
	}

    std::array<uint64_t, 4> result;
    uint64_t* pointer_result;
    pointer_result = &result[0];

	if (first) {
		ret.cache_ptr =  cache_create(cache, 65536);
		qkc_hash(ret.cache_ptr, &seedArray[0], pointer_result);
	} else {
		qkc_hash(cache_copy(cache_ptr), &seedArray[0], pointer_result);
	}
    
    

    //byte mixHash[32];

    for (int i = 0; i < 4; ++i) {
        org::quarkchain::PutUint64( (*(pointer_result + i)), &ret.mix_hash.b[i * 8]);
    }
 
    //std::cout << "mixHash result" << std::endl;
    //std::for_each(ret.mix_hash.b, ret.mix_hash.b + 32, display);
    //std::cout << "mixHash result end" << std::endl;

    //byte hash_result[SHA3_256::DIGESTSIZE];
   
    byte mix_template_result[64 + 32];
    for (int i = 0; i < 64; ++i) {
        mix_template_result[i] = final_result[i];
    }

    for (int i = 64; i < 96; ++i) {
        mix_template_result[i] = ret.mix_hash.b[i - 64];
    }

    CryptoPP::Keccak_256().CalculateDigest(ret.result.b, (const byte*) mix_template_result, 96);

    //std::cout << "hash_result: ";
    //std::for_each(ret.result.b, ret.result.b + 32, display);
    //std::cout << endl;

	ret.success = true;

    return ret;
}



unsigned QkchashCPUMiner::s_numInstances = 0;


#if ETH_CPUID || !ETH_TRUE
static string jsonEncode(map<string, string> const& _m)
{
	string ret = "{";

	for (auto const& i: _m)
	{
		string k = boost::replace_all_copy(boost::replace_all_copy(i.first, "\\", "\\\\"), "'", "\\'");
		string v = boost::replace_all_copy(boost::replace_all_copy(i.second, "\\", "\\\\"), "'", "\\'");
		if (ret.size() > 1)
			ret += ", ";
		ret += "\"" + k + "\":\"" + v + "\"";
	}

	return ret + "}";
}
#endif

QkchashCPUMiner::QkchashCPUMiner(GenericMiner<EthashProofOfWork>::ConstructionInfo const& _ci):
	GenericMiner<EthashProofOfWork>(_ci), Worker("miner" + toString(index()))
{
}

QkchashCPUMiner::~QkchashCPUMiner()
{
}

void QkchashCPUMiner::kickOff()
{
	stopWorking();
	startWorking();
}

void QkchashCPUMiner::pause()
{
	stopWorking();
}

/*
void QkchashCPUMiner::workLoop()
{
	auto tid = std::this_thread::get_id();
	static std::mt19937_64 s_eng((time(0) + std::hash<decltype(tid)>()(tid)));

	uint64_t tryNonce = s_eng();
	ethash_return_value ethashReturn;

	WorkPackage w = work();

	EthashAux::FullType dag;
	//cnote << "workLoop start";
	while (!shouldStop() && !dag)
	{
		while (!shouldStop() && EthashAux::computeFull(w.seedHash, true) != 100)
			this_thread::sleep_for(chrono::milliseconds(500));
		dag = EthashAux::full(w.seedHash, false);
	}

	h256 boundary = w.boundary;
	unsigned hashCount = 1;
	for (; !shouldStop(); tryNonce++, hashCount++)
	{
		ethashReturn = ethash_full_compute(dag->full, *(ethash_h256_t*)w.headerHash.data(), tryNonce);
		h256 value = h256((uint8_t*)&ethashReturn.result, h256::ConstructFromPointer);
	//	cnote << (value <= boundary) << "validation";
		if (value <= boundary && submitProof(EthashProofOfWork::Solution{(h64)(u64)tryNonce, h256((uint8_t*)&ethashReturn.mix_hash, h256::ConstructFromPointer)})) {
			cnote << value << "value";
			cnote << boundary << "boundary";
			break;
		}
		if (!(hashCount % 100))
			accumulateHashes(100);
	}
}
*/




void QkchashCPUMiner::workLoop()
{
	auto tid = std::this_thread::get_id();
	static std::mt19937_64 s_eng((time(0) + std::hash<decltype(tid)>()(tid)));

	uint64_t tryNonce = s_eng();
	qkchash_return_value qkchashReturn;

	WorkPackage w = work();

    std::set<uint64_t> oset;
    std::vector<uint64_t> slist;
    std::vector<uint8_t> seed;
    
	ethash_h256_t seedHash = *(ethash_h256_t*)w.seedHash.data();
    

	for (int i = 0; i < 32; ++i) {
		seed.push_back(seedHash.b[i]);
	}



    //std::cout << "Start" << std::endl;
	while(!shouldStop() && slist.empty()) {
    	org::quarkchain::generate_cache(oset, slist, seed);
	}


   	h256 boundary = w.boundary;
	unsigned hashCount = 1;

    bool first = true;

	void *cache_ptr = nullptr;

	for (; !shouldStop(); tryNonce++, hashCount++) {

    	qkchashReturn = qkchash_full_compute(&slist[0], *(ethash_h256_t*)w.headerHash.data(), tryNonce, first, cache_ptr);

		cache_ptr = qkchashReturn.cache_ptr;

		first = false;
        
		h256 value = h256((uint8_t*)&qkchashReturn.result, h256::ConstructFromPointer);
		//	cnote << (value <= boundary) << "validation";
		if (value <= boundary ) {
			//cnote << value << "value";
			//cnote << boundary << "boundary";
             
			bool submit_result = submitProof(EthashProofOfWork::Solution{(h64)(u64)tryNonce, h256((uint8_t*)&qkchashReturn.mix_hash, h256::ConstructFromPointer)});
			//cnote << submit_result << "submit_result";
			break;
		}
		if (!(hashCount % 100))
			accumulateHashes(100);

	}

	cache_destroy(cache_ptr);
    
}


std::string QkchashCPUMiner::platformInfo()
{
	string baseline = toString(std::thread::hardware_concurrency()) + "-thread CPU";
#if ETH_CPUID || !ETH_TRUE
	if (!cpuid_present())
		return baseline;
	struct cpu_raw_data_t raw;
	struct cpu_id_t data;
	if (cpuid_get_raw_data(&raw) < 0)
		return baseline;
	if (cpu_identify(&raw, &data) < 0)
		return baseline;
	map<string, string> m;
	m["vendor"] = data.vendor_str;
	m["codename"] = data.cpu_codename;
	m["brand"] = data.brand_str;
	m["L1 cache"] = toString(data.l1_data_cache);
	m["L2 cache"] = toString(data.l2_cache);
	m["L3 cache"] = toString(data.l3_cache);
	m["cores"] = toString(data.num_cores);
	m["threads"] = toString(data.num_logical_cpus);
	m["clocknominal"] = toString(cpu_clock_by_os());
	m["clocktested"] = toString(cpu_clock_measure(200, 0));
	/*
	printf("  MMX         : %s\n", data.flags[CPU_FEATURE_MMX] ? "present" : "absent");
	printf("  MMX-extended: %s\n", data.flags[CPU_FEATURE_MMXEXT] ? "present" : "absent");
	printf("  SSE         : %s\n", data.flags[CPU_FEATURE_SSE] ? "present" : "absent");
	printf("  SSE2        : %s\n", data.flags[CPU_FEATURE_SSE2] ? "present" : "absent");
	printf("  3DNow!      : %s\n", data.flags[CPU_FEATURE_3DNOW] ? "present" : "absent");
	*/
	return jsonEncode(m);
#else
	return baseline;
#endif
}
