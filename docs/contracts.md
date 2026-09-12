# 公共接口与安全契约

本文描述本项目 0.1.0 的接口，不把 Abseil 或原生 expected 的所有能力自动视为 Talon 能力。所有说明同时适用于两个后端，另有说明除外。

## Status

`StatusCode` 是底层类型为 int 的枚举。标准码完整如下；适配器使用显式 switch 映射，测试逐个比对真实 Abseil：

| 数值 | 枚举 | 数值 | 枚举 |
|---:|---|---:|---|
| 0 | kOk | 9 | kFailedPrecondition |
| 1 | kCancelled | 10 | kAborted |
| 2 | kUnknown | 11 | kOutOfRange |
| 3 | kInvalidArgument | 12 | kUnimplemented |
| 4 | kDeadlineExceeded | 13 | kInternal |
| 5 | kNotFound | 14 | kUnavailable |
| 6 | kAlreadyExists | 15 | kDataLoss |
| 7 | kPermissionDenied | 16 | kUnauthenticated |
| 8 | kResourceExhausted | | |

`Status()` 与 `OkStatus()` 为成功，消息为空。`Status(code, std::string)` 拥有消息；char 指针重载复制内容，null 表示 `"(null)"`。成功码丢弃任何消息，非法枚举值归一到 Unknown 并保留消息。非 OK 允许空消息。

`ok()`、`code()` 和 `message()` 无抛出；message 返回对象所拥有字符串的 const 引用，生命周期到该 Status 被修改或销毁。`ToString()` 为大写码名，非空消息追加 `": " + message`，包括嵌入 NUL 字节；这不是序列化协议。

复制拥有独立消息。复制赋值先准备新状态，分配失败时目标不变。移动不抛出，源保留原错误码、消息清空；自移动/自复制保持值。比较以码和完整消息为准，不比较对象身份；swap 无抛出。标准分配器 string 的默认构造/移动要求 noexcept，头内有实际 traits 断言。

提供所有 16 个非 OK 码对应的 `XxxError(args...)`，例如 `CancelledError`、`InvalidArgumentError`、`NotFoundError`、`InternalError`、`UnavailableError`，无参数时消息为空。

## 消息构造

工厂按顺序拼接参数，立即形成自有 string，不保留输入引用。支持 std::string、字符串字面量、以 NUL 结尾的有效 char 指针、nullptr、常用整型/浮点/字符/bool，以及有可访问 `ostream << const T&` 的自定义类型。C++17+ 支持 string_view；通过工厂输入，直接 Status 构造器仍是 string/char 指针接口。

```cpp
auto s = talon::InvalidArgumentError("invalid size: ", size,
                                      ", expected: ", expected);
```

std::string/string_view 保留完整长度与嵌入 NUL；char 指针读到第一个 NUL，null 输出 `(null)`，空字符串输出空内容。非空 char 指针必须可读且正确终止；库不能检测悬垂指针。临时字符串在函数调用期间读取完毕，随后释放不影响消息。

采用经典 locale 的 ostream 默认格式：bool 为 0/1，char 为字符，signed/unsigned char 为整数，浮点使用默认流精度（不承诺无损数值序列化）。自定义插入器可改变后续流格式，应自行保持其约定。不可插入类型产生明确 static_assert 诊断；格式化抛出的异常及 badbit/failbit 错误会传播，后者抛 ios_base::failure。没有任意类型自动格式化、宽字符串转码、fmt 或 Abseil 依赖。

纯字符串/常用整数/char/bool/nullptr/string_view 参数组使用等价的直接拼接快路径：整数转十进制使用 unsigned 幅值和按 numeric_limits 计算的栈缓冲，安全处理有符号最小值，不依赖 CHAR_BIT=8。含任何浮点、自定义类型或流操纵器时，整组仍走 ostream，保持操纵器对后续参数的影响；两条路径均立即形成自有消息，分配/长度异常正常传播。快路径不消耗传入的字符串对象。

## StatusOr<T>

公共类型是包装类，`value_type=T`，`error_type=Status`。标准后端私有成员是真实 `std::expected<T, Status>`；没有向 namespace std 注入任何类型，也不暴露可修改原生存储。

