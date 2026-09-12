# 构建选项与兼容范围

代码要求 C++11 及异常。下表区分本地实际验证与 CI 维护目标；CI 配置本身不代表对应平台已经运行成功。

## 后端选择

| CMake TALON_STATUS_BACKEND | 预处理器 TALON_STATUS_BACKEND | 行为 |
|---|---:|---|
| AUTO（默认） | 0 | C++23 且 `<expected>` 与功能宏可用时选 STD，否则 COMPAT |
| STD | 1 | 真实 std::expected，缺能力时 CMake/头文件报错 |
| COMPAT | 2 | 强制随库存储，可在 C++11 至 C++23 使用 |

头文件以 `_MSVC_LANG`（MSVC）或 `__cplusplus` 识别语言模式，并结合 `__has_include`、`__cpp_lib_expected >= 202202L`。不满足能力时不在旧语言头中泄漏新语法。`TALON_STATUS_USE_STD_EXPECTED` 是计算出的只读宏，供测试/诊断查看；不要手动定义。所有配置在包含第一个 Talon 头前完成。

CMake 的 `TALON_STATUS_CXX_STANDARD` 控制本项目测试/示例，允许 11/14/17/20/23。消费端自行设置语言标准；STD 导出目标传播至少 C++23。直接使用头时以 `-DTALON_STATUS_BACKEND=2` 等选择，必须保证所有相关 TU 一致。MSVC 没有 C++11 标准选项，本项目不会宣称 MSVC 的默认/C++14 模式就是实测 C++11。

## 选项

| 选项 | 默认 | 说明 |
|---|---|---|
| TALON_STATUS_BUILD_TESTS | OFF | 内置断言框架，无下载 |
| TALON_STATUS_BUILD_EXAMPLES | OFF | 构建 basic 示例 |
| TALON_STATUS_BUILD_BENCHMARKS | OFF | 两个翻译单元的无依赖微基准；与测试同时开启时增加 smoke 测试 |
| TALON_STATUS_WITH_ABSL | OFF | 查找已安装真实 Abseil、提供 talon::status_absl |
| TALON_STATUS_ABSL_TESTS | OFF | 必须开启核心测试；自动启用适配目标 |
| TALON_STATUS_STRICT_WARNINGS | OFF | 仅项目自身目标启用严格警告及 Werror/WX |
| TALON_STATUS_SANITIZERS | OFF | GCC/Clang 测试目标 ASan+UBSan，不传播消费端 |
| TALON_STATUS_COVERAGE | OFF | GCC/gcov 测试插桩，不传播消费端 |

分配失败注入测试替换全局 new/delete，必须与 ASan 的分配器拦截分开：sanitizer 配置下该测试仍完整运行，但不加 ASan/UBSan；其他生命周期测试照常插桩。覆盖率仍包含分配失败测试。Clang12 使用实测支持的 LLVM 选项开启返回后栈检测插桩。GCC12/libasan.so.8 的 fake-stack 会破坏 128 字节超对齐对象的对齐，仓库在 `tests/toolchain/gcc12_asan_alignment.cc` 保留了不依赖 Talon 的最小复现。CI 由 Clang 对 COMPAT 后端执行 UAR 检查，由 GCC 对 STD 后端执行其余 ASan/UBSan 检查并关闭 GCC 的 UAR 路径。

`TALON_STATUS_USE_LINE_COUNTER` 是预处理器宏，不是 CMake 缓存选项。需要时使用 `target_compile_definitions(app PRIVATE TALON_STATUS_USE_LINE_COUNTER=1)`，相关 TU 都保持一致。

## 支持依据

| 工具链 | 语言/后端范围 | 证据性质 |
|---|---|---|
| Linux GCC 11.4 / libstdc++11 | C++11 COMPAT | 本地编译、运行、真实旧 Abseil |
| Linux GCC 12.3 / libstdc++12 | C++11/14/17/20 COMPAT；C++23 AUTO/STD/COMPAT | 本地编译与运行 |
| Linux Clang 12.0.1 / 本机 libstdc++ | C++11 AUTO→COMPAT | 本地编译与运行；不宣称 Clang12 的 std::expected 能力 |
| Linux GCC14 / libstdc++14 | C++23 STD | GitHub/GitLab CI 配置，尚未远端验证 |
| Linux Clang18 / libc++18 | C++11 COMPAT、C++23 AUTO/STD | CI 配置，尚未远端验证 |
| macOS Xcode16.4 / AppleClang + libc++ | C++11 COMPAT、C++23 STD | CI 配置，尚未远端验证 |
| Windows VS2022 / MSVC STL | C++17 COMPAT、C++23 AUTO/STD | CMake 选择工具链真实支持的 C++23 选项；CI 尚未验证 |

官方能力参考：[libstdc++ 状态表](https://gcc.gnu.org/onlinedocs/libstdc++/manual/status.html)、[libc++ C++23 状态](https://libcxx.llvm.org/Status/Cxx23.html)、[MSVC 标准预处理器](https://learn.microsoft.com/en-us/cpp/build/reference/zc-preprocessor?view=msvc-170)。这些是配置依据，实际接受标准后端还必须通过探针和测试。

Abseil 20211102.0 的独立匹配安装用于 C++11，20250127.1 用于 C++23。后者要求至少 C++17；不能通过选择 Talon COMPAT 降低第三方库的最低标准。同版本 Abseil 的 `ABSL_OPTION_USE_STD_STRING_VIEW`、编译模式或标准库配置也可能造成 ABI 差异，应使用同一构建配置和明确 `absl_DIR`。

## 补充验证命令

```sh
# Release、严格警告
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release \
  -DTALON_STATUS_BUILD_TESTS=ON -DTALON_STATUS_STRICT_WARNINGS=ON
cmake --build build-release --parallel 4
ctest --test-dir build-release --output-on-failure --parallel 4

# GCC12，两个后端分别替换 BACKEND
cmake -S . -B build-asan -DCMAKE_CXX_COMPILER=g++-12 \
  -DCMAKE_BUILD_TYPE=Debug -DTALON_STATUS_CXX_STANDARD=23 \
  -DTALON_STATUS_BACKEND=COMPAT -DTALON_STATUS_BUILD_TESTS=ON \
  -DTALON_STATUS_SANITIZERS=ON
cmake --build build-asan --parallel 4
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 \
  ctest --test-dir build-asan --output-on-failure

# GCC覆盖率，gcovr是可选的开发工具，并非库依赖
cmake -S . -B build-coverage -DCMAKE_CXX_COMPILER=g++-12 \
  -DCMAKE_BUILD_TYPE=Debug -DTALON_STATUS_CXX_STANDARD=11 \
  -DTALON_STATUS_BACKEND=COMPAT -DTALON_STATUS_BUILD_TESTS=ON \
  -DTALON_STATUS_COVERAGE=ON
cmake --build build-coverage --parallel 4
ctest --test-dir build-coverage --output-on-failure
gcovr --gcov-executable gcov-12 --root . --filter include/talon/ \
  --exclude-unreachable-branches --print-summary --html-details coverage.html
```

CI 默认核心构建禁用 Abseil 查找；适配 job 显式下载固定的真实版本到独立目录。GitLab macOS/Windows job 需要带指定 tag 的自有 runner，并通过 `TALON_ENABLE_MACOS_RUNNER=1` / `TALON_ENABLE_WINDOWS_RUNNER=1` 启用，不能把未调度作通过。跨平台共享的库配置、CI runner 可用性和未来编译器升级仍需维护者验证。
