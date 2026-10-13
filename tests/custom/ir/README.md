# LLVM IR 综合测试

测试直接构造 C++ IR，覆盖 `IrBuilder`、`IrPrinter`、`IrVerifier` 和配套
`TypeTable`。只运行构建/检查 IR 的 C++ 程序，不执行生成的 IR，不验证 AST
lowering、RISC-V 后端、运行结果或 REIMU。所有测试源文件与入口都在本目录。

## 运行

需要 C++17 编译器和 LLVM **22** 的 `llvm-as`、`opt`、`FileCheck`。
coverage 还需要 Clang、`llvm-profdata`、`llvm-cov`，推荐使用同一 LLVM 安装。
sanitize 需要支持 ASan/UBSan 的编译器。

```sh
LLVM_BIN=/opt/homebrew/opt/llvm/bin bash tests/custom/ir/run.sh
LLVM_BIN=/opt/homebrew/opt/llvm/bin MODE=release bash tests/custom/ir/run.sh
LLVM_BIN=/opt/homebrew/opt/llvm/bin MODE=sanitize bash tests/custom/ir/run.sh
CXX=/opt/homebrew/opt/llvm/bin/clang++ LLVM_BIN=/opt/homebrew/opt/llvm/bin MODE=coverage bash tests/custom/ir/run.sh

# FILTER 是 shell glob，必须加引号。仍会编译共享 IR 与单元执行器。
LLVM_BIN=/opt/homebrew/opt/llvm/bin FILTER='builder.*' bash tests/custom/ir/run.sh
LLVM_BIN=/opt/homebrew/opt/llvm/bin FILTER='verifier.same_block.forward' bash tests/custom/ir/run.sh

# 执行器自身不调用 LLVM；独立运行只执行 C++ 断言。
build/custom-ir/debug/ir-unit --list
build/custom-ir/debug/ir-unit --case cfg.diamond.reverse.d1.u3 --output-dir build/custom-ir/manual-diamond
```

`CXX` 是一个编译器可执行文件路径，不是包含额外 flags 的命令字符串。
默认 `MODE=debug`，使用 `-O0 -g`；release 使用 `-O2 -DNDEBUG`；sanitize 使用
`-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all`；coverage 使用
`-O0 -g -fprofile-instr-generate -fcoverage-mapping`。断言不使用 C `assert`，
release 下仍有效。使用默认 FILTER 运行四种模式构成完整验收。

退出码：**0** 全部检查满足预期；**1** 存在测试失败；**2** 工具、模式、筛选、
执行器配置或覆盖率生成错误。预期非法输入被正确拒绝计为通过，其他失败返回 1。

## 组织与判定

- 原有 `01`–`10` 集成程序保留构造逻辑及 FileCheck 检查，仅将打印入口改为
  `verifyAndPrint`。项目 Verifier 拒绝时不打印；通过后检查文本及 LLVM。
- `unit/test.hpp` 提供具名测试注册、常驻断言、结构快照、精确异常类型检查和
  `IrError` 字段检查；`unit/main.cpp` 提供 `--list` 与 `--case`。
- `unit/builder.cpp`、`printer.cpp`、`verifier.cpp`、`cfg.cpp` 分别注册对应测试。
  每例独立进程，崩溃/断言失败不会阻止其他子例。零断言的子例直接失败。
- `--list` 输出 TSV：`case ID / LLVM oracle / 预期 LLVM 诊断关键字`。
  oracle 为 `none`、`accept`、`reject-parse` 或 `reject-verify`。
- LLVM 解析使用 `llvm-as -disable-verify`；验证使用 `opt -passes=verify`。
  预期拒绝必须退出 1 并包含目标诊断；崩溃、缺少 IR、其他阶段的失败不算通过。
  即使项目断言失败，也继续保存独立 LLVM 证据。
- Builder 负例要求精确的 `std::logic_error` 类型及目标消息。结构快照包含类型、
  结构体、签名、值表、指令内容和终结状态，独立于 Printer；随后插入合法指令
  检查插入目标及下一 ValueId，防止失败操作部分修改状态。

## 覆盖矩阵

