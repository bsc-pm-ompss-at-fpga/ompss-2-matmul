#!/bin/bash -el

MATRIX_SIZES=(2048 4096)
RES_FILE=$(pwd -P)/test_results.json

if [ "$BOARD" == "alveo_u200" ]; then
  MATRIX_SIZES=(3072 6144)
fi

for EXEC_MODE in d p; do
  for CREATE_FROM in 0 1; do
    for MATRIX_SIZE in ${MATRIX_SIZES[@]}; do
      echo "=== Check mode: ${EXEC_MODE}, from: ${CREATE_FROM}, msize: ${MATRIX_SIZE} ==="
      CHECK=$((([ "$MATRIX_SIZE" == "2048" ] || [ "$MATRIX_SIZE" == "3072" ]) && echo 1) || echo 0)
      NX_ARGS="--summary --fpga-alloc-pool-size=1G" timeout --preserve-status 250s ./build/matmul-${EXEC_MODE} ${MATRIX_SIZE} ${CHECK} ${CREATE_FROM}
      cat test_result.json >>$RES_FILE
      echo "," >>$RES_FILE
    done
  done
done
