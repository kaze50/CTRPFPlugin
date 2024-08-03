#include <iostream>
#include <string>
#include <vector>
#include <utility>
#include <algorithm>


//
// TokenKind: kinds of token.
//
enum TokenKind {
  TOK_VALUE,    // immidiate (digits, literals, ...)
  TOK_IDENT,    // identifier
  TOK_KEYWORD,  // keywords
  TOK_PUNCT,    // punctuater
  TOK_END,      // end of file
};

//
// KeywordKind: kinds of keywords.
//
enum KeywordKind {
  KW_NONE, // not kwd

  //
  // control statements
  //
  KW_IF,
  KW_ELSE,
  KW_FOR,
  KW_LOOP,
  KW_DO,
  KW_WHILE,
  KW_SWITCH,

  //
  // variable declaration
  KW_VAR,

  //
  // function definition
  KW_FUNC,
};


static const std::pair<KeywordKind, char const*> d_kwd_kind_table[] {
  { KW_NONE,      ""        },
  { KW_IF,        "if"      },
  { KW_ELSE,      "else"    },
  { KW_FOR,       "for"     },
  { KW_LOOP,      "loop"    },
  { KW_DO,        "do"      },
  { KW_WHILE,     "while"   },
  { KW_SWITCH,    "switch"  },
  { KW_VAR,       "var"     },
  { KW_FUNC,      "func"    },
};


struct Object;
struct Token {
  TokenKind kind;
  KeywordKind kwd;
  size_t position;
  std::string_view str;
  Object* object;     // when TOK_VALUE

  Token(TokenKind kind, size_t pos, std::string_view s = "", Object* object = nullptr)
    : kind(kind),
      position(pos),
      str(s),
      object(object)
  {
  }
};


//
// TypeKind: kinds of type-info of object.
//
enum TypeKind : uint8_t {
  TYPE_None,
  TYPE_Int,
  TYPE_Float,
  TYPE_Bool,
  TYPE_Char,
  TYPE_Pointer,
  TYPE_String,
  TYPE_List,
};

//
// オブジェクト
//
struct __attribute__((__packed__)) Object {
private:
  TypeKind  kind;

public:
  
  // 3DS での動作を想定しています
  // 実機ではポインタも 32bit です ----v
  union { //                           v
    int32_t       v_int;          // <-- よって、v_int を 0 にすれば union の領域すべてゼロ初期化できるはず。
    float         v_float;
    bool          v_bool;
    char16_t      v_char;
    void*         v_pointer;

    // この二つのポインタは必ず
    // [ctor, dtor, TypeKind を変更] のタイミングで確保と解放を行う
    std::u16string*       v_str_p;    // when TYPE_String
    std::vector<Object>*  v_list_p;   // when TYPE_List
  };

  //
  // Getter of kind
  //
  TypeKind get_kind() const {
    return this->kind;
  }

  //
  // Setter of kind
  //
  TypeKind set_kind(TypeKind kind) {
    if( this->kind == kind )
      return kind;

    if( this->kind == TYPE_String )
      delete this->v_str_p;
    else if( this->kind == TYPE_List )
      delete this->v_list_p;

    switch( kind ) {
      case TYPE_String:
        this->v_str_p = new std::u16string();
        break;
      
      case TYPE_List:
        this->v_list_p = new std::vector<Object>();
        break;
    }

    return this->kind = kind;
  }

  #define  _obj_creator(Name, T, K, V)  \
    static Object new_ ## Name (T val) { \
      Object obj{ K }; obj.V = val; return obj; \
    }

  _obj_creator(int, int32_t, TYPE_Int, v_int);
  _obj_creator(float, float, TYPE_Float, v_float);
  _obj_creator(bool, bool, TYPE_Bool, v_bool);
  _obj_creator(char, char, TYPE_Char, v_char);

  static Object new_string(std::u16string str) {
    Object obj { TYPE_String };
    *obj.v_str_p = std::move(str);
    return obj;
  }

  static Object new_list(std::vector<Object> list) {
    Object obj { TYPE_List };
    *obj.v_list_p = std::move(list);
    return obj;
  }

  //
  // コンストラクタ
  Object(TypeKind kind)
    : kind(TYPE_None),
      v_int(0)
  {
    // 開発環境 x64 なのでゼロ初期化を完全に行う
    this->v_pointer = nullptr;

    this->set_kind(kind);
  }

  //
  // デストラクタ
  ~Object()
  {
    switch( kind ) {
      case TYPE_String:
        delete this->v_str_p;
        break;

      case TYPE_List:
        delete this->v_list_p;
        break;
    }
  }
};


enum NodeKind {
  ND_VALUE,
  ND_VARIABLE,

  ND_ADD,
  ND_SUB,
  ND_MUL,
  ND_DIV,

  ND_IF,
  ND_FOR,

  ND_FUNC,
};


#define  _NC      child
#define  nd_lhs   _NC[0]
#define  nd_rhs   _NC[1]

struct Node {
  NodeKind  kind;
  Token*    token;
  Object*   value;
  std::vector<Node>  child;


  Node(NodeKind kind, std::vector<Node> child)
    : kind(kind),
      token(nullptr),
      value(nullptr),
      child(std::move(child))
  {
  }
};

class Parser {
public:

  Parser(std::vector<Token>& tokenlist)
    : tokenlist(tokenlist),
      current(this->tokenlist.data()),
      end(this->tokenlist.data() + tokenlist.size())
  {
  }

  bool check() const {
    return this->current != this->end;
  }

  bool eat(std::string_view str) {
    if( this->check() && this->current->str == str ) {
      this->current++;
      return true;
    }

    return false;
  }

  bool eat(TokenKind kind) {
    if( this->check() && this->current->kind == kind ) {
      this->current++;
      return true;
    }

    return false;
  }
  
  bool eat(KeywordKind kind) {
    if( this->check() && this->current->kind == TOK_KEYWORD && this->current->kwd == kind ) {
      this->current++;
      return true;
    }

    return false;
  }

  template <class T, class... Args>
  bool eat(T&& t, Args&&... args) {
    if( this->eat(t) ) {
      return this->eat(args...);
    }

    return false;
  }




  Node prs_factor() {

  }


private:

  std::vector<Token> const& tokenlist;
  Token const*  current;
  Token const*  end;

};



int main() {

  static constexpr auto source =
R"(
1 + 2 * 3
)";


  Object obj{ TYPE_List };

  for(int i=0;i<10;i++){
    obj.v_list_p->emplace_back(Object::new_int(i));
  }




  std::cout << "Hello!\n";


}

