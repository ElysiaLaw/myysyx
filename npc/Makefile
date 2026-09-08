#以下为根命名

MY_BASE_DIR = .
MY_SIM_NAME = sim
MY_TOP = top

#以下为辅助命名

MY_BUILD_DIR = $(MY_BASE_DIR)/build
MY_OBJ_DIR = $(MY_BUILD_DIR)/obj_dir

$(info mybuilddir = $(MY_BUILD_DIR))

MY_SRC_CPP = $(wildcard $(MY_BASE_DIR)/csrc/*.cpp)
MY_SRC_V = $(wildcard $(MY_BASE_DIR)/vsrc/*.v)

#以下为nvboard构建变量

MY_NVBIND = $(MY_BUILD_DIR)/auto_bind.cpp
MY_NXDC = $(MY_BASE_DIR)/constr/top.nxdc
MY_SRC_CPP += $(MY_NVBIND)

$(info mynxdc = $(MY_NXDC))

#以下为中间参数编译

INCFLAGS = $(addprefix -I,$(NVBOARD_HOME)/usr/include)
CXXFLAGS += $(INCFLAGS) -std=c++17 \
	-DTOP_NAME="\"V$(MY_TOP)\"" \
	-DTOP_CLASS=V$(MY_TOP) \
	-DSTIM_TOP_NAME=$(MY_TOP)


$(info MY_SRC_V = $(MY_SRC_V))
#$(info MY_SRC_CPP = $(MY_SRC_CPP))  #检测测试，

.PHONY: build sim all run clear

all: sim

build: $(MY_SRC_V) $(MY_SRC_CPP) $(NVBOARD_ARCHIVE) $(MY_NXDC) \
	$(MY_NVBIND)
	$(call git_commit, "build RTL") # DO NOT REMOVE THIS LINE!!!
	@mkdir -p $(MY_OBJ_DIR)
	@rm -rf $(MY_OBJ_DIR)
	verilator --cc --exe --build -j 0 --trace --vpi --public-flat-rw \
		--top-module $(MY_TOP) \
		--Mdir $(MY_OBJ_DIR) \
		$(MY_SRC_V) $(MY_SRC_CPP) $(NVBOARD_ARCHIVE) \
		-o $(MY_SIM_NAME) \
		$(addprefix -CFLAGS , $(CXXFLAGS)) \
		$(addprefix -LDFLAGS , $(LDFLAGS))

sim: build run

$(MY_NVBIND): $(MY_NXDC)
	@rm -f $@
	python3 $(NVBOARD_HOME)/scripts/auto_pin_bind.py \
		$< $@

run:
	$(MY_OBJ_DIR)/$(MY_SIM_NAME)

clear:
	@rm -rf $(MY_BUILD_DIR)


include ../Makefile
include $(NVBOARD_HOME)/scripts/nvboard.mk
