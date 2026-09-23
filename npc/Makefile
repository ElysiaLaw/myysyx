#********以下执行命令时可配置变量***************

#配置是否启用脚本输入以及wave仿真
TRACE_ON ?= 1

#配置最大仿真时间（仅在TRACE_ON启用时生效）
MAX_TIME ?= 2000

#可选脚本路径，留空使用 stim.h 的 STIM_FILE_PATH
STIM_FILE ?=

#配置复位时间（TRACE_ON和NVBoard均生效）
RESET_TIME ?= 20

#配置检查文件的文件名（check命令使用）
CHECK_FILE ?= * 

#*****************这里面是文件运行的可选参数*************

#工程根目录
MY_BASE_DIR = .

#输出的编译程序名称
MY_SIM_NAME = sim

#顶层模型名
MY_TOP = top

#中间代码文件目录
MY_BUILD_DIR = $(MY_BASE_DIR)/build
MY_OBJ_DIR = $(MY_BUILD_DIR)/obj_dir

#源码文件
MY_SRC_CPP = $(wildcard $(MY_BASE_DIR)/csrc/*.cpp)
MY_SRC_V = $(shell find $(MY_BASE_DIR)/vsrc -type f -name "*.v" | sort)

#nvboard自动绑定的c代码文件位置
MY_NVBIND = $(MY_BUILD_DIR)/auto_bind.cpp

#nvboard自动绑定引脚文件
MY_NXDC = $(MY_BASE_DIR)/constr/top.nxdc

#verilator检查错误输出日志
VER_ERR_LOG = $(MY_BASE_DIR)/last_check_error.log

#verilator检查忽略规则
VER_IGNORE_RULE = -Wno-DECLFILENAME \
				  -Wno-MULTITOP \
#忽略的规则注释：
#1\忽略文件名和内部的模型名字不一致的警告
#2\忽略一个文件里面存在多个顶层模型的警告

#***********************************************

#NVBoard 依赖仅在 TRACE_ON=0 时启用
ifeq ($(TRACE_ON),1)
MY_NV_SRC_CPP =
MY_NV_BUILD_DEPS =
MY_NV_LIB =
MY_NV_LDFLAGS =
else
MY_NV_SRC_CPP = $(MY_NVBIND)
MY_NV_BUILD_DEPS = $(MY_NVBIND) $(MY_NXDC) $(NVBOARD_ARCHIVE)
MY_NV_LIB = $(NVBOARD_ARCHIVE)
MY_NV_LDFLAGS = $(LDFLAGS)
endif

MY_SRC_CPP += $(MY_NV_SRC_CPP)

ifeq ($(TRACE_ON),1)
INCFLAGS =
else
INCFLAGS = $(addprefix -I,$(NVBOARD_HOME)/usr/include)
endif
ifneq ($(origin STIM_REPORT_PASS),undefined)
CXXFLAGS += -DSTIM_REPORT_PASS=$(STIM_REPORT_PASS)
endif

CXXFLAGS += $(INCFLAGS) -std=c++17 \
	-DTOP_CLASS=V$(MY_TOP) \
	-DMY_TOP=$(MY_TOP) \
	-DMAX_TIME=$(MAX_TIME) \
	-DRESET_TIME=$(RESET_TIME) \
	-DTRACE_ON=$(TRACE_ON)


#$(info mynxdc = $(MY_NXDC))
#$(info mybuilddir = $(MY_BUILD_DIR))
#$(info MY_SRC_V = $(MY_SRC_V))
#$(info MY_SRC_CPP = $(MY_SRC_CPP))  #检测测试，

.PHONY: build sim all run clear self_test check

all: sim

build: $(MY_SRC_V) $(MY_SRC_CPP) $(MY_NV_BUILD_DEPS)
	$(call git_commit, "build RTL") # DO NOT REMOVE THIS LINE!!!
	@mkdir -p $(MY_OBJ_DIR)
	@rm -rf $(MY_OBJ_DIR)
	verilator --cc --exe --build -j 0 --trace --vpi --public-flat-rw \
		-Wno-fatal \
		--top-module $(MY_TOP) \
		--Mdir $(MY_OBJ_DIR) \
		$(MY_SRC_V) $(MY_SRC_CPP) $(MY_NV_LIB) \
		-o $(MY_SIM_NAME) \
		$(addprefix -CFLAGS , $(CXXFLAGS)) \
		$(addprefix -LDFLAGS , $(MY_NV_LDFLAGS))

sim: build run

$(MY_NVBIND): $(MY_NXDC)
	@rm -f $@
	@mkdir -p $(dir $@)
	python3 $(NVBOARD_HOME)/scripts/auto_pin_bind.py \
		$< $@

run:
	@echo "**********************************"
	@git add -A
	git status
	@$(MY_OBJ_DIR)/$(MY_SIM_NAME) $(STIM_FILE)
ifeq ($(TRACE_ON),1)
	@gtkwave ./wave.vcd
endif

clear:
	@rm -rf $(MY_BUILD_DIR)

self_test:
	@rm -f ./wave.vcd
	@rm -f ./stim.txt
	@make sim TRACE_ON=1
	@make sim TRACE_ON=0

check:
	clear
	verilator --lint-only -Wall --top-module $(MY_TOP) $(VER_IGNORE_RULE) ./vsrc/$(CHECK_FILE) 2>&1 | tee $(VER_ERR_LOG)

watch:
	watchexec -w "$(abspath $(MY_BASE_DIR)/vsrc)" -e v -- make check



include ../Makefile
ifeq ($(TRACE_ON),0)
include $(NVBOARD_HOME)/scripts/nvboard.mk
endif
