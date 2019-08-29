# Qkchash Stratum CPU Miner for macOS



## Summary

This is an Qkchash CPU mining worker.

I forked the [Ethminer for mac](https://github.com/ArtSabintsev/Ethminer-for-macOS.git) and modified the code, which can support the QuarkChain Qkchash mining. 
With this modified ethminer, you can mine the shards, which relies on an Qkchash Proof of Work.
Noted that this version only support the Stratum mode, which can directly connect to Qkchash_pool.

## Features

* Support Stratum mode
* Support getWork mode
* Support CPU mining for Qkchash
* Support failover pool
* Support qkchash llrb version

## Pre-Installation Instructions
Download and install the latest version of
- Xcode 
    - Xcode 7.3 Command Line Tools at https://download.developer.apple.com/Developer_Tools/Command_Line_Tools_OS_X_10.11_for_Xcode_7.3/Command_Line_Tools_OS_X_10.11_for_Xcode_7.3.dmg
    - Afterwards run `sudo xcode-select --switch /Library/Developer/CommandLineTools`
    - For everything else - Xcode 8.3.3 or newer at https://developer.apple.com/xcode
- Homebrew from https://brew.sh

## Installation Instructions

1. Download this branch
```
git clone https://github.com/QuarkChain/mining.git
cd mining
git checkout Qkchash_CPU_Stratum_Miner
```

2. Enter the downloaded folder and create and enter the `build` directory.
```
mkdir build; cd build
```

3. Run _one of the the following_ `cmake` calls on the root directory from within the `build` directory.
  - This will also install all the homebrew dependencies that you'll need.
```
cmake -DETHSTRATUM=1 --DETHASHCL=OFF ..
```

4. Run `make` next:
```
make -j8
```

5. Afterwards, run `cmake` again, but on your current directory, the build directory.
```
cmake --build .
```

## Examples connecting to Qkchash_pool


please use,
```shell
cd ethminer
# IMPORTANT: always update coinbase address for mining
# use the following command 
./ethminer --farm http://$CLUSTER_IP:38391 --coinbase $COINBASE_ADDRESS -t <n> --shard-id $SHARD_ID

- `CLUSTER_IP` defines the IP for the Quarkchain cluster. If you want to try the one button quick mining, you can use fullnode.quarkchain.io or fullnode2.quarkchain.io.
- `--shard-id` defines one specify shard to mine. shard id 60001 and 70001 are Qkchash.
- `--coinbase` defines your mining coinbase address. Please use 20 bytes address generated the same way as an Ethereum address.
- `-t` limits number of CPU miners to n 

One example for chain 7, shard 0 is
./ethminer --farm fullnode.quarkchain.io:38391 --coinbase 0x1234000000000000000000000000000000000000 -t 4 --shard-id 70001
```
Note:
Please use the 20 bytes address with the first two characters "0x" from the coinbase address.
Please specify the shard id which you want to mine.


