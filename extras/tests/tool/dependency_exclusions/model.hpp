// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Sergei Kolesnik

#pragma once

namespace dependency_exclusions {

enum class excluded_color { red };

struct opaque;
using opaque_alias = opaque;

struct hidden_leaf {
  int value;
};

struct opaque {
  hidden_leaf child;

  int read(int) const { return child.value; }
  int read(bool) const { return child.value; }
};

template <typename T>
struct view {
  T value;
};

template <typename T, unsigned Size>
struct buffer {
  T values[Size];
};

} // namespace dependency_exclusions
