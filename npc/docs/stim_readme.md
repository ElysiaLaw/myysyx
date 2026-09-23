# STIM 使用与代码参考

STIM 根据仿真时间写入真实模型的输入，并在模型求值后比较输出。时钟、启动复位、时间推进、NVBoard 和波形由主仿真负责。

## 运行

在 `npc` 目录执行：

```sh
make sim                                      # 使用 constr/top.stim
make sim STIM_FILE=constr/example.stim          # 使用带注释的完整示例
make build                                    # 只构建，不运行脚本
./build/obj_dir/sim constr/example.stim         # 已构建后直接换脚本，无需重编译
make sim STIM_FILE=constr/example.stim STIM_REPORT_PASS=1
make build TRACE_ON=0                          # NVBoard 模式，须有匹配端口的约束
```

`example.stim` 基于当前 `top` 的高位优先编码器和译码器，检查 `out`、`valid`、`out2`，包含全部单独输入位、多位同时为 1、全零、全一、保持输入、忽略位及一次故意 FAIL。它应在 **t=25** 输出一次 FAIL，在 **t=26** 恢复通过，最终正常返回 0。示例的非法配置和复位前事件会输出 WARNING；注释中的非法语法用于手动测试，不会自动执行。

`STIM_FILE` 仅选择运行时文件，不改变模型名；切换 RTL 或 `MY_TOP` 必须重新构建。路径相对于运行目录，建议从 `npc` 执行。直接运行二进制时首个位置参数为脚本路径，其余参数仍交给 Verilator。

## 脚本格式

```text
top=top
max_time=36
reset_time=20
@default { data=8h00; out=3b000; }
@21 { data=8b00000010; }
expect @21 { out=3b001; }
```

第一条非空、非注释语句是 `top=<MY_TOP>`，必须匹配 Makefile 的 `MY_TOP`。允许 `top = top` 两侧空白。名字和关键字区分大小写。

| 语句 | 含义 |
| --- | --- |
| `max_time=36` | TRACE 仿真最后一个绝对时间戳，含复位阶段 |
| `reset_time=20` | 启动复位在 t=20 释放；0 表示不制造启动复位脉冲 |
| `@default { data=8h00; out=3b000; }` | 启用脚本时只初始化一次输入；建立默认输出期望 |
| `@21 { data=8h01; }` | t=21 更新输入，之后保持到下一次设置 |
| `expect @21 { out=3b000; }` | t=21 起更新该输出期望，持续到下一次设置 |

一个块可跨多行；一行只能有一个块。赋值之间用分号分隔，末项分号可省略。空块不执行操作。不同端口可分开写在同一时刻的多个块中；同一时刻同一端口重复设置，按文件顺序最后一项生效。不同时间点按时间执行，不要求文件按时间排序。

无事件时**保持上次输入**，不会每拍回写 `@default`。未列出的普通输入在脚本启用时初始化为零。未配置期望的输出不比较；已有输出期望会一直保持，直到更新。

### 数值、x 和注释

| 写法 | 含义 |
| --- | --- |
| `1b0`、`1b1` | 1 位二进制 |
| `8b00000001` | 8 位二进制，必须写足 8 位 |
| `8h1`、`8h01` | 8 位十六进制，不足高位补零 |
| `3h7` | 非四的倍数位宽也允许，但数值不得溢出 |
| `3b1xx` | 仅比较输出最高位，低两位忽略 |
| `8h8x` | 输出低四位忽略 |
| `3bxxx` | 三个输出位全部忽略 |
| `0`、`1` | 保留的单比特简写，相当于 `1b0`、`1b1` |

位宽必须等于真实端口位宽。基数字母只支持小写 `b`、`h`，十六进制数字和 `x/X` 不区分大小写。`x` 只能用于输出期望，不能用于输入。这里的 x 是比较掩码，不会让 Verilator 的双态模型变成四态仿真；不支持 `z`、十进制端口赋值、Verilog 单引号数值写法。

