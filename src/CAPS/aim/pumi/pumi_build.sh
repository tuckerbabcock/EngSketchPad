#!/bin/bash

EGADS_DIR=$1
PUMI_DIR=$2

if [[ -n "$EGADS_DIR" ]]; then
  if [[ -n "$PUMI_DIR" ]]; then
    cd $PUMI_DIR

    mkdir -pv build

    cd build

    cmake .. \
      -DCMAKE_INSTALL_PREFIX="$PWD/install" \
      -DSCOREC_EXTRA_C_FLAGS="-O0 -g -Wno-array-bounds -fPIC" \
      -DSCOREC_EXTRA_CXX_FLAGS="-O0 -g -Wno-array-bounds -fPIC" \
      -DCMAKE_C_COMPILER="mpicc" \
      -DCMAKE_CXX_COMPILER="mpicxx" \
      -DSCOREC_ENABLE_CXX11=YES \
      -DENABLE_EGADS=ON \
      -DEGADS_DIR="$EGADS_DIR" \
      -DBUILD_SHARED_LIBS=FALSE

    cmake --build . -j 4 --target install

    # # For some reason I need to build shared and static libaries to link
    # # correctly on linux
    # cmake .. \
    #   -DCMAKE_INSTALL_PREFIX="$PWD/install" \
    #   -DSCOREC_EXTRA_C_FLAGS="-O0 -g -Wno-array-bounds -fPIC" \
    #   -DSCOREC_EXTRA_CXX_FLAGS="-O0 -g -Wno-array-bounds -fPIC" \
    #   -DCMAKE_C_COMPILER="mpicc" \
    #   -DCMAKE_CXX_COMPILER="mpicxx" \
    #   -DSCOREC_ENABLE_CXX11=YES \
    #   -DENABLE_EGADS=ON \
    #   -DEGADS_DIR="$EGADS_DIR" \
    #   -DBUILD_SHARED_LIBS=TRUE

    # cmake --build . -j 4 --target install

  else
    echo "PUMI_DIR not set correctly!"
  fi
else
  echo "EGADS_DIR not set correctly!"
fi



