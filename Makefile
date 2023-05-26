.PHONY: clean
all: help

# Include the corresponding compiler makefile
--setup: FORCE
  ifeq ($(COMPILER),llvm)
    include llvm.mk
  else
    ifeq ($(COMPILER),mcxx)
      include mcxx.mk
    else
      $(info No valid COMPILER variable defined, using mcxx)
      include mcxx.mk
    endif
  endif
FORCE:

PROGRAM_ = matmul

# Required FPGA bitstream variables
#FPGA_HWRUNTIME         ?= som
FPGA_CLOCK             ?= 200
FPGA_MEMORY_PORT_WIDTH ?= 128
INTERCONNECT_OPT       ?= performance
MATMUL_BLOCK_SIZE      ?= 64
MATMUL_BLOCK_II        ?= 2
MATMUL_NUM_ACCS        ?= 1

COMPILER_FLAGS_ += -DMATMUL_BLOCK_SIZE=$(MATMUL_BLOCK_SIZE) -DMATMUL_BLOCK_II=$(MATMUL_BLOCK_II) -DMATMUL_NUM_ACCS=$(MATMUL_NUM_ACCS) -DFPGA_MEMORY_PORT_WIDTH=$(FPGA_MEMORY_PORT_WIDTH) -DBOARD=\"$(BOARD)\"

ifdef USE_URAM
	COMPILER_FLAGS_ += -DUSE_URAM
endif

common-help:
	@echo 'Supported targets:        $(PROGRAM_)-p, $(PROGRAM_)-i, $(PROGRAM_)-d, $(PROGRAM_)-seq, design-p, design-i, design-d, bitstream-p, bitstream-i, bitstream-d, clean, help'
	@echo 'FPGA env. variables:      BOARD, FPGA_CLOCK'
	@echo 'FPGA opt. env. variables: FPGA_MEMORY_PORT_WIDTH, MEMORY_INTERLEAVING_STRIDE, SIMPLIFY_INTERCONNECTION, INTERCONNECT_OPT, INTERCONNECT_REGSLICE, FLOORPLANNING_CONSTR, SLR_SLICES, PLACEMENT_FILE'

$(PROGRAM_)-p: ./src/$(PROGRAM_).c
	$(COMPILER_) $(COMPILER_FLAGS_) $^ -o $@ $(LINKER_FLAGS_)

$(PROGRAM_)-i: ./src/$(PROGRAM_).c
	$(COMPILER_) $(COMPILER_FLAGS_) $(COMPILER_FLAGS_I_) $^ -o $@ $(LINKER_FLAGS_)

$(PROGRAM_)-d: ./src/$(PROGRAM_).c
	$(COMPILER_) $(COMPILER_FLAGS_) $(COMPILER_FLAGS_D_) $^ -o $@ $(LINKER_FLAGS_)

$(PROGRAM_)-seq: ./src/$(PROGRAM_).c
	$(COMPILER_) $(COMPILER_FLAGS_) $^ -o $@ $(LINKER_FLAGS_)

design-p: ./src/$(PROGRAM_).c
	$(eval TMPFILE := $(shell mktemp))
	$(COMPILER_) $(COMPILER_FLAGS_) \
		$(AIT_FLAGS_) $(AIT_FLAGS_DESIGN_) \
		$^ -o $(TMPFILE) $(LINKER_FLAGS_)
	rm $(TMPFILE)

design-i: ./src/$(PROGRAM_).c
	$(eval TMPFILE := $(shell mktemp))
	$(COMPILER_) $(COMPILER_FLAGS_I_) \
		$(AIT_FLAGS_) $(AIT_FLAGS_DESIGN_) \
		$^ -o $(TMPFILE) $(LINKER_FLAGS_)
	rm $(TMPFILE)

design-d: ./src/$(PROGRAM_).c
	$(eval TMPFILE := $(shell mktemp))
	$(COMPILER_) $(COMPILER_FLAGS_D_) \
		$(AIT_FLAGS_) $(AIT_FLAGS_DESIGN_) $(AIT_FLAGS_D_) \
		$^ -o $(TMPFILE) $(LINKER_FLAGS_)
	rm $(TMPFILE)

bitstream-p: ./src/$(PROGRAM_).c
	$(eval TMPFILE := $(shell mktemp))
	$(COMPILER_) $(COMPILER_FLAGS_) \
		$(AIT_FLAGS_) \
		$^ -o $(TMPFILE) $(LINKER_FLAGS_)
	rm $(TMPFILE)

bitstream-i: ./src/$(PROGRAM_).c
	$(eval TMPFILE := $(shell mktemp))
	$(COMPILER_) $(COMPILER_FLAGS_I_) \
		$(AIT_FLAGS_) \
		$^ -o $(TMPFILE) $(LINKER_FLAGS_)
	rm $(TMPFILE)

bitstream-d: ./src/$(PROGRAM_).c
	$(eval TMPFILE := $(shell mktemp))
	$(COMPILER_) $(COMPILER_FLAGS_D_) \
		$(AIT_FLAGS_) $(AIT_FLAGS_D_) \
		$^ -o $(TMPFILE) $(LINKER_FLAGS_)
	rm $(TMPFILE)