`//` 支持行注释、行尾注释；`STIM_HASH_COMMENT=1` 时 `#` 也表示注释。不支持块注释 `/* ... */`。该注释处理只针对 STIM，不扫描或改写 Verilog。

## 时钟、复位和时间

- `main_time` 从 0 起算，t=0 初始化 clk=0；t=1 上升、t=2 下降，之后每刻度翻转一次。此规律不受复位长度影响。
- `reset(bool)` 只设置 rst，既不操作 clk，也不推进时间和求值。
- `reset_time=R>0` 时，t=R 的正常时钟求值仍处于复位中，随后释放 rst、初始化默认输入、再次求值；最终状态只记录一次。
- 显式输入/期望点 **t<=R 被忽略并警告**；第一个可执行点是 R+1，不存在原先额外的两拍。例如 R=20 时 @21 正常执行。
- 默认输入在释放时立即生效；第一轮期望比较从 R+1 开始。R 为奇数时释放后下一沿是下降沿，不会强行制造上升沿。
- R=0 时不产生启动复位脉冲，t=0 初始化默认值，显式 @0 仍属于忽略边界。
- `max_time<R` 时到最大时间即结束，并提示尚未完成启动复位。不会为了复位超出最大时间。
- 每个普通时刻：应用输入及期望更新 → 常规时钟沿和 `eval()` → 检查输出 → 记录波形 → 推进时间。

clk 归主时序，脚本不能赋值。启动 rst 归 main，不能放在 `@default` 中；复位结束后允许显式 `@time { rst=1b1; }` / `rst=1b0`，它只驱动该信号，不改变时钟相位、时间和其他输入，也不触发脚本重新初始化。同步/异步复位效果由 RTL 定义，STIM 不添加复位时钟。

TRACE_OFF 也推进主仿真时间、执行启动复位，只读取运行配置，不加载端口、驱动脚本或比较期望。NVBoard 持续运行，不因 MAX_TIME 结束；DUT 主动 `$finish` 可结束进程。

## 配置和诊断

`max_time`、`reset_time` 是十进制无符号整数，范围 0 到 2^64-2。配置优先级：

```text
脚本最后一个合法值 > make 命令行变量 > Makefile 默认值 > main.h 的 #ifndef 兜底值
```

脚本的覆盖发生在运行时，不能改变编译宏。缺失或非法值保留已有合法值，没有合法值则使用 main/Makefile 默认值；非法值输出 WARNING。重复合法配置输出 WARNING，最后一个合法值生效。建议把配置放在 top 行之后、事件之前。

| 情况 | 行为 |
| --- | --- |
| 时间配置非法、重复配置、复位边界内事件 | shell WARNING，继续 |
| 脚本格式、未知端口、方向或位宽错误 | 完整分析并逐条输出 shell ERROR，在仿真前返回 1 |
| 期望不匹配 | shell ERROR，含来源行、时间、端口、expected、actual、FAIL；继续至终点，返回 0 |
| 文件丢失，TRACE_ON=1 | ERROR，返回 1 |
| 文件丢失或配置头不匹配，TRACE_ON=0 | WARNING，保留默认运行配置 |

错误和警告输出到 stderr，不再写入 `stim.txt`。脚本解析错误不会创建本次波形，旧波形可能仍存在；不要把旧文件当成本次结果。默认不创建或截断 `stim.txt`；已有历史报告也不会自动删除。`STIM_REPORT_PASS=1` 时才在脚本启用时打开/覆盖 `stim.txt`，保存 PASS/SKIP，文件关闭时刷新。

`__init_check__` 位于实际 MY_TOP 的顶层作用域。某时刻只要一个被比较输出失败就为 1，通过或无检查为 0。全 x 期望会通过。PASS 开关不控制 ERROR/WARNING，也不影响比较。

make 显示 `Error 1` 表示子进程返回了 1；make 自身一般返回 2。不要把“编译成功”“解析成功”“所有期望通过”混为同一件事。

## 源码架构与接口

