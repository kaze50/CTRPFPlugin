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
    static Object from_ ## Name (T val) { \
      Object obj{ K }; obj.V = val; return obj; \
    }

  _obj_creator(int, int32_t, TYPE_Int, v_int);
  _obj_creator(float, float, TYPE_Float, v_float);
  _obj_creator(bool, bool, TYPE_Bool, v_bool);
  _obj_creator(char, char, TYPE_Char, v_char);

  static Object from_string(std::u16string str) {
    Object obj { TYPE_String };
    *obj.v_str_p = std::move(str);
    return obj;
  }

  static Object from_list(std::vector<Object> list) {
    Object obj { TYPE_List };
    *obj.v_list_p = std::move(list);
    return obj;
  }

  //
  // コンストラクタ
  Object(TypeKind kind = TYPE_None)
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


struct Token {
  TokenKind         kind;
  KeywordKind       kwd;
  size_t            position;
  std::string_view  str;
  Object            object;     // when TOK_VALUE

  Token(TokenKind kind, size_t pos, std::string_view s = "", Object object = { }, KeywordKind kwd = KW_NONE)
    : kind(kind),
      kwd(kwd),
      position(pos),
      str(s),
      object(std::move(object))
  {
  }


  //
  // デバッグ用関数 ( ソースコード上で作成できるようにするため )
  //
  static Token from_value(Object&& obj) {
    return Token(TOK_VALUE, 0, "", obj);
  }

  static Token from_str(std::string_view s, TokenKind kind = TOK_IDENT, KeywordKind kwd = KW_NONE) {
    return Token(kind, 0, s, { }, kwd);
  }


};



#define  _NC      child
#define  nd_lhs   _NC[0]
#define  nd_rhs   _NC[1]

struct Node {
  NodeKind        kind;
  Token const*    token;
  Object const*   value; // ONLY SET POINTER TO OBJECT CONTAINED BY TOKEN!!!!
  std::vector<Node>  child;


  Node(NodeKind kind, Token const* token, Object const* value, std::vector<Node> child = { })
    : kind(kind),
      token(token),
      value(value),
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

  template <class T, class U, class... Args>
  bool eat(T&& t, U&& u, Args&&... args) {
    if( this->eat(t) ) {
      return this->eat(u, args...);
    }

    return false;
  }


  Node prs_factor() {
    this->save();

    if( this->eat(TOK_VALUE) )
      return Node(ND_VALUE, this->temp, &this->temp->object);

    printf("syntax err\n");
    exit(1);
  }

  Node prs_term() {
    auto x = this->prs_factor();

    while( this->check() ) {
      this->save();

      if( this->eat("*") )
        x = Node{ ND_MUL, this->temp, nullptr, { x, this->prs_factor() } };
      else if( this->eat("/") )
        x = Node{ ND_DIV, this->temp, nullptr, { x, this->prs_factor() } };
      else
        break;
    }

    return x;
  }

  Node parse() {
    return this->prs_term();
  }


private:

  void save() {
    this->temp = this->current;
  }

  std::vector<Token> const& tokenlist;
  Token const*  current;
  Token const*  end;
  Token const*  temp; // save current pointer temporary

};



int main() {

  static constexpr auto source =
R"(
1 + 2 * 3
)";


  std::cout<<"Hello!\n";


  std::vector<Token> tokenlist {
    Token::from_value(Object::from_int(1)),
    Token::from_str("*", TOK_PUNCT),
    Token::from_value(Object::from_int(2)),
    Token::from_str("*", TOK_PUNCT),
    Token::from_value(Object::from_int(3)),
  };


  Parser parser{ tokenlist };

  auto node = parser.parse();





}

