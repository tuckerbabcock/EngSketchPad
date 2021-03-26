#!/bin/bash

EGADS_DIR=$1
PUMI_DIR=$2

if [[ -n "$EGADS_DIR" ]]; then
  if [[ -n "$PUMI_DIR" ]]; then

    cmake "$PUMI_DIR/build" \
      -DENABLE_EGADS=ON \
      -DEGADS_DIR="$EGADS_DIR" \
      -DSCOREC_CXX_OPTIMIZE="YES"

    cmake --build "$PUMI_DIR/build" --target install

  else
    echo "PUMI_DIR not set correctly!"
  fi
else
  echo "EGADS_DIR not set correctly!"
fi