| 操作 | 契约 |
|---|---|
| 显式默认构造 | Unknown 错误、空消息；不构造 T，`return {};` 不可用 |
| 从 Status 构造 | 隐式错误转换，只接受实际可复制/移动的 Status；非 OK 必须成立 |
| OK 作为错误源 | 构造/赋值均抛 invalid_argument，赋值目标保持不变；Debug/Release 一致 |
| 从值 U 构造 | T 可从 U 构造时有效；U→T 隐式则结果转换隐式，否则显式 |
| in_place | `StatusOr<T>(talon::in_place, args...)` 直接构造 T，支持非默认构造、局部不可移动值 |
| ok | 只反映是否存在 T，不检查 T 所持指针是否非空 |
| status const& | 成功时返回永久有效的只读 OK 引用；失败时引用当前成员错误 |
| status && | 按值返回并可移动错误；源错误保留码，消息清空 |
| status const&& | 按值复制，避免从 const 临时结果返回成员引用 |
| value、operator* | 提供 T&、const T&、T&&、const T&& 四种重载；移动由调用方决定 |
| operator-> | 返回值对象地址（std::addressof），支持 T 自定义 operator& |

没有 bool 转换，没有跨 `StatusOr<U>` 的自动转换。原生 `std::expected`/`std::unexpected`/标准标记对象不能直接作为值构造或赋值源，防止 greedy T（例如 std::any）被底层特殊重载误解为状态传播。需要把这类对象存为值时，使用 `talon::in_place` 或先显式构造 T，再赋值。嵌套 Talon 结果同样使用 in_place。

失败时 `value()`、`*`、`->` 全部抛 `BadStatusOrAccess`（派生自 logic_error，提供 status()）。异常对象本身需要分配消息，内存不足可能先抛 bad_alloc。合法访问后的引用/指针只在所引用的 T 仍存活时有效；结果赋值、移动或销毁可能改变值或终止其生命周期。对临时结果的引用访问不延长结果寿命。

原始指针或 unique_ptr 值为 null 仍是成功：检查 `ok()` 后还要按应用契约检查指针。库不替应用决定 null 是否错误。

内联存储遵守 T 的对齐要求。C++11/14 动态分配超对齐的 StatusOr 时，调用方须使用满足该对齐的分配方式；库不替调用方实现 aligned new。本地超对齐回归使用栈对象，sanitizer 的工具链限制另见验证报告。

T 必须是完整、非 cv、非数组的对象类型，析构不得抛异常。拒绝 void、T&、const T、volatile T、Status、talon::in_place_t、可见的 std::in_place_t/std::unexpect_t/std::unexpected 特化；头内 static_assert 给出诊断。无需值时返回 Status，需要借用时可使用指针或 reference_wrapper 并自行管理被引用对象寿命。C++11/14 中不可复制也不可移动的结果不能从函数按值返回，这是语言规则；本地 in_place 构造可用，C++17 的强制复制消除允许相应 prvalue 返回，不保证命名 NRVO。

复制构造要求 T 可复制；复制赋值同时要求可复制构造和赋值。移动构造/赋值采用相应要求；若默认移动被删除，语言可能回退到可用的 const 复制。因此 traits 与真实调用能力一致，而不是机械令包装的所有移动 traits 等于 T。特殊成员对照 std::expected 实测，不承诺平凡性或大小一致。

移动构造在 T 的移动构造无抛出时 noexcept，移动赋值还要求 T 的移动赋值无抛出；右值回退到复制时以实际复制路径为准。in_place/值构造以 T 的对应构造 traits 决定 noexcept。复制、消息生成、错误验证、转换和 value 访问不承诺 noexcept；普通值赋值也未额外声明条件 noexcept。

## 状态切换与异常安全

| 转换 | 保证 |
|---|---|
| 错误→值 | T 构造成功后才提交；抛出时目标保留原错误 |
| 值→错误 | 先准备完整 Status，再销毁 T，以无抛出移动提交；复制消息失败不销毁 T |
| 错误→错误 | Status 复制赋值具有强保证；移动无抛出 |
| 值→值 | 调用 T 赋值，失败后的值遵从 T 的保证；库不承诺 T 未改变 |
| 构造 T 失败 | 未构造成功的 T 不调用析构；已构造的库成员正常清理 |
| 从源移动失败 | 目标保持上述不变量；源 T 的内容遵从 T 的移动保证 |

