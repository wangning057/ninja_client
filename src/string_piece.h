// Copyright 2011 Google Inc. All Rights Reserved.
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

#ifndef NINJA_STRINGPIECE_H_
#define NINJA_STRINGPIECE_H_

#include <string.h>

#include <algorithm>
#include <string>

#include "hash_map.h"

using namespace std;

/// StringPiece represents a slice of a string whose memory is managed
/// externally.  It is useful for reducing the number of std::strings
/// we need to allocate.
///
/// This class is something like std::string_view from C++17.
struct StringPiece {
  typedef const char* const_iterator;
  typedef const char& const_reference;

  constexpr StringPiece() : str_(nullptr), len_(0) {}

  /// The constructors intentionally allow for implicit conversions.
  StringPiece(const string& str) : str_(str.data()), len_(str.size()) {}
  StringPiece(const char* str) : str_(str), len_(strlen(str)) {}

  constexpr StringPiece(const char* str, size_t len) : str_(str), len_(len) {}

  /// Convert the slice into a full-fledged std::string, copying the
  /// data into a new string.
  string AsString() const {
    return len_ ? string(str_, len_) : string();
  }

  constexpr const_iterator begin() const { return str_; }
  constexpr const_iterator end() const { return str_ + len_; }
  constexpr const char* data() const { return str_; }
  constexpr const_reference operator[](size_t pos) const { return str_[pos]; }
  constexpr size_t size() const { return len_; }
  constexpr bool empty() const { return len_ == 0; }
  constexpr const_reference front() const { return str_[0]; }
  constexpr const_reference back() const { return str_[len_ - 1]; }

  void remove_prefix(size_t n) { str_ += n; len_ -= n; }
  void remove_suffix(size_t n) { len_ -= n; }

  StringPiece substr(size_t pos=0, size_t count=std::string::npos) const {
    return StringPiece(str_ + pos, std::min(count, len_ - pos));
  }

  int compare(StringPiece other) const {
    size_t min_len = std::min(len_, other.len_);
    if (min_len != 0) { // strncmp(NULL, NULL, 0) has undefined behavior.
      if (int cmp = strncmp(str_, other.str_, min_len)) {
        return cmp;
      }
    }
    return (len_ == other.len_) ? 0 : ((len_ < other.len_) ? -1 : 1);
  }

  const char* str_;
  size_t len_;
};

inline bool operator==(StringPiece x, StringPiece y) {
  if (x.size() != y.size())
    return false;
  if (x.size() == 0) // memcmp(NULL, NULL, 0) has undefined behavior.
    return true;
  return !memcmp(x.data(), y.data(), x.size());
}

inline bool operator!=(StringPiece x, StringPiece y) {
  return !(x == y);
}

inline bool operator<(StringPiece x, StringPiece y) { return x.compare(y) < 0; }
inline bool operator>(StringPiece x, StringPiece y) { return x.compare(y) > 0; }
inline bool operator<=(StringPiece x, StringPiece y) { return x.compare(y) <= 0; }
inline bool operator>=(StringPiece x, StringPiece y) { return x.compare(y) >= 0; }

inline uint32_t HashStr(const StringPiece& str) {
  if (str.empty()) {
    // Returning 0 for an empty string allows HashedStrView to be zero-initialized.
    return 0;
  }
  return MurmurHash2(str.data(), str.size());
}

namespace std {
  template<> struct hash<StringPiece> {
    size_t operator()(StringPiece key) const {
      return HashStr(key);
    }
  };
}

#endif  // NINJA_STRINGPIECE_H_
