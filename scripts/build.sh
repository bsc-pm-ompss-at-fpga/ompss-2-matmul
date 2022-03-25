#!/bin/bash -e

if [ "$BOARD" == "" ]; then
  echo "BOARD environment variable not defined"
  exit 1
elif [ "$FPGA_HWRUNTIME" == "" ]; then
  echo "FPGA_HWRUNTIME environment variable not defined"
  exit 1
fi

PROG_NAME=matmul
OUT_DIR=$(pwd -P)/build
RES_FILE=$(pwd -P)/resources_results.json

# Cleanup
make clean
mkdir -p $OUT_DIR

if [ "$1" == "binary" ]; then
  #Only build the binaries
  make ${PROG_NAME}-p ${PROG_NAME}-i ${PROG_NAME}-d
  mv ${PROG_NAME}-p ${PROG_NAME}-i ${PROG_NAME}-d $OUT_DIR
elif [ "$1" == "design" ]; then
  #Only generate the design
  make design-p design-i design-d

  #Remove OUT_DIR directory since we are not generating output products
  rm -rf $OUT_DIR
else
  make bitstream-p LDFLAGS=--Wf,--disable_utilization_check

  mv ${PROG_NAME}_ait/${PROG_NAME}.bin $OUT_DIR/bitstream.bin
  mv ${PROG_NAME}_ait/${PROG_NAME}.bit $OUT_DIR/bitstream.bit
  mv ${PROG_NAME}_ait/${PROG_NAME}.xtasks.config $OUT_DIR/xtasks.config

  printf "{\"version\": \"${FPGA_HWRUNTIME}_${MATMUL_BLOCK_SIZE}\", " >>$RES_FILE
  printf "\"hwruntime\": \"${FPGA_HWRUNTIME}\", " >>$RES_FILE
  printf "\"accels_freq\": \"${FPGA_CLOCK}\", " >>$RES_FILE
  printf "\"memory_port_width\": \"${FPGA_MEMORY_PORT_WIDTH}\", " >>$RES_FILE
  printf "\"benchmark\": \"${PROG_NAME}" >>$RES_FILE
  for PARAM in BRAM DSP FF LUT; do
    printf "\", \"${PARAM}\": \"" >>$RES_FILE
    grep "$PARAM" ${PROG_NAME}_ait/${PROG_NAME}.resources-hls.txt | awk '{printf $2}' >>$RES_FILE
  done
  for PARAM in BRAM DSP FF LUT; do
    printf "\", \"${PARAM}_IMPL\": \"" >>$RES_FILE
    grep "${PARAM}" ${PROG_NAME}_ait/${PROG_NAME}.resources-impl.txt | awk '{printf $2}' >>$RES_FILE
  done
  for PARAM in WNS TNS NUM_ENDPOINTS NUM_FAIL_ENDPOINTS; do
    printf "\", \"${PARAM}\": \"" >>$RES_FILE
    grep "${PARAM}" ${PROG_NAME}_ait/${PROG_NAME}.timing-impl.txt | awk '{printf $2}' >>$RES_FILE
  done
  printf "\"},\n" >>$RES_FILE
fi
