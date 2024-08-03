#pragma once

#include "types.h"

namespace CTRPluginFramework::Script {

enum TokenKind {
  TOK_Int,
  TOK_Ident,
  TOK_Punctuater,
  TOK_End
};

struct Token {
  TokenKind kind;
  Token* prev;
  Token* next;

  char const* str;
  size_t len;

  Token(TokenKind kind, Token* prev, char const* str, size_t len)
    : kind(kind),
      prev(prev),
      next(nullptr),
      str(str),
      len(len)
  {
    if( prev )
      prev->next = this;
  }
};




} // namespace CTRPluginFramework::Script

