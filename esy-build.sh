#! /bin/bash

cmake -G"Unix Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$cur__install -S $cur__root -B $cur__target_dir
make -C $cur__target_dir
make -C $cur__target_dir install