不存在额外的 valueless 状态。兼容层使用 union 内联存储、始终活跃的 Status 和每次 placement-new 返回的指针，后续访问/析构使用该指针，适用于 C++11 的较严格对象替换规则（见 [P0137R1](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2016/p0137r1.html) 中 const 成员示例）。共享 OK 使用无析构、无堆分配的静态存储，可在全局析构时访问。

## 宏

正式名称是 `TALON_ASSIGN_OR_RETURN`、`TALON_RETURN_IF_ERROR`、`TALON_CONTINUE_IF_ERROR`。短名头只在无冲突时定义相同语义；不提供误拼写 `COTINUE_IF_ERROR`。

| 场景 | ASSIGN | RETURN | CONTINUE |
|---|---|---|---|
| expr 恰好一次 | 是 | 是 | 是 |
| 输入 | StatusOr<T> | Status 或 StatusOr<T> | Status 或 StatusOr<T> |
| 成功 | 移动/复制取值到 lhs | 丢弃检查结果，继续 | 继续当前迭代后续语句 |
| 失败 | 立即返回适配错误，不求 lhs | 立即返回适配错误 | 继续最近外围循环，丢弃错误 |
| 无花括号 if/else、循环体 | 不支持 | 支持 | 支持，但须有外围循环 |
| switch 分支 | case 内另加花括号 | 支持 | 在循环内的 switch 中支持 |
| lambda | 明确返回类型；作用于该 lambda | 同左 | 必须有 lambda 自己的外围循环 |

ASSIGN 展开为声明临时结果、条件返回、lhs 初始化/赋值三部分；变量在同一外围块可见。不得作为无花括号 if/else 或循环的唯一语句，也不得跨 case 标签跳过它的声明。部分非法位置编译器会拒绝，部分可能编译却有错误控制流；语法限制不能完全靠宏诊断，必须遵守花括号约定。

```cpp
if (enabled) {
  TALON_ASSIGN_OR_RETURN(Value value, MakeValue());
  Use(value);
}
TALON_ASSIGN_OR_RETURN(existing, MakeValue());
TALON_ASSIGN_OR_RETURN((std::pair<int, int> pair), (MakePair<int, int>()));
TALON_ASSIGN_OR_RETURN((items[index++]), MakeValue()); // index 只在成功时增加
```

ASSIGN 按值保存 expr，因此左值结果被复制，移动专用左值需显式 `std::move(result)`；随后从内部结果的右值取 T。临时结果活到外围块结束，可能比普通表达式临时对象活得久。`const T&`、`T&&`、auto&& 可以借用它的子对象到块结束，不能让引用或捕获逃出块；非 const 左值引用声明不能绑定这个右值。建议默认声明拥有值。对移动专用值不做额外复制。

RETURN 在表达式所属的完整表达式结束前完成检查；成功路径不复制也不移动 StatusOr 中的值，包括 unique_ptr 或不可移动 T。成功输入 StatusOr 的值被忽略，左值对象本身仍保留；临时结果在检查语句结束销毁。失败路径在来源仍存活时取得自有错误状态：左值错误复制、右值错误可移动，返回代理随后持有该错误直到最终返回转换完成。因此 `std::move(临时状态)` 和临时 owner 的引用访问器可以安全作为输入；调用前已经悬垂的引用仍是调用方错误。

CONTINUE 是只有错误 case 的 switch 语句，成功时不匹配任何标签。内部没有循环、lambda 或额外日志/重试；错误触发的 continue 穿过 switch 作用于最近外围循环，do-while 的条件仍按语言规则执行。尾随分号属于 continue，所以外层无花括号 if/else 仍是一条语句；也避免了旧 if/else 实现用于单独外层 if 时的 GCC dangling-else 警告。循环外是编译错误，包括 lambda 外虽有循环但 lambda 内没有循环的情况。它不复制左值结果，表达式临时值在条件检查后正常销毁。该 switch 故意没有 default；用户额外开启 `-Wswitch-default` 时会得到警告，不能把该警告策略与宏同时作为 Werror 使用，除非消费端明确豁免这一检查。

