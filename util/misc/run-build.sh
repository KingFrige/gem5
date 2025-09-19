#!/bin/bash

ARCH=${1:-RISCV}
verision=${2:-debug}

if [[ ${ARCH} == "TOTAL" ]]; then
  for arch in ALL X86 RISCV ARM; do
    for ver in opt debug fast; do
      time scons -sQ build/${arch}/gem5.${ver} --gold-link -j4
    done
  done
else
  time scons -sQ build/${ARCH}/gem5.${verision} --gold-link -j4
fi
