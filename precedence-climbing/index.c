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
  V("<eof>", Eof) \
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
    return (struct TokenIterator){{kEof}};
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

#define FOR_EACH_BINARY_OP(V) \
  V(Plus) \
  V(Minus) \
  V(Times) \
  V(Divide) \
  V(Power) \
  V(Less) \
  V(LessEqual)

#define FOR_EACH_AST_NODE_TYPE(V) \
  V(Int) \
  V(Var) \
  V(Negate) \
  FOR_EACH_BINARY_OP(V) \
  V(Call)

enum ASTNodeType {
  kASTInvalid = -1,
#define ENUM(name) kAST##name,
  FOR_EACH_AST_NODE_TYPE(ENUM)
#undef ENUM
};

const char *ast_node_name(enum ASTNodeType type) {
  switch (type) {
#define STR(name) \
    case kAST##name: return #name;
    FOR_EACH_AST_NODE_TYPE(STR)
#undef STR
    default: return "Unknown";
  }
}

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

struct BinaryOpNode {
  struct ASTNode base;
  struct ASTNode *left;
  struct ASTNode *right;
};

struct UnaryOpNode {
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

struct ASTNode *ast_new_binary_op(enum ASTNodeType type, struct ASTNode *left, struct ASTNode *right) {
  struct BinaryOpNode *node = malloc(sizeof(struct BinaryOpNode));
  node->base.type = type;
  node->left = left;
  node->right = right;
  return (struct ASTNode *)node;
}

// #define NEW_FUNC(name) \
// struct ASTNode *ast_new_##name(struct ASTNode *left, struct ASTNode *right) { \
//   return ast_new_binary_op(kAST##name, left, right); \
// }
// 
// FOR_EACH_BINARY_OP(NEW_FUNC)
// 
// #undef NEW_FUNC

struct ASTNode *ast_new_Negate(struct ASTNode *operand) {
  struct UnaryOpNode *node = malloc(sizeof(struct UnaryOpNode));
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
    case kLessEqual:
      return prec;
    default: prec++;
  }
  switch (type) {
    case kPlus:
    case kMinus:
      return prec;
    default: prec++;
  }
  switch (type) {
    case kTimes:
    case kDivide:
      return prec;
    default: prec++;
  }
  switch (type) {
    case kPower:
      return prec;
    default: prec++;
  }
  fprintf(stderr, "Invalid operator type %s\n", token_name(type));
  exit(EXIT_FAILURE);
}

enum Associativity { kLeft, kRight, kAny, };

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
    default: {
      fprintf(stderr, "Invalid operator type %s\n", token_name(type));
      exit(EXIT_FAILURE);
    }
  }
}

bool parser_at_end(struct TokenIterator current) { return current.token.type == kEof; }

struct ASTNode *parse_atom(struct TokenIterator *iterator) {
  if (parser_at_end(*iterator)) {
    fprintf(stderr, "Unexpected end of input\n");
    exit(EXIT_FAILURE);
  }
  switch (iterator->token.type) {
    case kInt: {
      struct ASTNode *result = ast_new_Int(iterator->token.number);
      *iterator = tokenizer_next(*iterator);
      return result;
    }
    case kVar: {
      struct ASTNode *result = ast_new_Var(iterator->token.slice);
      *iterator = tokenizer_next(*iterator);
      return result;
    }
    // TODO(max): Parse negation
    // TODO(max): Parse parentheses
    default:
      fprintf(stderr, "Unexpected token %s\n", token_name(iterator->token.type));
      exit(EXIT_FAILURE);
  }
}

struct ASTNode *parse_expression(struct TokenIterator *iterator, int min_prec) {
  struct ASTNode *lhs = parse_atom(iterator);
  struct Token token;
  while (!parser_at_end(*iterator) && is_operator((token = iterator->token).type)) {
    int op_prec = operator_precedence(token.type);
    if (op_prec < min_prec) {
      return lhs;
    }
    *iterator = tokenizer_next(*iterator);
    int next_prec = operator_associativity(token.type) == kLeft ? op_prec + 1 : op_prec;
    struct ASTNode *rhs = parse_expression(iterator, next_prec);
    switch (token.type) {
#define CALL_NEW(name) case k##name: lhs = ast_new_binary_op(kAST##name, lhs, rhs); break;
      FOR_EACH_BINARY_OP(CALL_NEW)
#undef CALL_NEW
      // TODO(max): Parse function calls
      default:
        fprintf(stderr, "Unhandled operator %s\n", token_name(token.type));
        exit(EXIT_FAILURE);
    }
  }
  return lhs;
}

int main() {
  const char *input = "1+2*3";
  struct TokenIterator iterator = tokenizer_new(input);
  iterator = tokenizer_next(iterator);  // prime the iterator
  struct ASTNode *ast = parse_expression(&iterator, 0);
  if (!parser_at_end(iterator)) {
    fprintf(stderr, "Unexpected token after expression: %s\n", token_name(iterator.token.type));
    exit(EXIT_FAILURE);
  }
  if (ast->type != kASTPlus) {
    fprintf(stderr, "Expected an addition operation at the root, got %s\n", ast_node_name(ast->type));
    exit(EXIT_FAILURE);
  }
  struct BinaryOpNode *root = (struct BinaryOpNode *)ast;
  struct ASTNode *left = root->left;
  if (left->type != kASTInt) {
    fprintf(stderr, "Expected an integer on the left side, got %s\n", ast_node_name(left->type));
    exit(EXIT_FAILURE);
  }
  struct ASTNode *right = root->right;
  if (right->type != kASTTimes) {
    fprintf(stderr, "Expected a binary operation on the right side, got %s\n", ast_node_name(right->type));
    exit(EXIT_FAILURE);
  }
  return 0;
}