| 文件 | 职责 |
| --- | --- |
| `csrc/main.cpp` | 主入口、主要全局变量、运行配置覆盖、主仿真循环与生命周期 |
| `csrc/main.h` | TRACE/MAX_TIME/RESET_TIME 默认值、主入口依赖与可选 STIM 接入 |
| `csrc/sim_utils.h/.cpp` | reset、常规时钟、时间推进、波形记录和显示层级整理 |
| `csrc/stim.h` | STIM 配置宏、Config、Program 接口和内部数据声明 |
| `csrc/stim.cpp` | 语法解析、诊断、VPI 端口绑定、输入驱动与输出比较 |
| `Makefile` | MY_TOP、宏命令行覆盖、递归 RTL 文件集合、Verilator 构建、NVBoard 分支依赖 |

STIM 已删除手写 Verilog 文本扫描。不再按 top.v 文件名或 module 文本猜测端口，而是通过当前已构建模型的 VPI 元数据读取名称、方向和位宽。参数化模块、表达式位宽和源码层级交给 Verilator 处理。当前 Verilator 使用 `vpi_iterate(vpiReg, scope)` 暴露变量，再按 `vpiDirection` 筛选 IO；不是所有仿真器都支持相同的枚举约定。

main 的空实例名使顶层 IO 处于 `MAIN_VPI_SCOPE="TOP"`，而顶层 Verilog 模块名来自 `MY_TOP`。这两个名字含义不同。`MAIN_CLOCK_SIGNAL` / `MAIN_RESET_SIGNAL` 可指定模型的时钟、复位 C++ 成员名。打包位向量支持至 `STIM_MAX_WIDTH`（默认 65536）；inout 和非打包数组明确不支持。示例业务端口名仅出现在示例脚本，不写入驱动实现。

### Program 生命周期

```cpp
stim::Program program;
program.load(path, top_name, scope, clock_name, reset_name, config_only);
program.config();              // 取得 optional max_time/reset_time
program.start(release_time);   // 释放复位时调用一次：初始化默认值、丢弃早期事件
program.apply(main_time);      // 正常时刻、DUT 求值之前
bool matched = program.check(main_time); // DUT 求值之后
```

`load()`：DUT 必须已经创建；清理旧状态、读取配置/语句、绑定端口并校验、汇总诊断；失败返回 false，驱动保持关闭。`config_only=true` 只读配置，不绑定模型；用于 TRACE_OFF。配置值用 `std::optional<uint64_t>` 表示，未设置与值为 0 不混淆。

`start()` 只能在每次成功 load 后调用一次；`apply()`/`check()` 按递增主时间调用。Program 不拥有 DUT、不调用 eval、不推进时间。Program 不可复制；析构释放句柄并关闭可选报告。main 用 RAII 确保 Program 在模型销毁前释放。

内部数据：端口表保存 VPI 句柄/方向/位宽；defaults 保存初始赋值；events 按时间保存显式输入和期望；expected 保存当前有效输出期望及来源行。所有状态属于 Program 对象，不使用跨运行静态全局 STIM 状态。

旧接口 `get_port/readfile/load_program/init_runtime/apply_inputs/check_outputs/finish_report/print_errors`、旧 `STIM_V_PATH/STIM_TOP_NAME/TOP_NAME` 宏及扫描器已移除。使用新接口迁移，不保留空函数或旧逻辑兼容层。

### 主时序接口

- `reset(bool active)`：只写模型 rst。
- `clock_step()`：翻转一次 clk 并 eval。
- `advance_time()`：推进 main_time 和 Verilator context time，独立于 TRACE。
- `trace_init()`：注册 `__init_check__`；无 tfp 时不启用。
- `trace_step()`：只记录当前时间，不自增。
- `hide_rootio_from_wave()`：整理显示层级，将检查信号放入 MY_TOP，去掉 $rootio；失败保留原波形并报告错误。

删除 `stim.h/stim.cpp` 后，主仿真仍按自身 MAX_TIME、RESET_TIME、TRACE_ON 运行，不要求 STIM 提供任何时钟、复位或波形函数。TRACE_ON=1 完全不编译或链接 NVBoard。
