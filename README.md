# talon-status

`talon-status` 是面向大型 C++ 工程的错误处理基础库，提供 `talon::Status`、`talon::StatusOr<T>` 以及一组经过严格约束的错误传播宏。

它解决的不只是“如何返回一个错误”，还包括长期工程治理中的几个核心问题：错误语义是否统一，失败路径是否容易审查，资源所有权是否明确，不同语言标准和基础设施能否采用同一套接口，以及公共工具是否会给宿主工程引入隐式依赖、全局构建选项或不可控的控制流。

项目借鉴 Google/Abseil 的工程经验，同时维护独立、明确的接口契约。核心库不依赖 Abseil，不承诺与 Abseil 的源码或 ABI 完全兼容；已经使用 Abseil 的项目可以显式启用互操作层。

## 工程定位

在大型代码库中，错误处理同时影响接口设计、代码审查、可观测性边界、资源生命周期和跨模块协作。`talon-status` 将这些约束收敛为一套可复用规则：

- 业务函数通过标准错误码和自有消息表达失败，调用者无需解析日志文本或依赖异常类型推断语义。
- `StatusOr<T>` 把“有效值”和“错误状态”约束为互斥状态，适合工厂、解析、查找、RPC 边界和多阶段流水线。
- 传播宏让正常路径保持线性，减少重复的样板判断，同时保留清晰、可预测的 return/continue 语义。
- 宏只负责值提取与控制流，不记录日志、不重试、不吞掉异常，也不植入业务策略。日志归属、重试策略和错误包装仍由调用层决定。
- 公共接口覆盖 C++11 至 C++23；C++23 工具链具备标准能力时真实采用 `std::expected<T, talon::Status>` 作为内部基础。
- CMake 目标只传播使用库所必需的要求。严格警告、sanitizer、覆盖率和测试设置不会污染消费端。

这使错误处理成为可评审、可测试的接口协议，而不是散落在调用链中的约定俗成。对数百层调用关系、跨团队组件和多平台产品线而言，这种一致性通常比增加更多宏更有价值。

## 核心能力

- 完整的 17 个标准 `StatusCode`，以及 `OkStatus()` 和常用错误工厂。
- 拥有错误消息的 `Status`，支持字符串、数值和可流输出的自定义类型安全拼接。
- 单模板参数 `StatusOr<T>`，支持移动专用、不可默认构造和受限复制/移动类型。
- C++23 标准后端和项目自带的 C++11 兼容后端。
- `TALON_ASSIGN_OR_RETURN`、`TALON_RETURN_IF_ERROR`、`TALON_CONTINUE_IF_ERROR`。
- 独立头文件启用的 `ASSIGN_OR_RETURN`、`RETURN_IF_ERROR`、`CONTINUE_IF_ERROR` 短名称。
- 可选的真实 Abseil 状态、结果和宏错误传播互操作。
- 安装导出、`find_package`、`add_subdirectory`、消费端测试和跨平台 CI 配置。
- 无外部测试框架依赖的生命周期、异常、控制流、长调用链及预期编译失败测试。

## 快速示例

```cpp
#include <vector>

#include "talon/status_macros_short.h"

struct Value {
  int size;
};

talon::StatusOr<Value> MakeValue(int size) {
  if (size < 0) {
    return talon::InvalidArgumentError(
        "invalid size: ", size, ", expected: nonnegative");
  }
  return Value{size};
}

talon::Status Validate(const Value& value) {
  return value.size == 0 ? talon::FailedPreconditionError("empty value")
                         : talon::OkStatus();
}

talon::StatusOr<Value> BuildValue(int size) {
  Value value{1};
  ASSIGN_OR_RETURN(value, MakeValue(size));
  ASSIGN_OR_RETURN(Value checked, MakeValue(value.size));
  RETURN_IF_ERROR(Validate(checked));
  return checked;
}

void Process(const std::vector<int>& sizes) {
  for (int size : sizes) {
    CONTINUE_IF_ERROR(BuildValue(size));
    // 只处理成功项；失败是该宏明确选择的丢弃策略。
  }
}
```

完整程序见 [examples/basic.cc](examples/basic.cc)。生产代码建议优先使用带 `TALON_` 前缀的正式宏；短名称只由 `talon/status_macros_short.h` 启用，发现已有同名宏时会给出编译错误，不会静默覆盖。

## 宏的工程契约

| 宏 | 成功路径 | 失败路径 | 主要用途 |
|---|---|---|---|
| `TALON_ASSIGN_OR_RETURN(lhs, expr)` | 从结果中提取值并初始化或赋值 `lhs` | 从当前函数返回错误 | 产生后续步骤需要的值 |
| `TALON_RETURN_IF_ERROR(expr)` | 继续执行，若输入是结果则丢弃其值 | 从当前函数返回错误 | 验证、提交和只关心成败的调用 |
| `TALON_CONTINUE_IF_ERROR(expr)` | 继续当前迭代的后续语句 | 跳过当前迭代 | 显式允许单项失败的批处理 |

三个宏都保证 `expr` 恰好求值一次。`ASSIGN_OR_RETURN` 只有成功时才求值复杂左值；`RETURN_IF_ERROR` 会在来源临时对象仍存活时取得自有错误状态；`CONTINUE_IF_ERROR` 的 `continue` 作用于最近一层用户循环。

为了让声明形式的变量在调用后继续可见，`ASSIGN_OR_RETURN` 有意展开为多条语句，必须放在带花括号的作用域中：

```cpp
if (enabled) {
  TALON_ASSIGN_OR_RETURN(Value value, MakeValue(8));
  Use(value);
}
```

