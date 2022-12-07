// Copyright 2018 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef NINJA_HASHED_STR_VIEW_
#define NINJA_HASHED_STR_VIEW_

#include <stdint.h>

#include "string_piece.h"

class HashedStr;

using StrHashType = size_t;

class HashedStrView {
public:
  constexpr HashedStrView() {}

  HashedStrView(const char* str) : str_(str), hash_(HashStr(str_)) {}
  HashedStrView(const std::string& str) : str_(str), hash_(HashStr(str_)) {}
  HashedStrView(StringPiece str) : str_(str), hash_(HashStr(str_)) {}
  inline HashedStrView(const HashedStr& str);

  // Bypass the hashing step when necessary for better performance.
  explicit HashedStrView(const StringPiece& str, StrHashType hash)
      : str_(str), hash_(hash) {}

  const StringPiece& str_view() const { return str_; }

  StrHashType hash() const { return hash_; }
  const char* data() const { return str_.data(); }
  size_t size() const { return str_.size(); }
  bool empty() const { return str_.empty(); }

private:
  // Reversing the order of these fields would break the constructors, most of
  // which initialize hash_ using str_.
  StringPiece str_;
  StrHashType hash_ = 0;
};

class HashedStr {
public:
  HashedStr() {}

  HashedStr(const char* str) : str_(str), hash_(HashStr(str_)) {}
  HashedStr(const std::string& str) : str_(str), hash_(HashStr(str_)) {}
  HashedStr(std::string&& str) : str_(std::move(str)), hash_(HashStr(str_)) {}
  explicit HashedStr(StringPiece str) : str_(NonNullData(str), str.size()), hash_(HashStr(str_)) {}
  explicit HashedStr(const HashedStrView& str) : str_(NonNullData(str.str_view()), str.size()), hash_(str.hash()) {}

  HashedStr& operator=(std::string&& str) {
    str_ = std::move(str);
    hash_ = HashStr(str_);
    return *this;
  }

  HashedStr& operator=(const std::string& str) {
    str_ = str;
    hash_ = HashStr(str_);
    return *this;
  }

  HashedStr& operator=(StringPiece str) {
    str_.assign(NonNullData(str), str.size());
    hash_ = HashStr(str_);
    return *this;
  }

  HashedStr& operator=(const HashedStrView& str) {
    str_.assign(NonNullData(str.str_view()), str.size());
    hash_ = str.hash();
    return *this;
  }

  const std::string& str() const { return str_; }

  StrHashType hash() const { return hash_; }
  const char* c_str() const { return str_.c_str(); }
  const char* data() const { return str_.data(); }
  size_t size() const { return str_.size(); }
  bool empty() const { return str_.empty(); }

private:
  static const char* NonNullData(const StringPiece& piece) {
    return piece.data() == nullptr ? "" : piece.data();
  }

  // Reversing the order of these fields would break the constructors, most of
  // which initialize hash_ using str_.
  std::string str_;
  StrHashType hash_ = 0;
};

inline HashedStrView::HashedStrView(const HashedStr& str) : str_(str.str()), hash_(str.hash()) {}

inline bool operator==(const HashedStr& x, const HashedStr& y) {
  return x.hash() == y.hash() && x.str() == y.str();
}

inline bool operator==(const HashedStr& x, const HashedStrView& y) {
  return x.hash() == y.hash() && x.str() == y.str_view();
}

inline bool operator==(const HashedStrView& x, const HashedStr& y) {
  return x.hash() == y.hash() && x.str_view() == y.str();
}

inline bool operator==(const HashedStrView& x, const HashedStrView& y) {
  return x.hash() == y.hash() && x.str_view() == y.str_view();
}

inline bool operator!=(const HashedStr& x, const HashedStr& y) { return !(x == y); }
inline bool operator!=(const HashedStr& x, const HashedStrView& y) { return !(x == y); }
inline bool operator!=(const HashedStrView& x, const HashedStr& y) { return !(x == y); }
inline bool operator!=(const HashedStrView& x, const HashedStrView& y) { return !(x == y); }

namespace std {
  template<> struct hash<HashedStrView> {
    size_t operator()(const HashedStrView& x) const { return x.hash(); }
  };

  template<> struct hash<HashedStr> {
    size_t operator()(const HashedStr& x) const { return x.hash(); }
  };
}

// The comparison operators ignore the hash code. For efficiency, the
// ConcurrentHashMap orders keys by their hash code first, then by their
// operator< functions.

inline bool operator<(const HashedStr& x, const HashedStr& y) { return x.str() < y.str(); }
inline bool operator>(const HashedStr& x, const HashedStr& y) { return x.str() > y.str(); }
inline bool operator<=(const HashedStr& x, const HashedStr& y) { return x.str() <= y.str(); }
inline bool operator>=(const HashedStr& x, const HashedStr& y) { return x.str() >= y.str(); }

inline bool operator<(const HashedStr& x, const HashedStrView& y) { return x.str() < y.str_view(); }
inline bool operator>(const HashedStr& x, const HashedStrView& y) { return x.str() > y.str_view(); }
inline bool operator<=(const HashedStr& x, const HashedStrView& y) { return x.str() <= y.str_view(); }
inline bool operator>=(const HashedStr& x, const HashedStrView& y) { return x.str() >= y.str_view(); }

inline bool operator<(const HashedStrView& x, const HashedStr& y) { return x.str_view() < y.str(); }
inline bool operator>(const HashedStrView& x, const HashedStr& y) { return x.str_view() > y.str(); }
inline bool operator<=(const HashedStrView& x, const HashedStr& y) { return x.str_view() <= y.str(); }
inline bool operator>=(const HashedStrView& x, const HashedStr& y) { return x.str_view() >= y.str(); }

inline bool operator<(const HashedStrView& x, const HashedStrView& y) { return x.str_view() < y.str_view(); }
inline bool operator>(const HashedStrView& x, const HashedStrView& y) { return x.str_view() > y.str_view(); }
inline bool operator<=(const HashedStrView& x, const HashedStrView& y) { return x.str_view() <= y.str_view(); }
inline bool operator>=(const HashedStrView& x, const HashedStrView& y) { return x.str_view() >= y.str_view(); }

#endif  // NINJA_HASHED_STR_VIEW_
