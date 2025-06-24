#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#define FOR_EACH_TOKEN_TYPE(V) \
  V("<=", LessEqual) \
  V("<", Less) \
  V("+", Plus) \
  V("-", Minus) \
  V("*", Times) \
  V("/", Divide) \
  V("^", Power) \
  V("(", LeftParen) \
  V(")", RightParen) \
  V(",", Comma) \
  V("<int>", Int) \
  V("<var>", Var)

enum TokenType {
  kInvalid = -1,
#define ENUM(name, value) k##value,
  FOR_EACH_TOKEN_TYPE(ENUM)
#undef ENUM
};

const char *token_name(enum TokenType type) {
  switch (type) {
#define STR(name, value) \
    case k##value: return name;
    FOR_EACH_TOKEN_TYPE(STR)
#undef STR
    default: return "Unknown";
  }
}

// Not NUL terminated.
struct Slice {
  const char *data;
  size_t length;
};

struct Slice slice(const char *data, size_t length) {
  return (struct Slice){.data = data, .length = length};
}

struct Token {
  enum TokenType type;
  union {
    int number;
    // For variable names
    struct Slice slice;
  };
};

struct TokenIterator {
  struct Token token;
  const char *data;
};

struct TokenIterator tokenizer_new(const char *input) { return (struct TokenIterator){{kInvalid}, input}; }
char tokenizer_peek(struct TokenIterator current) { return *current.data; }
bool tokenizer_at_end(struct TokenIterator current) { return tokenizer_peek(current) == '\0'; }
void tokenizer_advance(struct TokenIterator *current) { current->data++; }

struct TokenIterator tokenizer_next(struct TokenIterator current) {
try_again:
  if (tokenizer_at_end(current)) {
    return current;
  }
  char c = tokenizer_peek(current);
  tokenizer_advance(&current);
  if (c == ' ' || c == '\t' || c == '\n') {
    goto try_again;
  }
  if (c == '#') {
    // Skip comments until the end of the line.
    while (!tokenizer_at_end(current) && tokenizer_peek(current) != '\n') {
      tokenizer_advance(&current);
    }
    goto try_again;
  }
  if ('0' <= c && c <= '9') {
    int result = c - '0';
    while (!tokenizer_at_end(current) && 
           (c = tokenizer_peek(current)) >= '0' && c <= '9') {
      result = result * 10 + (c - '0');
      tokenizer_advance(&current);
    }
    current.token.type = kInt;
    current.token.number = result;
    return current;
  }
  if (c == '+') { current.token.type = kPlus; return current; }
  if (c == '-') { current.token.type = kMinus; return current; }
  if (c == '*') { current.token.type = kTimes; return current; }
  if (c == '/') { current.token.type = kDivide; return current; }
  if (c == '^') { current.token.type = kPower; return current; }
  if (c == '(') { current.token.type = kLeftParen; return current; }
  if (c == ')') { current.token.type = kRightParen; return current; }
  if (c == ',') { current.token.type = kComma; return current; }
  if (c == '<') {
    if (tokenizer_peek(current) == '=') {
      tokenizer_advance(&current);
      current.token.type = kLessEqual;
      return current;
    } else {
      current.token.type = kLess;
      return current;
    }
  }
  if (isalpha(c)) {
    const char *start = current.data - 1; // Include the current character
    size_t length = 1;
    while (!tokenizer_at_end(current) && isalpha(tokenizer_peek(current))) {
      tokenizer_advance(&current);
      length++;
    }
    current.token.type = kVar;
    current.token.slice = slice(start, length);
    return current;
  }
  fprintf(stderr, "Unexpected character '%c'\n", c);
  exit(EXIT_FAILURE);
}

#define FOR_EACH_AST_NODE_TYPE(V) \
  V(Int) \
  V(Var) \
  V(Add) \
  V(Sub) \
  V(Mul) \
  V(Div) \
  V(Pow) \
  V(Less) \
  V(LessEqual) \
  V(Negate) \
  V(Call)

enum ASTNodeType {
  kASTInvalid = -1,
#define ENUM(name) kAST##name,
  FOR_EACH_AST_NODE_TYPE(ENUM)
#undef ENUM
};

struct ASTNode {
  enum ASTNodeType type;
};

