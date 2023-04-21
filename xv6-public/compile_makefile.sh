#!/bin/bash

make clean && make -j && make fs.img -j && ./bootxv6.sh
