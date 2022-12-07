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

#include "lexer.h"

#include "eval_env.h"
#include "test.h"

TEST(Lexer, ReadVarValue) {
  Lexer lexer("plain text $var $VaR ${x}\nmore text");
  StringPiece uneval;
  std::string err;
  EXPECT_TRUE(lexer.ReadBindingValue(&uneval, &err));
  EXPECT_EQ("", err);
  EXPECT_EQ("plain text $var $VaR ${x}\n", uneval.AsString());
  std::string eval = EvaluateBindingForTesting(uneval);
  EXPECT_EQ("plain text [$var] [$VaR] [$x]", eval);
}

TEST(Lexer, ReadEvalStringEscapes) {
  Lexer lexer("$ $$ab c$: $\ncde\nmore text");
  StringPiece uneval;
  std::string err;
  EXPECT_TRUE(lexer.ReadBindingValue(&uneval, &err));
  EXPECT_EQ("", err);
  EXPECT_EQ("$ $$ab c$: $\ncde\n", uneval.AsString());
  std::string eval = EvaluateBindingForTesting(uneval);
  EXPECT_EQ(" $ab c: cde", eval);
}

TEST(Lexer, ReadIdent) {
  Lexer lexer("foo baR baz_123 foo-bar");
  StringPiece ident;
  EXPECT_TRUE(lexer.ReadIdent(&ident));
  EXPECT_EQ("foo", ident);
  EXPECT_TRUE(lexer.ReadIdent(&ident));
  EXPECT_EQ("baR", ident);
  EXPECT_TRUE(lexer.ReadIdent(&ident));
  EXPECT_EQ("baz_123", ident);
  EXPECT_TRUE(lexer.ReadIdent(&ident));
  EXPECT_EQ("foo-bar", ident);
}

TEST(Lexer, ReadIdentCurlies) {
  // Verify that ReadIdent includes dots in the name,
  // but in an expansion $bar.dots stops at the dot.
  Lexer lexer("foo.dots $bar.dots ${bar.dots}\n");
  StringPiece ident;
  EXPECT_TRUE(lexer.ReadIdent(&ident));
  EXPECT_EQ("foo.dots", ident);

  StringPiece uneval;
  string err;
  EXPECT_TRUE(lexer.ReadBindingValue(&uneval, &err));
  EXPECT_EQ("", err);
  EXPECT_EQ("$bar.dots ${bar.dots}\n", uneval.AsString());
  std::string eval = EvaluateBindingForTesting(uneval);
  EXPECT_EQ("[$bar].dots [$bar.dots]", eval);
}

TEST(Lexer, PeekCanonicalPath) {
  auto peek_canon = [](const char* pattern) {
    Lexer lexer(pattern);
    return lexer.PeekCanonicalPath().AsString();
  };
  EXPECT_EQ("", peek_canon(""));
  EXPECT_EQ("", peek_canon(" : "));
  EXPECT_EQ("", peek_canon("/ : "));
  EXPECT_EQ("", peek_canon("/foo/ : "));
  EXPECT_EQ("", peek_canon("/foo/../bar : "));
  EXPECT_EQ("", peek_canon("/../foo/bar : "));

  EXPECT_EQ("", peek_canon("foo$$bar : "));
  EXPECT_EQ("", peek_canon("foo${bar}baz : "));

  EXPECT_EQ("/foo", peek_canon("/foo : "));
  EXPECT_EQ("/foo/bar", peek_canon("/foo/bar : "));
  EXPECT_EQ("foo", peek_canon("foo : "));
  EXPECT_EQ("foo/bar", peek_canon("foo/bar : "));
  EXPECT_EQ("../foo/bar", peek_canon("../foo/bar : "));
  EXPECT_EQ("../../foo/bar", peek_canon("../../foo/bar : "));

  EXPECT_EQ("foo", peek_canon("./foo : "));
  EXPECT_EQ("../../foo/bar", peek_canon("./../../foo/bar : "));
}

