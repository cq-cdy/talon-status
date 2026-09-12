// Copyright 2026 Talon Status Contributors. SPDX-License-Identifier: Apache-2.0
#include <memory>
#include <string>

int main() {
  std::unique_ptr<std::string> text(new std::string("baseline"));
  return text->empty() ? 1 : 0;
}
