#include <iostream>
#include <string>
#include <vector>
#include <utility>
#include <algorithm>
#include <cassert>

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
  TYPE_NONE,
  TYPE_INT,
  TYPE_FLOAT,
  TYPE_BOOL,
  TYPE_CHAR,
  TYPE_POINTER,
  TYPE_STRING,
  TYPE_LIST,

  // 16 個まで！！
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
  TypeKind  kind : 8;


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
    // コピーコンストラクタの場合は必ず複製する
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

    if( this->kind == TYPE_STRING )
      delete this->v_str_p;
    else if( this->kind == TYPE_LIST )
      delete this->v_list_p;

    switch( kind ) {
      case TYPE_STRING:
        this->v_str_p = new std::u16string();
        break;
      
      case TYPE_LIST:
        this->v_list_p = new std::vector<Object>();
        break;
    }

    return this->kind = kind;
  }

  #define  _obj_creator(Name, T, K, V)  \
    static Object from_ ## Name (T val) { \
      Object obj{ K }; obj.V = val; return obj; \
    }

  _obj_creator(int, int32_t, TYPE_INT, v_int);
  _obj_creator(float, float, TYPE_FLOAT, v_float);
  _obj_creator(bool, bool, TYPE_BOOL, v_bool);
  _obj_creator(char, char, TYPE_CHAR, v_char);

  static Object from_string(std::u16string str) {
    Object obj { TYPE_STRING };
    *obj.v_str_p = std::move(str);
    return obj;
  }

  static Object from_list(std::vector<Object> list) {
    Object obj { TYPE_LIST };
    *obj.v_list_p = std::move(list);
    return obj;
  }

  bool is_numeric() const {
    switch( this->kind ) {
      case TYPE_INT:
      case TYPE_FLOAT:
        return true;
    }

    return false;
  }

  //
  // コンストラクタ
  Object(TypeKind kind = TYPE_NONE)
    : kind(TYPE_NONE), // <= use TYPE_NONE. this is not wrong.
      v_int(0)
  {
    // 開発環境 x64 なのでゼロ初期化を完全に行う
    this->v_pointer = nullptr;

    // 必要に応じてメモリを確保
    this->set_kind(kind);
  }

  Object(Object&&) = default;

  Object(Object const& obj)
    : kind(obj.kind),
      v_pointer(obj.v_pointer)
  {
    if( kind == TYPE_STRING )
      this->v_str_p = new std::u16string(*obj.v_str_p);
    else if( kind == TYPE_LIST )
      this->v_list_p = new std::vector<Object>(*obj.v_list_p);
  }

  //
  // デストラクタ
  ~Object()
  {
    switch( kind ) {
      case TYPE_STRING:
        delete this->v_str_p;
        break;

      case TYPE_LIST:
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



//
// preprocessers to access to childlen node of node

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

  Parser(std::vector<Token> const& tokenlist)
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

  Node prs_expr() {
    auto x = this->prs_term();

    while( this->check() ) {
      this->save();

      if( this->eat("+") )
        x = Node{ ND_ADD, this->temp, nullptr, { x, this->prs_term() } };
      else if( this->eat("-") )
        x = Node{ ND_SUB, this->temp, nullptr, { x, this->prs_term() } };
      else
        break;
    }

    return x;
  }

  Node parse() {
    return this->prs_expr();
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

namespace interp_exception {

class runtime_error {
public:

  Node const* node;
  std::string  message;


};


} // namespace interp_exception

//
// === Evaluator ===
//
// evaluate node and return an object as result.
//
class Evaluator {

  // 
  // nd_ptr_keeper_t:
  //   restore the pointer 'cur_node' before returning from eval()
  struct nd_ptr_keeper_t {
    Evaluator& E;
    Node const* nd;

    ~nd_ptr_keeper_t() { E.cur_node = nd; }
  };
  
  //
  // Current evaluating node in eval()
  Node const* cur_node;


public:

  static Object obj_add(Object a, Object const& b) {
  
    // if( !a.is_numeric() || !b.is_numeric() )
    
    a.v_int += b.v_int;

    return a;
  }

  static Object obj_mul(Object a, Object const& b) {
  
    if( !a.is_numeric() || !b.is_numeric() )
      throw 123;
    
    a.v_int *= b.v_int;

    return a;
  }

  Object eval(Node const& node) {

    // 関数から戻るとき cur_node を前の値に戻す
    nd_ptr_keeper_t
      _instance{ *this, this->cur_node };

    this->cur_node = &node;

    switch( node.kind ) {
      case ND_ADD:
        return obj_add(this->eval(node.nd_lhs), this->eval(node.nd_rhs));

      case ND_MUL:
        return obj_mul(this->eval(node.nd_lhs), this->eval(node.nd_rhs));

      default:
        break;
    }

    assert(node.kind == ND_VALUE);
    return node.token->object;
  }


};


class Lexer {

  static constexpr char const* punctuaters[] {
    "<<=",
    ">>=",
    "->",
    "<<",
    ">>",
    "<=",
    ">=",
    "==",
    "!=",
    "..",
    "&&",
    "||",
    "<",
    ">",
    "+",
    "-",
    "/",
    "*",
    "%",
    "=",
    ";",
    ":",
    ",",
    ".",
    "[",
    "]",
    "(",
    ")",
    "{",
    "}",
    "!",
    "?",
    "&",
    "^",
    "|",
  };

  std::string const& source;
  size_t position;

  bool check() const {
    return this->position < this->source.length();
  }

  char peek() const {
    return this->source[this->position];
  }

  void pass_space() {
    while( this->check() && isspace(this->peek()) )
      this->position++;
  }

  bool match(std::string_view s) const {
    return this->position + s.length() <= this->source.length()
      && this->source.substr(this->position, s.length()) == s;
  }

  //
  // get_str:
  //  現在位置から "N <= peek() <= M" の条件に合う限り進め、文字列を返す
  //
  std::string_view get_str(std::initializer_list<std::pair<char, char>> ranges) {
    size_t pos = this->position;

    while( this->check() ) {
      char c = this->peek();

      for( auto&& [n, m] : ranges )
        if( n <= c && c <= m ) goto __l_continue;

      break;

    __l_continue:
      this->position++;
    }

    return { this->source.data() + pos, this->position - pos };
  }

public:

  
  Lexer(std::string const& source) // 参照を渡す
    : source(source),
      position(0)
  {
  }

  std::vector<Token>  lex() {
    std::vector<Token>  vec;

    this->pass_space();

    while( this->check() ) {
      char c = this->peek();
      char const* str = this->source.data() + this->position;
      size_t pos = this->position;

      if( isdigit(c) ) {
        vec.emplace_back(Token::from_value(Object::from_int(std::atoi(str))))
              .str = this->get_str({{'0', '9'}});
      }

      else {
        for( std::string_view pu : punctuaters ) {
          if( this->match(pu) ) {
            vec.emplace_back(Token::from_str(pu, TOK_PUNCT));
            this->position += pu.length();
            goto _lex_pu_cnt;
          }
        }

        printf("invalid token\n");
        exit(1);

      _lex_pu_cnt:;
      }

      this->pass_space();
    }

    return vec;
  }

};


int main() {


  std::cout<<"Hello!\n";

  // try {


  static const std::string source =
R"(
1 + 2 * 3
)";

    //
    // tokenlist はアプリケーションの最後まで保持されている必要があります

    auto const tokenlist = Lexer(source).lex();


    Parser parser{ tokenlist };

    auto node = parser.parse();

    Evaluator evaluator;

    auto obj = evaluator.eval(node);

    std::cout<<obj.v_int<<std::endl;


    return 0;

  // }
  // catch( ... ) {
  //   std::cout << "exception!\n";
  // }

  return -1;
}

