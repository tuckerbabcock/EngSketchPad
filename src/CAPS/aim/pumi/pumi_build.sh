#!/bin/bash

EGADS_DIR=$1
PUMI_DIR=$2
PUMI_BUILD=$3
PUMI_INSTALL=$4

if [[ -n "$EGADS_DIR" ]]; then
  if [[ -n "$PUMI_DIR" ]]; then
    if [[ -n "$PUMI_BUILD" ]]; then
      if [[ -n "$PUMI_INSTALL" ]]; then

        mkdir -p "$PUMI_BUILD"
        cd "$PUMI_BUILD"

        cmake "$PUMI_DIR" \
         -DCMAKE_C_COMPILER="mpicc" \
         -DCMAKE_CXX_COMPILER="mpicxx" \
         -DSCOREC_ENABLE_CXX11="ON" \
         -DSCOREC_CXX_OPTIMIZE="OFF" \
         -DSCOREC_CXX_SYMBOLS="ON" \
         -DCMAKE_POSITION_INDEPENDENT_CODE="ON" \
         -DENABLE_EGADS="ON" \
         -DEGADS_DIR="$EGADS_DIR" \
         -DBUILD_SHARED_LIBS="ON" \
         -DCMAKE_INSTALL_PREFIX="../$PUMI_INSTALL"

        cmake --build . --target install --parallel 4
      else
        echo "PUMI_INSTALL not set correctly!"
      fi
    else
      echo "PUMI_BUILD not set correctly!"
    fi
  else
    echo "PUMI_DIR not set correctly!"
  fi
else
  echo "EGADS_DIR not set correctly!"
fi