| 分组 | 场景 |
| --- | --- |
| `types.*` | 五类类型、驻留/查找、按值 `get`、数组类型键与嵌套、opaque/空结构体/共享布局、无效引用、不支持宽度、void/数组引用循环/命名循环、有效引用与有限布局的区别 |
| `builder.constant.*` | i1/i8/i32 的 0、1、2、255、256、最高位及全 1 位模式；常量掩码和 null 表示 |
| `builder.align.*` | 全部 256 个 uint8_t 取值，对 alloca、entry alloca、load、store 检查接受/拒绝 |
| `builder.lifecycle`、`guard.*`、`declaration_guards`、`empty_function_name.*` | 初始/清除/函数切换/块切换、无块/声明/重复符号/重复定义、空函数名的声明/定义拒绝、参数边界、签名及字段拒绝、晚插入 entry alloca 与插入点保持 |
| `builder.operand.*`、`real_foreign_value` | 无效类型、局部值范围/类型、函数归属、直接 Immediate 越界、非整数 Immediate、错误 NullPointer；只注册能够到达目标校验的组合 |
| `builder.terminated.*`、`memory_unsized.*`、`instruction.*` | 所有 emitter 的终结状态边界、各类 unsized 类型、地址/运算类型、调用参数过少/过多/类型错误、分支和返回错误 |
| `builder.cast.*`、`gep_*`、`result_bookkeeping` | 全部宽度转换方向；零/单/动态/嵌套/指针源 GEP、结构体宽度/动态/空/越界/负索引和标量过度索引；void call/store 不分配 ValueId |
| `printer.*` 与原有 10 例 | 全部 9 类指令、13 个 opcode、10 个 predicate、4 类 terminator；i1/i8/i32 运算矩阵；聚合与调用；声明/定义顺序；空模块/空 target；名称转义；结构体行边界；非顺序及两位以上编号；流 flags 与重复打印；自递归/互递归 |
| `verifier.*` | 空模块/声明、完整性错误收集、入口前驱含不可达前驱、布局循环和传播、DAG 与 TypeId/StructId 数值碰撞、阶段短路、同块顺序与自引用、多函数 SSA 错误收集 |
| `verifier.use.*` | 每个操作数位置的有效/无效支配：load 地址、store 地址/值/双错、binary/icmp 左/右/双错、zext/trunc、GEP 基址及三处索引、call 各参数/双错、condbr、ret；逐字段检查 IrError |
| `cfg.*` | 直线、菱形、循环、非入口自环、重复后继、不可约循环、不可达链、不可达循环；正向/反向非入口编号；枚举每个定义块与使用块组合，共 228 例 |

CFG 预期由独立的路径排除算法给出：对可达使用，移除定义块后仍有入口到使用块
的路径，则定义不支配使用；不可达定义不能供可达块使用。不可达使用按 LLVM 22
的实际行为接受，不能套用可达块的顺序规则。所有定义都先通过 Builder 注册，
再构造使用及终结指令。只有同块顺序/自引用反例受控调整已注册的指令操作数或顺序。

Verifier 测试保留现有职责划分，不要求它处理任意损坏的 ID、重复结果、损坏的
参数表、非法 enum 或其他绕过 Builder 局部校验的对象。raw 数组引用循环只用于
Builder 类型检查/字段拒绝，不交给 Verifier。

## 产物与结果

产物位于 `build/custom-ir/<MODE>/`。每例目录保存 `module.ll`、适用时的
`module.bc`、`project-verifier.log`、`llvm-as.log`、`llvm-verify.log`；单元例还有
`assertions.log`、`run.log`，集成例还有 FileCheck/编译日志。
`summary.txt` 和 `results.tsv` 记录逐例结果，`cases.tsv` 是单元例清单。
每次清理所选例的旧产物；FILTER 未选例的历史目录保留，不计入本轮结果。

coverage 模式在测试失败后仍合并本轮 profile，生成 `coverage.txt`、
`coverage.json`、`coverage-html/index.html`。统计限定 `src/ir`，不包含测试代码
和标准库。当前被调用的所有二进制参与合并，未选集成例的旧 profile 不参与。

2026-10-13 修复后本地实测：共 **745 例**，其中原有集成程序 10 例、具名子例
735 例（Builder 413、类型 2、Printer 26、Verifier 65、CFG 228、辅助入口 1）。
debug/release/sanitize 使用 Apple Clang 21.0.0，coverage 使用 Homebrew Clang
22.1.8；LLVM 检查工具均为 22.1.8。