包含预处理器逗号的左侧参数需要整体加括号：

```cpp
TALON_ASSIGN_OR_RETURN((std::pair<int, int> value), MakePair());
```

宏作用域、引用寿命、`if/else`、`switch`、lambda、同一行调用和 ODR 边界详见 [公共接口与安全契约](docs/contracts.md)。

## 构建与测试

核心库要求 C++11、异常支持及 CMake 3.16 或更新版本。配置 C++23 目标需要 CMake 3.20 或更新版本。

标准后端：

```sh
cmake -S . -B build \
  -DCMAKE_CXX_COMPILER=g++-12 \
  -DCMAKE_BUILD_TYPE=Release \
  -DTALON_STATUS_BACKEND=STD \
  -DTALON_STATUS_BUILD_TESTS=ON \
  -DTALON_STATUS_BUILD_EXAMPLES=ON \
  -DTALON_STATUS_STRICT_WARNINGS=ON
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure --parallel 4
```

C++11 兼容后端：

```sh
cmake -S . -B build11 \
  -DCMAKE_CXX_COMPILER=g++ \
  -DCMAKE_BUILD_TYPE=Debug \
  -DTALON_STATUS_BACKEND=COMPAT \
  -DTALON_STATUS_CXX_STANDARD=11 \
  -DTALON_STATUS_BUILD_TESTS=ON \
  -DTALON_STATUS_STRICT_WARNINGS=ON
cmake --build build11 --parallel 4
ctest --test-dir build11 --output-on-failure --parallel 4
```

`AUTO` 是默认后端：只有语言模式、头文件和 `__cpp_lib_expected` 能力同时满足要求时才选择标准后端。显式请求不可用的 `STD` 会在配置或编译阶段给出清楚错误，而不会退回另一实现。

所有 CMake 选项和已验证工具链范围见 [兼容性说明](docs/compatibility.md)。

## 安装与消费

```sh
cmake -S . -B build-core
cmake --install build-core --prefix "$PWD/install"
```

消费端：

```cmake
find_package(talon-status 0.1 CONFIG REQUIRED)

add_executable(app main.cc)
target_link_libraries(app PRIVATE talon::status)
```

配置消费工程时，将安装前缀加入 `CMAKE_PREFIX_PATH`。源码集成也可以使用 `add_subdirectory(path/to/talon-status)` 和同一个 `talon::status` 目标。

## 可选 Abseil 互操作

适配层位于 `talon/absl_adapter.h`，只在使用者显式包含并链接适配目标时引入真实 Abseil：

```cmake
find_package(talon-status 0.1 CONFIG REQUIRED COMPONENTS absl)
target_link_libraries(app PRIVATE talon::status_absl)
```

```cpp
#include "talon/absl_adapter.h"

absl::StatusOr<int> ConsumeTalon() {
  TALON_ASSIGN_OR_RETURN(int value, talon::StatusOr<int>(42));
  TALON_RETURN_IF_ERROR(talon::OkStatus());
  return value;
}
```

跨体系转换保留标准错误码和完整消息。同体系 Abseil 错误传播保留 payload；跨 Talon/Abseil 边界时 payload 不保留，因为 Talon 核心没有 payload 存储。Abseil 必须与消费端使用兼容的编译器、语言标准、标准库和 ABI 配置。

## 质量保障

测试覆盖正常路径和错误路径之外的工程边界，包括：

- 值/错误状态切换、构造与赋值异常、分配失败、超对齐、静态析构和资源计数。
- 移动专用、不可默认构造、不可移动及受限特殊成员类型。
- 宏单次求值、左值副作用、提前返回、所有循环形式、嵌套循环、`if/else`、`switch`、lambda、模板和逗号参数。
- 最长 257 个逻辑节点的混合调用链，并在每层注入入口错误、出口错误和异常，核对完整消息、控制流和析构平衡。
- Talon/Abseil 状态与结果的交叉输入、交叉返回、移动值和 payload 边界。
- 预期编译失败、公共头自包含、多翻译单元、安装后外部消费、严格警告、ASan/UBSan 和覆盖率。

GitHub Actions 和 GitLab CI 配置覆盖 GCC、Clang、AppleClang 和 MSVC。CI 文件代表维护矩阵；具体平台是否通过应以对应提交的真实流水线结果为准。

## 性能原则

错误处理基础库位于大量调用路径上，因此性能优化必须以契约等价和可复现测量为前提。常用字符串和整数消息采用无第三方依赖的直接拼接路径；自定义流输出、格式操纵器和浮点参数继续使用完整流语义。结果基准由独立翻译单元消费，避免编译器消除被测构造。

本地基准中，常见消息构造相对统一流实现减少约 77%–95% 的耗时，64 层错误传播减少约 8%–9%。这些数字只描述记录的编译器、硬件和工作负载，不构成跨平台时延保证。方法、数据和复现命令见 [性能设计与基准](docs/performance.md)。

## 项目结构

```text
include/talon/       公共类型、宏、兼容存储和可选适配层
tests/               单元、长调用链、诊断、头文件和消费端测试
benchmarks/          无外部依赖的微基准
examples/            可编译、可运行的接口示例
cmake/               包导出及编译/配置/消费测试驱动
docs/                API 契约、兼容性和性能说明
.github/workflows/   GitHub Actions
.gitlab-ci.yml       GitLab CI
```

## 文档

- [公共接口与安全契约](docs/contracts.md)
- [构建选项与兼容范围](docs/compatibility.md)
- [性能设计与基准](docs/performance.md)
- [第三方来源与授权](THIRD_PARTY.md)

## 许可证

项目采用 [Apache License 2.0](LICENSE)。