struct IntNode {
  struct ASTNode base;
  int value;
};

struct VarNode {
  struct ASTNode base;
  struct Slice name;
};

#define BINARY_OP_NODE(name) \
struct name##Node { \
  struct ASTNode base; \
  struct ASTNode *left; \
  struct ASTNode *right; \
};

BINARY_OP_NODE(Add)
BINARY_OP_NODE(Sub)
BINARY_OP_NODE(Mul)
BINARY_OP_NODE(Div)
BINARY_OP_NODE(Pow)
BINARY_OP_NODE(Less)
BINARY_OP_NODE(LessEqual)

#undef BINARY_OP_NODE

struct NegateNode {
  struct ASTNode base;
  struct ASTNode *operand;
};

struct ASTNode *ast_new_Int(int value) {
  struct IntNode *node = malloc(sizeof(struct IntNode));
  node->base.type = kASTInt;
  node->value = value;
  return (struct ASTNode *)node;
}

struct ASTNode *ast_new_Var(struct Slice name) {
  struct VarNode *node = malloc(sizeof(struct VarNode));
  node->base.type = kASTVar;
  node->name = name;
  return (struct ASTNode *)node;
}

#define BINARY_OP_NEW(name) \
struct ASTNode *ast_new_##name(struct ASTNode *left, struct ASTNode *right) { \
  struct name##Node *node = malloc(sizeof(struct name##Node)); \
  node->base.type = kAST##name; \
  node->left = left; \
  node->right = right; \
  return (struct ASTNode *)node; \
}

BINARY_OP_NEW(Add)
BINARY_OP_NEW(Sub)
BINARY_OP_NEW(Mul)
BINARY_OP_NEW(Div)
BINARY_OP_NEW(Pow)
BINARY_OP_NEW(Less)
BINARY_OP_NEW(LessEqual)

#undef BINARY_OP_NEW

struct ASTNode *ast_new_Negate(struct ASTNode *operand) {
  struct NegateNode *node = malloc(sizeof(struct NegateNode));
  node->base.type = kASTNegate;
  node->operand = operand;
  return (struct ASTNode *)node;
}

bool is_operator(enum TokenType type) {
  switch (type) {
    case kPlus:
    case kMinus:
    case kTimes:
    case kDivide:
    case kPower:
    case kLess:
    case kLessEqual:
      return true;
    default:
      return false;
  }
}

int operator_precedence(enum TokenType type) {
  int prec = 0;
  switch (type) {
    case kLess:
    case kLessEqual: return prec;
    prec++;
    case kPlus:
    case kMinus: return prec;
    prec++;
    case kTimes:
    case kDivide: return prec;
    prec++;
    case kPower: return prec;
    default: return -1; // Invalid precedence
  }
}

enum Associativity { kInvalidAssociativity = -1, kLeft, kRight, kAny, };

enum Associativity operator_associativity(enum TokenType type) {
  switch (type) {
    case kPlus:
    case kTimes:
      return kAny;
    case kMinus:
    case kDivide:
    case kLess:
    case kLessEqual:
      return kLeft;
    case kPower:
      return kRight;
    default:
      return kInvalidAssociativity;
  }
}

struct ASTNode *parse_atom(struct TokenIterator *iterator) {
  abort();
}

struct ASTNode *parse_expression(struct TokenIterator *iterator, int min_prec) {
  struct ASTNode *lhs = parse_atom(iterator);
  while (!tokenizer_at_end(*iterator) && is_operator(iterator->token.type)) {
    int prec = operator_precedence(iterator->token.type);
    if (prec < min_prec) {
      return lhs;
    }
    abort();
  }
  abort();
}

int main() {
  const char *input = "1+abc*3";
  struct TokenIterator iterator = tokenizer_new(input);
  while (!tokenizer_at_end(iterator)) {
    iterator = tokenizer_next(iterator);
    fprintf(stderr, "Token type: %s\n", token_name(iterator.token.type));
    if (iterator.token.type == kInt) {
      fprintf(stderr, "  : %d\n", iterator.token.number);
    } else if (iterator.token.type == kVar) {
      fprintf(stderr, "  : %.*s\n", (int)iterator.token.slice.length, iterator.token.slice.data);
    }
    // Process the token here (not implemented yet)
  }

  return 0;
}
