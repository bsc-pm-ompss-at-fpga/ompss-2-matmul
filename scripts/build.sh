#!/bin/bash -e

if [ "$BOARD" == "" ]; then
  echo "BOARD environment variable not defined"
  exit 1
elif [ "$FPGA_HWRUNTIME" == "" ]; then
  echo "FPGA_HWRUNTIME environment variable not defined"
  exit 1
fi

OUT_DIR=$(pwd -P)/build
RES_FILE=$(pwd -P)/resources_results.json

mkdir -p $OUT_DIR

if [ "$1" == "binary" ]; then
  #Only build the binaries
  make clean
  make matmul-p matmul-d
  mv matmul-p matmul-d $OUT_DIR
else
  make clean
  make bitstream-p LDFLAGS=--Wf,--disable_utilization_check
  mv matmul-p $OUT_DIR
  mv matmul_ait/matmul.bin $OUT_DIR/bitstream.bin
  mv matmul_ait/matmul.bit $OUT_DIR/bitstream.bit

  printf "{\"version\": \"${FPGA_HWRUNTIME}_${MATMUL_BLOCK_SIZE}\", " >>$RES_FILE
  printf "\"hwruntime\": \"${FPGA_HWRUNTIME}\", " >>$RES_FILE
  printf "\"accels_freq\": \"${FPGA_CLOCK}\", " >>$RES_FILE
  printf "\"memory_port_width\": \"${FPGA_MEMORY_PORT_WIDTH}\", " >>$RES_FILE
  printf "\"benchmark\": \"matmul\", " >>$RES_FILE
  printf "\"BRAM\": \"" >>$RES_FILE
  grep "BRAM" matmul_ait/matmul.resources-hls.txt | awk '{printf $2}' >>$RES_FILE
  printf "\", \"DSP\": \"" >>$RES_FILE
  grep "DSP" matmul_ait/matmul.resources-hls.txt | awk '{printf $2}' >>$RES_FILE
  printf "\", \"FF\": \"" >>$RES_FILE
  grep "FF" matmul_ait/matmul.resources-hls.txt | awk '{printf $2}' >>$RES_FILE
  printf "\", \"LUT\": \"" >>$RES_FILE
  grep "LUT" matmul_ait/matmul.resources-hls.txt | awk '{printf $2}' >>$RES_FILE
  printf "\", \"BRAM_IMPL\": \"" >>$RES_FILE
  grep "BRAM" matmul_ait/matmul.resources-impl.txt | awk '{printf $2}' >>$RES_FILE
  printf "\", \"DSP_IMPL\": \"" >>$RES_FILE
  grep "DSP" matmul_ait/matmul.resources-impl.txt | awk '{printf $2}' >>$RES_FILE
  printf "\", \"FF_IMPL\": \"" >>$RES_FILE
  grep "FF" matmul_ait/matmul.resources-impl.txt | awk '{printf $2}' >>$RES_FILE
  printf "\", \"LUT_IMPL\": \"" >>$RES_FILE
  grep "LUT" matmul_ait/matmul.resources-impl.txt | awk '{printf $2}' >>$RES_FILE
  printf "\", \"WNS\": \"" >>$RES_FILE
  grep "WNS" matmul_ait/matmul.timing-impl.txt | awk '{printf $2}' >>$RES_FILE
  printf "\", \"TNS\": \"" >>$RES_FILE
  grep "TNS" matmul_ait/matmul.timing-impl.txt | awk '{printf $2}' >>$RES_FILE
  printf "\", \"NUM_ENDPOINTS\": \"" >>$RES_FILE
  grep "NUM_ENDPOINTS" matmul_ait/matmul.timing-impl.txt | awk '{printf $2}' >>$RES_FILE
  printf "\", \"NUM_FAIL_ENDPOINTS\": \"" >>$RES_FILE
  grep "NUM_FAIL_ENDPOINTS" matmul_ait/matmul.timing-impl.txt | awk '{printf $2}' >>$RES_FILE
  printf "\"},\n" >>$RES_FILE
fi
