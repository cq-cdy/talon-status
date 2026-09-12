# 来源与授权

本工程实现采用 Apache License 2.0，完整文本见 LICENSE；版权标识为 Talon Status Contributors。

设计参考 [C++23 工作草案 N4950](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2023/n4950.pdf) 及 [Abseil 官方 Status 接口](https://abseil.io/docs/cpp/guides/status)。本仓库不包含 Abseil、fmt、测试框架或第三方 expected 的实现副本；兼容存储为本项目实现。API 名称相似不构成行为、源码或 ABI 完全兼容承诺。

可选互操作测试使用真实 Abseil（Apache License 2.0）。本地消费安装包，CI 在独立工作路径构建固定版本；分发 Abseil 的工程必须自行遵守其 LICENSE/NOTICE。libstdc++、libc++、编译器、CMake 等属于外部工具链，不随本项目分发。

LICENSE 为 Apache License 2.0 标准文本。无第三方源文件需要额外归属声明；如未来引入源代码，须同步更新本文件并保留相应授权。
