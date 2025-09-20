#!/bin/bash

ARCH=${1:-ARM}

scons build/${ARCH}/libgem5_opt.so -j4 --without-tcmalloc --duplicate-sources