预处理器不识别模板、花括号或结构化绑定中的逗号。lhs 的保护语法是对整个参数加**一层**括号；expr 使用普通完整表达式括号。lhs 若以局部括号开头（`(object).field`），需要整个再次包住，即 `((object).field)`。不保证任意宏生成的声明或任意括号前缀组合。C++17 结构化绑定可在该语言模式下用完整括号保护，不在 C++11 模拟。

使用正常尾随分号。内部变量采用非语言保留的 `talon_status_internal_` 前缀，用户不得占用该前缀；库无法消除一切人为名称碰撞。默认优先用实现扩展 `__COUNTER__` 支持同行多次调用，缺失时回退 __LINE__；无 GNU statement expression。可在所有相关 TU 编译定义 `TALON_STATUS_USE_LINE_COUNTER=1` 强制回退，此时 ASSIGN 必须分行，外层包装宏也不得让多个 ASSIGN 落在同一展开行。

共享头中含宏的 inline/模板定义必须在所有 TU 具有一致的预处理 token。不同的 __COUNTER__ 历史可能改变内部标识符；这类头建议全工程强制 LINE 模式，并把调用分行。普通多 TU 链接通过不能证明任意用户头的 ODR 正确。Microsoft MSVC 前端必须启用 `/Zc:preprocessor`，CMake 目标会传播该选项，传统模式给出明确诊断；clang-cl 使用 Clang 的合规预处理器，不要求该 MSVC 专用开关。

错误代理只能返回所声明支持的 Status/StatusOr 类型，不支持 void、引用或任意业务返回类型。返回类型推导 lambda 可能把代理推导为返回类型而与成功返回冲突，应写 `[]() -> talon::StatusOr<T> { ... }`。

ASSIGN 总是从内部结果的右值提取 T。若 T 显式删除移动构造，即使 StatusOr 本身可以按语言规则从右值回退复制，`ASSIGN_OR_RETURN(T value, expr)` 的直接初始化仍会选中被删除的移动构造并拒绝编译；这类类型应直接检查结果后从 const 左值复制。该限制有预期编译失败测试，不以静默复制改变宏的移动语义。

## Abseil 互操作

`absl_adapter.h` 包含真实 Abseil 并添加独立特化，不按适配开关修改核心类定义。接口为 `ToTalonStatus`、`ToAbslStatus`、`ToTalonStatusOr`、`ToAbslStatusOr`；结果转换支持 const& 复制和 && 移动，移动不可用但复制可用时可绑定 const& 回退。不提供跨体系隐式转换构造器。

启用适配后，宏支持两家的 Status/StatusOr 输入，以及两家的 Status/StatusOr 返回目标（RETURN 4×4，ASSIGN 两种结果输入×四种目标）。代理直接转换到最终目标类型，避免连续两次用户转换。CONTINUE 支持四种输入，仅检查并丢弃错误。

全部标准错误码与消息字节被保留；Abseil 非标准码归一 Unknown。同体系 Abseil 错误返回不经过 Talon，保留原 payload；跨体系转换不保留 payload，Talon 不提供该存储能力。API 习惯相似、迁移辅助和这里声明的类型互操作，都不等于全源码或 ABI 兼容。

## 线程、重入和布局

库不创建线程、锁或异步回调。不同对象可独立使用；同一对象可并发只读（T 自身的 const 访问也必须安全），与写入/移动/析构并发需要调用方同步。引用不会延长对象寿命。T 的构造/赋值/析构及流插入器是用户代码，不能在构造或状态切换未完成时重入访问同一个结果对象；库不承诺中间状态可观察。共享 OK 的首次初始化依赖标准 C++11 线程安全静态初始化，不支持禁用该语言保证的编译选项。

所有交换 StatusOr 的翻译单元必须一致配置语言标准、编译器/标准库、后端和影响定义的宏。AUTO 在混合标准 TU 中可能选不同布局；这违反 ODR，链接器通常不会诊断。无跨后端/版本/标准库 ABI 稳定承诺。兼容后端的 Status 和存储指针使布局可能更大；不承诺平凡复制、constexpr、零开销或特定性能。
