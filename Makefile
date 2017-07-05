CFLAGS_      = $(CFLAGS) -O3 -std=gnu99
MCC_FLAGS_   = --ompss
MCC_FLAGS_I_ = --instrument
MCC_FLAGS_D_ = --debug -g -k
LDFLAGS_     = $(LDFLAGS)

## MKL Variables
MKL_DIR      ?= $(MKLROOT)
MKL_INC_DIR  ?= $(MKL_DIR)/include
MKL_LIB_DIR  ?= $(MKL_DIR)/lib
MKL_SUPPORT_ = $(if $(and $(wildcard $(MKL_INC_DIR)/mkl.h ), \
               $(wildcard $(MKL_LIB_DIR)/libmkl_sequential.so )),YES,NO)

ifeq ($(MKL_SUPPORT_),YES)
	CFLAGS_  += -I$(MKL_INC_DIR) -DUSE_MKL
	LDFLAGS_ += -L$(MKL_LIB_DIR) -lmkl_sequential -lmkl_core -lmkl_intel_lp64
#	LDFLAGS_ += -L$(MKL_LIB_DIR) -lmkl_sequential -lmkl_core -lmkl_rt
endif

PROGRAM_ = matmul
PROGS_   = $(PROGRAM_)-p $(PROGRAM_)-i $(PROGRAM_)-d

MCC  ?= mcc
MCC_  = $(CROSS_COMPILE)$(MCC)

all: $(PROGS_)

$(PROGRAM_)-p: ./src/$(PROGRAM_).c
	$(MCC_) $(CFLAGS_) $(MCC_FLAGS_) $^ -o $@ $(LDFLAGS_)

$(PROGRAM_)-i:  ./src/$(PROGRAM_).c
	$(MCC_) $(CFLAGS_) $(MCC_FLAGS_) $(MCC_FLAGS_I_) $^ -o $@ $(LDFLAGS_)

$(PROGRAM_)-d:  ./src/$(PROGRAM_).c
	$(MCC_) $(CFLAGS_) $(MCC_FLAGS_) $(MCC_FLAGS_D_) $^ -o $@ $(LDFLAGS_)

clean:
	rm -f *.o $(PROGS_) $(MCC_)_$(PROGRAM_).c
