# run test

## riscv-tests

### check

```bash
$ ./main.py list gem5 --suites

$ ./main.py list --build-targets --exclude-tags VEGA_X86 -q
```

### 运行所有测试

```bash
$ cd tests
$ ./main.py run gem5/asmtest

$ ./main.py run --skip-build gem5/asmtest

$ ./main.py run --skip-build --exclude-tags gcn_gpu gem5/asmtest

$ ./main.py run --skip-build gem5/suite_tests

$ ./main.py run gem5/suite_tests
```

###  运行特定测试

```bash
$ ./build/RISCV/gem5.opt tests/gem5/asmtest/configs/riscv_asmtest.py <test_name> <cpu_type>
```

例如：
```bash
./build/RISCV/gem5.opt tests/gem5/asmtest/configs/riscv_asmtest.py rv64ui-ps-add atomic
```


```bash
$ ./main.py run --skip-build --bin-path=/home/dongguangda/workspace/project/riscv/riscv-tests/install/share/riscv-tests/isa -j4 gem5/asmtest
```

###  使用本地resources

```bash
$ GEM5_CONFIG=src/python/gem5_config.json ./build/RISCV/gem5.debug tests/gem5/asmtest/configs/riscv_asmtest.py rv64samt-ps-sysclone_d atomic
$ GEM5_CONFIG=src/python/gem5_config.json ./build/RISCV/gem5.debug tests/gem5/asmtest/configs/riscv_asmtest.py hello atomic
$ GEM5_CONFIG=src/python/gem5_config.json ./build/RISCV/gem5.debug --debug-file=test-run.log --debug-flags=Commit,O3CPU,Fetch tests/gem5/asmtest/configs/riscv_asmtest.py hello o3

$ cd tests
$ GEM5_CONFIG=../src/python/gem5_config.json ./main.py run --skip-build gem5/asmtest
```

###  指定bin path

```bash
$ ./main.py run --skip-build --bin-path=$RISCV/target/share/riscv-tests/isa -j4 gem5/asmtest
```

## o3 example

```bash
$ ./build/RISCV/gem5.debug configs/example/gem5_library/riscv-rvv-example.py -c 2 -v 256 -e 32 rvv-sgemm

$ ./build/RISCV/gem5.debug --debug-file=test-run-Fetch.log    --debug-flags=Fetch configs/example/gem5_library/riscv-rvv-example.py    -c 2 -v 256 -e 32 rvv-sgemm
$ ./build/RISCV/gem5.debug --debug-file=test-run-Exec.log     --debug-flags=Exec configs/example/gem5_library/riscv-rvv-example.py     -c 2 -v 256 -e 32 rvv-sgemm
$ ./build/RISCV/gem5.debug --debug-file=test-run-O3CPUAll.log --debug-flags=O3CPUAll configs/example/gem5_library/riscv-rvv-example.py -c 2 -v 256 -e 32 rvv-sgemm
$ ./build/RISCV/gem5.debug --debug-file=test-run-CacheAll.log --debug-flags=CacheAll configs/example/gem5_library/riscv-rvv-example.py -c 2 -v 256 -e 32 rvv-sgemm
$ ./build/RISCV/gem5.debug --debug-file=test-run-IEW.log      --debug-flags=IEW configs/example/gem5_library/riscv-rvv-example.py      -c 2 -v 256 -e 32 rvv-sgemm
```

## benchmark

### riscv fs

```bash
$ ./build/RISCV/gem5.debug configs/example/gem5_library/riscvmatched-hello.py
$ ./build/RISCV/gem5.debug configs/example/gem5_library/riscvmatched-fs.py
$ ./build/RISCV/gem5.debug configs/example/gem5_library/riscvmatched-microbenchmark-suite.py
```

### tiny

```bash
$ ./build/RISCV/gem5.debug configs/leap5/simple-riscv.py --binary=../../../riscv/riscv-tests/benchmarks/dhrystone.riscv
$ ./build/RISCV/gem5.debug configs/leap5/simple-riscv.py --binary=/home/dongguangda/workspace/project/benchmark/coremark-workload/riscv-coremark/coremark.riscv
```

### spec

```bash
# spec cpu2006
build/X86/gem5.opt \
  configs/example/gem5_library/x86-spec-cpu2006-benchmarks.py \
  --image <path_to_built_spec-2006_disk_image> \
  --partition <root_partition_to_mount> \
  --benchmark <benchmark_program> \
  --size <workload_size>

./build/ALL/gem5.opt configs/example/gem5_library/x86-spec-cpu2006-benchmarks.py --image /home/samantha/.cache/gem5/x86-ubuntu-24.04-img-4.0.0 --benchmark 400.perlbench --size test

./build/RISCV/gem5.opt configs/example/gem5_library/checkpoint/riscv-hello-save-checkpoint.py

build/RISCV/gem5.debug \
  configs/example/riscv/fs_linux.py \
  --kernel=/home/samantha/.cache/gem5/riscv-linux-6.5.5-kernel \
  --bootloader=/home/samantha/.cache/gem5/riscv-bootloader-opensbi-1.3.1 \
  --disk-image=/home/samantha/.cache/gem5/riscv-ubuntu-20.04-img
```


