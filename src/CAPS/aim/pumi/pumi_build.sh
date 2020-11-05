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
      -DCMAKE_C_COMPILER="mpicc" \
      -DCMAKE_CXX_COMPILER="mpicxx" \
      -DENABLE_EGADS=ON \
      -DEGADS_DIR="$EGADS_DIR" \
      -DBUILD_SHARED_LIBS=FALSE \
      -DCMAKE_POSITION_INDEPENDENT_CODE=ON

    cmake --build . -j 4 --target install

    # For some reason I need to build shared and static libaries to link
    # correctly on linux
    cmake .. \
      -DCMAKE_INSTALL_PREFIX="$PWD/install" \
      -DCMAKE_C_COMPILER="mpicc" \
      -DCMAKE_CXX_COMPILER="mpicxx" \
      -DENABLE_EGADS=ON \
      -DEGADS_DIR="$EGADS_DIR" \
      -DBUILD_SHARED_LIBS=TRUE \
      -DCMAKE_POSITION_INDEPENDENT_CODE=ON

    cmake --build . -j 4 --target install

  else
    echo "PUMI_DIR not set correctly!"
  fi
else
  echo "EGADS_DIR not set correctly!"
fi



