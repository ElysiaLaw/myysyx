这个文件用于记录被我忽略的verilog代码的警告

可以继续优化的（就是我可能为了代码的可读性而进行了忽略）

0. example
location:   example.v       { line 7 : line 9 }
warning:    PINCONNECTEMPTY
test:       忽略原因是---，
            报错原因是---

1. ALU悬空
location:   caculation.v    { line 23 : line 32 }
warning:    PINCONNECTEMPTY
test:       报错提示没有接入cout端口
            这里add模块加法的输出进位悬空，没有输出，也就是加法的输出溢出被忽略了

2.
location:   basic.v         {line 88 : line 114 }
warning:    UNUSEDSIGNAL
test:       报错提示downcheck的第0位没有输出使用
            这里这个downcheck的最低位确实用不到，只是写0位可以用genvar快速生成，代码看上去更简洁