TEST(Lexer, ReadPath) {
  Lexer lexer("x$$y$ z$:: $bar.dots\n");
  std::string err;
  LexedPath uneval;
  std::string eval;

  EXPECT_TRUE(lexer.ReadPath(&uneval, &err));
  EXPECT_EQ("", err);
  EXPECT_EQ("x$$y$ z$:", uneval.str_.AsString());
  eval = EvaluatePathForTesting(uneval);
  EXPECT_EQ("x$y z:", eval);

  EXPECT_EQ(Lexer::COLON, lexer.ReadToken());

  EXPECT_TRUE(lexer.ReadPath(&uneval, &err));
  EXPECT_EQ("", err);
  EXPECT_EQ("$bar.dots", uneval.str_.AsString());
  eval = EvaluatePathForTesting(uneval);
  EXPECT_EQ("[$bar].dots", eval);
}

TEST(Lexer, Error) {
  Lexer lexer("foo$\nbad $");
  StringPiece uneval;
  string err;
  ASSERT_FALSE(lexer.ReadBindingValue(&uneval, &err));
  EXPECT_EQ("input:2: bad $-escape (literal $ must be written as $$)\n"
            "bad $\n"
            "    ^ near here"
            , err);
}

TEST(Lexer, CommentEOF) {
  // Verify we don't run off the end of the string when the EOF is
  // mid-comment.
  Lexer lexer("# foo");
  Lexer::Token token = lexer.ReadToken();
  EXPECT_EQ(Lexer::ERROR, token);
}

TEST(Lexer, Tabs) {
  // Verify we print a useful error on a disallowed character.
  Lexer lexer("   \tfoobar");
  Lexer::Token token = lexer.ReadToken();
  EXPECT_EQ(Lexer::INDENT, token);
  token = lexer.ReadToken();
  EXPECT_EQ(Lexer::ERROR, token);
  EXPECT_EQ("tabs are not allowed, use spaces", lexer.DescribeLastError());
}

TEST(Lexer, AdvanceToNextManifestChunk) {
  auto advance = [](std::string input) -> std::string {
    // The input string may optionally have a '^' marking where the lexer should
    // start scanning for a chunk.
    size_t idx = input.find_first_of('^');
    if (idx == std::string::npos) {
      idx = 0;
    } else {
      input = input.substr(0, idx) + input.substr(idx + 1);
    }
    idx = AdvanceToNextManifestChunk(input, idx);
    // The return string has a '^' marking where the manifest was split.
    return input.substr(0, idx) + "^" + input.substr(idx);
  };

  EXPECT_EQ("^", advance(""));
  EXPECT_EQ("A\n^B\n", advance("A\nB\n"));
  EXPECT_EQ("\n^B\n", advance("\nB\n"));
  EXPECT_EQ("\0\0\0^"_s, advance("\0\0\0"_s));

  // An LF preceded by "$" or "$\r" might be part of a line continuator, so we
  // skip it when looking for a place to split the manifest.
  EXPECT_EQ("A$\nB\n^C", advance("^A$\nB\nC"));
  EXPECT_EQ("A$\nB\n^C", advance("A^$\nB\nC"));
  EXPECT_EQ("A$\nB\n^C", advance("A$^\nB\nC"));
  EXPECT_EQ("A$\r\nB\n^C", advance("^A$\r\nB\nC"));
  EXPECT_EQ("A$\r\nB\n^C", advance("A^$\r\nB\nC"));
  EXPECT_EQ("A$\r\nB\n^C", advance("A$^\r\nB\nC"));
  EXPECT_EQ("A$\r\nB\n^C", advance("A$\r^\nB\nC"));

  // Skip over blank lines and comment lines.
  EXPECT_EQ("\n^", advance("\n"));
  EXPECT_EQ("\n\n^A\n", advance("\n\nA\n"));
  EXPECT_EQ("\n \n^A", advance("\n \nA"));
  EXPECT_EQ("\n#\n^A", advance("\n#\nA"));
}