| 模式 | 通过 | 失败 | 原有 10 例 | 退出码 |
| --- | ---: | ---: | --- | ---: |
| debug | 745 | 0 | 全部通过 | 0 |
| release | 745 | 0 | 全部通过 | 0 |
| sanitize | 745 | 0 | 全部通过 | 0 |
| coverage | 745 | 0 | 全部通过 | 0 |

修复前四模式均为 733 通过、10 失败；本轮对应回归点及新增空函数名拒绝测试均通过。
预期非法输入被正确拒绝的用例计入通过数。sanitize 日志没有 ASan/UBSan
报告。所有单元测试源在四种模式下均无编译警告；共享实现仍有 Printer 未使用
参数的既有警告。

| 实现 | 函数覆盖 | 行覆盖 | 分支覆盖 |
| --- | ---: | ---: | ---: |
| Builder | 100.00% | 99.60% | 98.20% |
| Printer | 100.00% | 100.00% | 95.39% |
| TypeTable | 100.00% | 98.51% | 85.00% |
| Verifier | 100.00% | 100.00% | 98.26% |
| `src/ir` 已映射源文件与头文件合计 | 100.00% (97/97) | 99.78% (1336/1339) | 96.72% (442/457) |

region 覆盖为 97.82% (582/595)。已映射的全部 IR 函数均执行到；
未覆盖行/分支主要来自下节说明的保护条件和不可达分支。LLVM 的行计数可能包含
模板实例化累计，应使用报告中的原始分母，不将其当作去重后的物理源码行数。
新增加但未被这些组件包含、没有 coverage mapping 的文件不计入此统计。

## 回归修复与覆盖边界

以下回归点已在 `src/ir` 做最小修复，测试保持正常的通过/失败判定，不使用 XFAIL。
测试运行本身不修改生产代码。

- `verifier.same_block.forward` / `.self`：定义表记录实际块和指令下标，可达同块
  要求定义严格早于使用，项目与 LLVM 均拒绝；不可达使用及晚插入 entry alloca 仍通过。
- `printer.stream.hex` / `.hex_showbase` / `.octal` / `.octal_showbase` / `.showpos`：
  所有数字经 `printUnsigned` 用 `to_chars` 转成十进制，再以 `ostream::write` 写入，
  不受流 flags 影响，也不改变调用方 flags。
- `printer.struct_lines.opaque_defined` / `.opaque_opaque`：opaque 分支补上换行，
  满足逐行输出约定。这两例原始文本已被 LLVM 接受，属于格式回归。
- `printer.name.0`：保留空结构体名，用非空函数名完成引用检查。
  Builder 在修改状态前拒绝空函数名；新增声明/定义负例检查异常、状态、插入点、
  ValueId 和 FunctionId。不会再将空函数名直接打印成不可引用的 `@""`。

不能据此声称所有任意输入或运行语义均已验证。以下覆盖缺口不通过篡改内部状态补齐：

- `hasInsertionPoint` 的 declaration 分支要求当前 block 有值，但正常接口在
  选择 declaration 时会清除它；类似的失效 current function/block 保护不制造。
- TypeTable 的越界 `get`/非整数 `integerSize` assertion 失败，以及
  `integerSize` 在整数 1/8/32 之外的最终 return，不属于当前支持输入。
- GEP 结构体字段访问前已检查 sizedness，因此后面的 `fields.has_value()` false
  条件无法从正常 Builder 输入到达；void struct field / void GEP 的晚检查类似。
- Printer variant 的穷尽末分支、合法 enum switch 的默认边界、Verifier DFS 对
  已有状态节点的重复入口，以及常真循环条件存在不可达/冗余分支。

规范依据：[本地 Rx backend 规范](../../../../rx-compiler-specification/src/backend.md)、
[LLVM 22 LangRef](https://releases.llvm.org/22.1.0/docs/LangRef.html)、
[Clang source coverage](https://clang.llvm.org/docs/SourceBasedCodeCoverage.html)。
RV32/ILP32 target 配置沿用现有测试，不把 LLVM 接受等同于 Rx lowering 或后端正确。
