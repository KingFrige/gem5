# run test

## benchmark

```bash
$ ./build/RISCV/gem5.debug configs/leap5/simple-riscv.py --binary=../../../riscv/riscv-tests/benchmarks/dhrystone.riscv
$ ./build/RISCV/gem5.debug configs/leap5/simple-riscv.py --binary=/home/dongguangda/workspace/project/benchmark/coremark-workload/riscv-coremark/coremark.riscv
```

## riscv-tests

1. check

```bash
$ ./main.py list gem5 --suites

$ ./main.py list --build-targets --exclude-tags VEGA_X86 -q
```

2. 运行所有测试

```bash
$ cd tests
$ ./main.py run gem5/asmtest

$ ./main.py run --skip-build gem5/asmtest

$ ./main.py run --skip-build --exclude-tags gcn_gpu gem5/asmtest

$ ./main.py run --skip-build gem5/suite_tests

$ ./main.py run gem5/suite_tests
```

3. 运行特定测试

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

4. 使用本地resources

```bash
$ GEM5_CONFIG=src/python/gem5_config.json ./build/RISCV/gem5.debug tests/gem5/asmtest/configs/riscv_asmtest.py rv64samt-ps-sysclone_d atomic
$ GEM5_CONFIG=src/python/gem5_config.json ./build/RISCV/gem5.debug tests/gem5/asmtest/configs/riscv_asmtest.py hello atomic
$ GEM5_CONFIG=src/python/gem5_config.json ./build/RISCV/gem5.debug --debug-file=test-run.log --debug-flags=Commit,O3CPU,Fetch tests/gem5/asmtest/configs/riscv_asmtest.py hello o3

$ cd tests
$ GEM5_CONFIG=../src/python/gem5_config.json ./main.py run --skip-build gem5/asmtest
```
