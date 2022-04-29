// Legacy parser from the 90's

#include <ctype.h>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "src/core.h"
#include "src/mt.h"

namespace cottontail {

namespace {

typedef addr gcl_position;
#define GCL_EPSILON 1
#define GCL_MAX_POSITION (maxfinity)
#define GCL_MIN_POSITION 0
#define GCL_PLUS_INFINITY (maxfinity)
#define GCL_MINUS_INFINITY (minfinity)

enum gcl_qnodeType {
  GCL_CONTAINING,
  GCL_CONTAINED_IN,
  GCL_NOT_CONTAINING,
  GCL_NOT_CONTAINED_IN,
  GCL_BOTH_OF,
  GCL_ONE_OF,
  GCL_BEFORE,
  GCL_NOFM,
  GCL_NOFM_ELEMENT,
  GCL_TERM,
  GCL_FIXED,
  GCL_PARAMETER,
  GCL_NULL
};

typedef struct gcl_qnodeStructure {
  enum gcl_qnodeType type;
  gcl_position u, v, uNext, vNext;
  union {
    struct {
      struct gcl_qnodeStructure *left, *right;
    } a;
    struct {
      struct gcl_qnodeStructure *elements;
      unsigned n;
    } b;
    struct {
      struct gcl_qnodeStructure *element, *next;
    } c;
    struct {
      void *term;
    } d;
  } un;
} * gcl_q;

#define GCL_NULL_Q ((gcl_q)0)

#define GCL_Q_TYPE(q) ((q)->type)
#define GCL_Q_U(q) ((q)->u)
#define GCL_Q_V(q) ((q)->v)
#define GCL_Q_U_NEXT(q) ((q)->uNext)
#define GCL_Q_V_NEXT(q) ((q)->vNext)
#define GCL_Q_RIGHT(q) ((q)->un.a.right)
#define GCL_Q_LEFT(q) ((q)->un.a.left)
#define GCL_Q_N(q) ((q)->un.b.n)
#define GCL_Q_ELEMENTS(q) ((q)->un.b.elements)
#define GCL_Q_ELEMENT(q) ((q)->un.c.element)
#define GCL_Q_NEXT(q) ((q)->un.c.next)
#define GCL_Q_TERM(q) ((q)->un.d.term)

} // namespace

typedef struct gcl_envStructure {
  struct gcl_envStructure *next;
  char *name;
  gcl_q q;
} * gcl_env;

namespace {

#define GCL_NULL_ENV ((gcl_env)0)

thread_local struct gcl_qnodeStructure gcl_eofNode = {GCL_NULL};
thread_local struct gcl_qnodeStructure gcl_syntaxErrorNode = {GCL_NULL};
thread_local gcl_q freeQList = GCL_NULL_Q;
thread_local std::string gcl_syntaxError;

#define GCL_EOF (&gcl_eofNode)
#define GCL_SYNTAX_ERROR (&gcl_syntaxErrorNode)

void gcl_freeQ(gcl_q q);
void gcl_freeEnv(gcl_env env);
gcl_q gcl_copyQ(gcl_q q);
gcl_q gcl_createQFromString(char *s, gcl_env *env);

#define YES 1
#define NO 0

typedef struct cgrepStructure {
  char *regexp;
  FILE *fp;
} * cgrep;

void *term_create(char *str) {
  cgrep c;

  if ((c = (cgrep)malloc(sizeof(struct cgrepStructure))))
    if ((c->regexp = strdup(str))) {
      c->fp = NULL;
      return (void *)c;
    } else {
      free(c);
      return (void *)0;
    }
  else
    return (void *)0;
}

void term_free(void *term) {
  cgrep c = (cgrep)term;

  free(c->regexp);
  if (c->fp != NULL && c->fp != stdin)
    pclose(c->fp);
  free(c);
}

void *term_copy(void *term) { return term_create(((cgrep)term)->regexp); }

std::string term_to_string(void *term) {
  return std::string(((cgrep)term)->regexp);
}

gcl_q allocQNode(void) {
  gcl_q q;

  if (freeQList == GCL_NULL_Q)
    q = (gcl_q)malloc(sizeof(struct gcl_qnodeStructure));
  else {
    q = freeQList;
    freeQList = GCL_Q_NEXT(q);
  }
  GCL_Q_TYPE(q) = GCL_NULL;
  GCL_Q_U(q) = GCL_MINUS_INFINITY;
  GCL_Q_V(q) = GCL_MINUS_INFINITY;
  GCL_Q_U_NEXT(q) = GCL_MINUS_INFINITY;
  GCL_Q_V_NEXT(q) = GCL_MINUS_INFINITY;

  return q;
}

void freeQNode(gcl_q q) {
  GCL_Q_NEXT(q) = freeQList;
  freeQList = q;
}

gcl_q allocBinaryQ(enum gcl_qnodeType type, gcl_q left, gcl_q right) {
  gcl_q q = allocQNode();

  GCL_Q_TYPE(q) = type;
  GCL_Q_LEFT(q) = left;
  GCL_Q_RIGHT(q) = right;

  return q;
}

gcl_q allocNOfMQ(unsigned n, gcl_q elements) {
  gcl_q q = allocQNode();

  GCL_Q_TYPE(q) = GCL_NOFM;
  GCL_Q_N(q) = n;
  GCL_Q_ELEMENTS(q) = elements;

  return q;
}

gcl_q allocNOfMElement(gcl_q element, gcl_q next) {
  gcl_q q = allocQNode();

  GCL_Q_TYPE(q) = GCL_NOFM_ELEMENT;
  GCL_Q_ELEMENT(q) = element;
  GCL_Q_NEXT(q) = next;

  return q;
}

gcl_q allocTermQ(void *term) {
  gcl_q q = allocQNode();

  GCL_Q_TYPE(q) = GCL_TERM;
  GCL_Q_TERM(q) = term;

  return q;
}

gcl_q allocFixedQ(unsigned n) {
  gcl_q q = allocQNode();

  GCL_Q_TYPE(q) = GCL_FIXED;
  GCL_Q_N(q) = n;

  return q;
}

gcl_q allocParameterQ(unsigned n) {
  gcl_q q = allocQNode();

  GCL_Q_TYPE(q) = GCL_PARAMETER;
  GCL_Q_N(q) = n;

  return q;
}

void gcl_freeQ(gcl_q q) {
  if (q == GCL_NULL_Q)
    return;

  switch (GCL_Q_TYPE(q)) {
  case GCL_CONTAINING:
  case GCL_CONTAINED_IN:
  case GCL_NOT_CONTAINING:
  case GCL_NOT_CONTAINED_IN:
  case GCL_BOTH_OF:
  case GCL_ONE_OF:
  case GCL_BEFORE:
    gcl_freeQ(GCL_Q_LEFT(q));
    gcl_freeQ(GCL_Q_RIGHT(q));
    break;
  case GCL_NOFM:
    gcl_freeQ(GCL_Q_ELEMENTS(q));
    break;
  case GCL_NOFM_ELEMENT:
    gcl_freeQ(GCL_Q_ELEMENT(q));
    gcl_freeQ(GCL_Q_NEXT(q));
    break;
  case GCL_TERM:
    term_free(GCL_Q_TERM(q));
    break;
  case GCL_FIXED:
  case GCL_PARAMETER:
    break;
  case GCL_NULL:
    if (q == GCL_EOF || q == GCL_SYNTAX_ERROR)
      return;
    break;
  }

  freeQNode(q);
}

gcl_q gcl_copyQ(gcl_q q) {
  if (q == GCL_NULL_Q)
    return GCL_NULL_Q;

  switch (GCL_Q_TYPE(q)) {
  case GCL_CONTAINING:
  case GCL_CONTAINED_IN:
  case GCL_NOT_CONTAINING:
  case GCL_NOT_CONTAINED_IN:
  case GCL_BOTH_OF:
  case GCL_ONE_OF:
  case GCL_BEFORE:
    return allocBinaryQ(GCL_Q_TYPE(q), gcl_copyQ(GCL_Q_LEFT(q)),
                        gcl_copyQ(GCL_Q_RIGHT(q)));
  case GCL_NOFM:
    return allocNOfMQ(GCL_Q_N(q), gcl_copyQ(GCL_Q_ELEMENTS(q)));
  case GCL_NOFM_ELEMENT:
    return allocNOfMElement(gcl_copyQ(GCL_Q_ELEMENT(q)),
                            gcl_copyQ(GCL_Q_NEXT(q)));
  case GCL_TERM:
    return allocTermQ(term_copy(GCL_Q_TERM(q)));
    break;
  case GCL_FIXED:
    return allocFixedQ(GCL_Q_N(q));
  case GCL_PARAMETER:
    return allocParameterQ(GCL_Q_N(q));
  case GCL_NULL:
    if (q == GCL_EOF || q == GCL_SYNTAX_ERROR)
      return q;
    else
      return allocQNode();
  }
  return GCL_NULL_Q;
}

std::string to_s_expression(gcl_q q) {
  if (q == GCL_NULL_Q)
    return "";

  switch (GCL_Q_TYPE(q)) {
  case GCL_CONTAINING:
    return "(>> " + to_s_expression(GCL_Q_LEFT(q)) + " " +
           to_s_expression(GCL_Q_RIGHT(q)) + ")";
  case GCL_CONTAINED_IN:
    return "(<< " + to_s_expression(GCL_Q_LEFT(q)) + " " +
           to_s_expression(GCL_Q_RIGHT(q)) + ")";
  case GCL_NOT_CONTAINING:
    return "(!> " + to_s_expression(GCL_Q_LEFT(q)) + " " +
           to_s_expression(GCL_Q_RIGHT(q)) + ")";
  case GCL_NOT_CONTAINED_IN:
    return "(!< " + to_s_expression(GCL_Q_LEFT(q)) + " " +
           to_s_expression(GCL_Q_RIGHT(q)) + ")";
  case GCL_BOTH_OF:
    return "(^ " + to_s_expression(GCL_Q_LEFT(q)) + " " +
           to_s_expression(GCL_Q_RIGHT(q)) + ")";
  case GCL_ONE_OF:
    return "(+ " + to_s_expression(GCL_Q_LEFT(q)) + " " +
           to_s_expression(GCL_Q_RIGHT(q)) + ")";
  case GCL_BEFORE:
    return "(... " + to_s_expression(GCL_Q_LEFT(q)) + " " +
           to_s_expression(GCL_Q_RIGHT(q)) + ")";
  case GCL_NOFM: {
    if (GCL_Q_N(q) == 0)
      return "(# 1)";
    if (GCL_Q_N(q) == 1)
      return "(+" + to_s_expression(GCL_Q_ELEMENTS(q)) + ")";
    unsigned i = 0;
    for (gcl_q q0 = GCL_Q_ELEMENTS(q); q0 != GCL_NULL_Q; q0 = GCL_Q_NEXT(q0))
      i++;
    if (GCL_Q_N(q) == i)
      return "(^" + to_s_expression(GCL_Q_ELEMENTS(q)) + ")";
    if (GCL_Q_N(q) < i)
      return "(" + std::to_string(GCL_Q_N(q)) +
             to_s_expression(GCL_Q_ELEMENTS(q)) + ")";
    return "";
  }
  case GCL_NOFM_ELEMENT:
    return " " + to_s_expression(GCL_Q_ELEMENT(q)) +
           to_s_expression(GCL_Q_NEXT(q));
  case GCL_TERM:
    return "\"" + term_to_string(GCL_Q_TERM(q)) + "\"";
  case GCL_FIXED:
    return "(# " + std::to_string(GCL_Q_N(q)) + ")";
  case GCL_PARAMETER:
  case GCL_NULL:
  default:
    return "";
  }
}

thread_local char *ssNext, *ssToken, ssSaved;
thread_local int ssTokenIsInteger, ssTokenIsIdentifier;
thread_local unsigned ssTokenAsInteger;

#define token() (ssToken)
#define look(token) (strcmp(ssToken, (token)) == 0)
#define tokenIsInteger() (ssTokenIsInteger)
#define tokenIsIdentifier() (ssTokenIsIdentifier)
#define tokenAsInteger() (ssTokenAsInteger)

#define IS_SPACE(c) ((c) == ' ' || (c) == '\t')
#define IS_ALPHA(c) (isalpha(c))
#define IS_DIGIT(c) (isdigit(c))
#define IS_ALNUM(c) (isalnum(c))
#define IS_MULTI(c) ((c) == '>' || (c) == '<' || (c) == '/' || (c) == '.')
#define DIGIT_VALUE(c) ((c) - '0')
#define MAX_UNSIGNED ((unsigned)~0)
#define MAX_CVF ((MAX_UNSIGNED - 9) / 10)

char *scan() {
  *ssNext = ssSaved;
  while (IS_SPACE(*ssNext))
    ssNext++;

  ssToken = ssNext;

  ssTokenIsInteger = 0;
  ssTokenIsIdentifier = 0;

  if (IS_DIGIT(*ssNext)) {
    ssTokenIsInteger = 1;
    ssTokenAsInteger = 0;

    do {
      if (ssTokenAsInteger <= MAX_CVF)
        ssTokenAsInteger = 10 * ssTokenAsInteger + DIGIT_VALUE(*ssNext);
      else
        ssTokenAsInteger = MAX_UNSIGNED;
      ssNext++;
    } while (IS_DIGIT(*ssNext));
  } else {
    if (IS_ALPHA(*ssNext)) {
      ssTokenIsIdentifier = 1;
      do
        ssNext++;
      while (IS_ALNUM(*ssNext));
    } else if (IS_MULTI(*ssNext))
      do
        ssNext++;
      while (IS_MULTI(*ssNext));
    else if (*ssNext != '\0')
      ssNext++;
  }

  ssSaved = *ssNext;
  *ssNext = '\0';

  return ssToken;
}

char *startScan(char *s) {
  ssNext = s;
  ssSaved = *s;
  return scan();
}

char *inhale(char endToken) {
  char *s;

  ssTokenIsInteger = 0;

  *ssNext = ssSaved;

  for (s = ssNext; *s && *s != endToken; s++)
    ;

  if (*s == '\0') {
    ssToken = ssNext = s;
    ssSaved = '\0';
    return (char *)0;
  } else {
    ssToken = ssNext;
    ssNext = s;
    ssSaved = *ssNext;
    *ssNext = '\0';
    return ssToken;
  }
}

void repair(void) {
  if (ssSaved) {
    *ssNext = ssSaved;
    ssSaved = *++ssNext;
  }
}

void gcl_freeEnv(gcl_env env) {
  while (env) {
    gcl_env e = env->next;

    free(env->name);
    free(env);
    env = e;
  }
}

gcl_q parseQ(gcl_env *env, int topCall);

gcl_q parseQTerm(gcl_env *env, int topCall) {
  gcl_q q;

  if (tokenIsInteger() || look("one") || look("all")) {
    unsigned n = 0;
    gcl_q element;
    int symbolic = 0, all = 0;

    if (tokenIsInteger())
      n = tokenAsInteger();
    else {
      if (look("one"))
        n = 1;
      else if (look("all")) {
        n = 0;
        all = 1;
      }
      symbolic = 1;
    }

    scan();
    if (look("^") || look("of")) {
      scan();

      if (!look("(")) {
        gcl_syntaxError =
            std::string("Missing \"(\" after start of combination operator.");
        return GCL_SYNTAX_ERROR;
      }

      q = allocNOfMQ(n, GCL_NULL_Q);

      do {
        scan();
        if ((element = parseQ(env, 0)) == GCL_SYNTAX_ERROR) {
          gcl_freeQ(q);
          return GCL_SYNTAX_ERROR;
        }
        GCL_Q_ELEMENTS(q) = allocNOfMElement(element, GCL_Q_ELEMENTS(q));
      } while (look(","));

      if (look(")"))
        scan();
      else {
        gcl_freeQ(q);
        gcl_syntaxError = std::string("Missing \")\".");
        return GCL_SYNTAX_ERROR;
      }

      n = 0;
      for (element = GCL_Q_ELEMENTS(q); element; element = GCL_Q_NEXT(element))
        n++;
      if (all)
        GCL_Q_N(q) = n;
      else if (n < GCL_Q_N(q)) {
        gcl_freeQ(q);
        gcl_syntaxError = std::string(
            "Too few elements on right-hand side of combination operator.");
        return GCL_SYNTAX_ERROR;
      }
    } else if (look("words")) {
      scan();
      if (n == 0 && !all) {
        gcl_syntaxError =
            std::string("The expression \"0 words\" is not valid.");
        return GCL_SYNTAX_ERROR;
      } else if (symbolic) {
        if (all)
          gcl_syntaxError =
              std::string("Expected \"^\" or \"of\" after \"all\".");
        else
          gcl_syntaxError =
              std::string("Expected \"^\" or \"of\" after \"one\".");
        return GCL_SYNTAX_ERROR;
      }
      q = allocFixedQ(n);
    } else {
      gcl_syntaxError =
          std::string("Integer not followed by  \"^\" or \"of\" or \"words\".");
      return GCL_SYNTAX_ERROR;
    }
  } else if (tokenIsIdentifier()) {
    gcl_env e;

    for (e = *env; e; e = e->next)
      if (strcmp(e->name, token()) == 0)
        break;

    if (e == GCL_NULL_ENV)
      if (topCall) {
        char *name = strdup(token());

        scan();
        if (look("=") &&
            (e = (gcl_env)malloc(sizeof(struct gcl_envStructure)))) {
          if (name)
            e->name = name;
          else {
            free(e);
            gcl_syntaxError = std::string("Out of memory!");
            return GCL_SYNTAX_ERROR;
          }
        } else {
          if (name)
            free(name);
          if (look("="))
            gcl_syntaxError = std::string("Out of memory!");
          else
            gcl_syntaxError = std::string("Undefined symbol.");
          return GCL_SYNTAX_ERROR;
        }
        scan();
        if ((q = parseQ(env, 0)) == GCL_SYNTAX_ERROR) {
          free(name);
          free(e);
          return GCL_SYNTAX_ERROR;
        } else {
          e->q = q;
          e->next = *env;
          *env = e;
          return GCL_NULL_Q;
        }
      } else {
        gcl_syntaxError = std::string("Undefined symbol.");
        return GCL_SYNTAX_ERROR;
      }
    else {
      scan();
      if (topCall && look("=")) {
        scan();
        if ((q = parseQ(env, 0)) == GCL_SYNTAX_ERROR)
          return GCL_SYNTAX_ERROR;
        gcl_freeQ(e->q);
        e->q = q;
        return GCL_NULL_Q;
      } else
        q = gcl_copyQ(e->q);
    }
  } else if (look("(")) {
    scan();
    if ((q = parseQ(env, 0)) == GCL_SYNTAX_ERROR)
      return GCL_SYNTAX_ERROR;
    if (look(")"))
      scan();
    else {
      gcl_syntaxError = std::string("Missing \")\".");
      return GCL_SYNTAX_ERROR;
    }
  } else if (look("[")) {
    scan();
    if (tokenIsInteger()) {
      unsigned n = tokenAsInteger();

      scan();
      if (look("]")) {
        scan();
        if (n == 0) {
          gcl_syntaxError = std::string("The expression \"[0]\" is not valid.");
          return GCL_SYNTAX_ERROR;
        }
        q = allocFixedQ(n);
      } else {
        gcl_syntaxError = std::string("Missing \"]\".");
        return GCL_SYNTAX_ERROR;
      }
    } else {
      gcl_syntaxError = std::string("Missing integer after \"[\".");
      return GCL_SYNTAX_ERROR;
    }
  } else if (look("\"")) {
    if (inhale('"')) {
      void *term;

      if ((term = term_create(token())))
        q = allocTermQ(term);
      else
        return GCL_SYNTAX_ERROR;
      repair();
    } else {
      gcl_syntaxError = std::string("End of quoted string missing.");
      return GCL_SYNTAX_ERROR;
    }
    scan();
  } else {
    gcl_syntaxError = std::string("Unexpected character in input.");
    return GCL_SYNTAX_ERROR;
  }

  return q;
}

gcl_q parseQ(gcl_env *env, int topCall) {
  gcl_q left, right;
  enum gcl_qnodeType type;

  if ((left = parseQTerm(env, topCall)) == GCL_SYNTAX_ERROR)
    return GCL_SYNTAX_ERROR;

  for (;;) {
    if (look(">>") || look(">"))
      type = GCL_CONTAINING;
    else if (look("<<") || look("<"))
      type = GCL_CONTAINED_IN;
    else if (look("/>"))
      type = GCL_NOT_CONTAINING;
    else if (look("/<"))
      type = GCL_NOT_CONTAINED_IN;
    else if (look("^") || look("and"))
      type = GCL_BOTH_OF;
    else if (look("+") || look("or"))
      type = GCL_ONE_OF;
    else if (look("<>") || look("..") || look("...") || look("...."))
      type = GCL_BEFORE;
    else if (look("containing"))
      type = GCL_CONTAINING;
    else if (look("contained")) {
      scan();
      if (look("in"))
        type = GCL_CONTAINED_IN;
      else {
        gcl_freeQ(left);
        return GCL_SYNTAX_ERROR;
      }
    } else if (look("not")) {
      scan();
      if (look("containing"))
        type = GCL_NOT_CONTAINING;
      else if (look("contained")) {
        scan();
        if (look("in"))
          type = GCL_NOT_CONTAINED_IN;
        else {
          gcl_freeQ(left);
          return GCL_SYNTAX_ERROR;
        }
      } else {
        gcl_freeQ(left);
        return GCL_SYNTAX_ERROR;
      }
    } else
      return left;

    scan();

    if ((right = parseQTerm(env, 0)) == GCL_SYNTAX_ERROR) {
      gcl_freeQ(left);
      return GCL_SYNTAX_ERROR;
    } else
      left = allocBinaryQ(type, left, right);
  }
}

gcl_q gcl_createQFromString(char *s, gcl_env *env) {
  gcl_q q;

  startScan(s);
  if (look(""))
    return GCL_NULL_Q;
  if ((q = parseQ(env, 1)) == GCL_SYNTAX_ERROR) {
    repair();
    return GCL_SYNTAX_ERROR;
  }
  if (look(""))
    return q;
  else {
    repair();
    gcl_freeQ(q);
    gcl_syntaxError = std::string("Extra characters at the end.");
    return GCL_SYNTAX_ERROR;
  }
}

} // namespace

Mt::~Mt() {
  gcl_freeEnv(env_);
  env_ = nullptr;
}

bool Mt::infix_expression(const std::string &expr, std::string *error) {
  char *temp = strdup(expr.c_str());
  gcl_q q = gcl_createQFromString(temp, &env_);
  free(temp);
  if (q == GCL_SYNTAX_ERROR) {
    safe_set(error) = gcl_syntaxError;
    return false;
  } else {
    s_expression_ = to_s_expression(q);
    gcl_freeQ(q);
    return true;
  }
  return true;
};

} // namespace cottontail
