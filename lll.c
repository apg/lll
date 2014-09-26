#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef __LP64__
typedef uint64_t sn_ptr_t;
typedef int64_t sn_int_t;
#define SN_INT_BITS 63
#else
typedef uint32_t sn_ptr_t;
typedef int32_t sn_int_t;
#define SN_INT_BITS 31
#endif

#include "lll.h"


static void print_atom(sn_t *S, FILE *out, atom_t a);
static void print_cons(sn_t *S, FILE *out, cons_t a);

#define isdelim(ch) (isspace(ch) || ch == '(' || ch == ')' || ch == '"')


static void
print_atom(sn_t *S, FILE *out, atom_t a)
{
  int i;
  switch (a.flag) {
  case FIXNUM_T:
    fprintf(out, "%ld", a.fixnum);
    break;
  case FLONUM_T:
    fprintf(out, "%lf", a.flonum);
    break;
  case KEYWORD_T:
  case SYMBOL_T:
    fprintf(out, "%s", a.string.data);
    break;
  case STRING_T:
    fputc('"', out);
    for (i = 0; i < a.string.length; i++) {
      if (a.string.data[i] == '"') {
        fputc('\\', out);
        fputc('"', out);
      }
      else {
        fputc(a.string.data[i], out);
      }
    }
    fputc('"', out);
    break;
  }
}

static void
print_cons(sn_t *S, FILE *out, cons_t a)
{
  obj_t *obj;

  if (ISNIL(a)) {
    fprintf(out, "()");
    return;
  } 
  fputc('(', out);
  for (obj = a.car; obj; ) {
    print_object(S, out, obj);
    
    obj = a.cdr;
    if (obj && obj->flag == CONS_T) {
      if (ISNIL(obj->cons)) {
        break;
      }
      a = obj->cons;
      obj = a.car;
      fputc(' ', out);
    } 
    else if (obj) {
      fputc(' ', out);
      print_object(S, out, obj);
      obj = NULL;
    }
  }
  fputc(')', out);
}

void
print_object(sn_t *S, FILE *out, obj_t *o)
{
  switch (o->flag) {
  case ATOM_T:
    print_atom(S, out, o->atom);
    break;
  case CONS_T:
    print_cons(S, out, o->cons);
    break;
  case CLOS_T:
    fputs("<#Closure: ", out);
    print_cons(S, out, o->cons);
    fputs(">", out);
    break;
  case PRIM_T:
    fputs("<#Primitive>", out);
    break;
  default:
    fprintf(stderr, "Invalid object! Aborting\n");
    exit(1);
  }
}

static obj_t *
read_string(sn_t *S, FILE *in)
{
  char buffer[255];
  int bufi = 0;
  int ch, la;

  while (bufi < 255) {
    ch = fgetc(in);
    if (ch == EOF) {
      fprintf(stderr, "ERROR: EOF while reading string.\n");
      return NULL;
    }
    if (ch == '"') {
      buffer[bufi] = '\0';
      return mk_str(S, buffer, bufi);
    }
    else if (ch == '\\') {
      la = fgetc(in);
      if (la == EOF) {
        fprintf(stderr, "ERROR: EOF while reading string.\n");
        return NULL;
      }
      switch (la) {
      case '\\':
        buffer[bufi++] = '\\';
        break;
      case '"':
        buffer[bufi++] = '"';
        break;
      case 'a':
        buffer[bufi++] = '\a';
        break;
      case 'n':
        buffer[bufi++] = '\n';
        break;
      case 'r':
        buffer[bufi++] = '\r';
        break;
      case 't':
        buffer[bufi++] = '\t';
        break;
      }
    } else {
      buffer[bufi++] = ch;
    }
  }
  
  fprintf(stderr, "ERROR: String too long\n");
  return NULL;
}

static obj_t *
read_number(sn_t *S, FILE *in, int negative)
{
  char buffer[32];
  int bufi = 0;
  int sawdot = 0;
  int ch;
  double floval;
  long fixval;

  while (bufi < 32) {
    ch = fgetc(in);
    if (ch == EOF) {
      fprintf(stderr, "ERROR: EOF while reading number\n");
      return NULL;
    }
    
    if (isdigit(ch)) {
      buffer[bufi++] = ch;
    }
    else if (ch == '.') {
      if (sawdot) {
        fprintf(stderr, "ERROR: Invalid number found\n");
        return NULL;
      }
      else {
        buffer[bufi++] = '.';
        sawdot = 1;
      }
    }
    else if (isdelim(ch)) {
      ungetc(ch, in);
      /* have our number. Let's do it */
      buffer[bufi] = '\0';
      if (bufi > 0) {
        if (sawdot) {
          floval = strtod(buffer, NULL);
          if (negative) {
            floval *= -1.0;
          }
          return mk_flonum(S, floval);
        }
        else {
          fixval = strtol(buffer, NULL, 10);
          if (negative) {
            fixval *= -1L;
          }
          return mk_fixnum(S, fixval);
        }
      }
      else {
        fprintf(stderr, "ERROR: Invalid number found\n");
        return NULL;
      }
    }
  }
  fprintf(stderr, "ERROR: Can't support numbers that large\n");
  return NULL;
}

static obj_t *
read_symbol(sn_t *S, FILE *in)
{
  char buffer[255];
  int bufi = 0;
  int ch, la, sawdot = -1;
  obj_t *module, *identifier;

  while (bufi < 255) {
    ch = fgetc(in);
    if (ch == EOF) {
      fprintf(stderr, "ERROR: EOF while reading string.\n");
      return NULL;
    }

    if (isalnum(ch)) {
      buffer[bufi++] = ch;
    }
    else if (ch == '.') { /* TODO: Need to turn this into a refer form ideally ... */
      sawdot = bufi;
      buffer[bufi++] = ch;
    }
    else if (isgraph(ch) && !isdelim(ch)) {
      buffer[bufi++] = ch;
    }
    else if (isdelim(ch)) {
      ungetc(ch, in);
      buffer[bufi] = '\0';

      /* This is a bit hacky */
      if (sawdot > 0) {
        buffer[sawdot] = '\0';
        module = intern(S, buffer, sawdot);
        buffer[sawdot] = ':';
        identifier = intern(S, buffer + sawdot, bufi - sawdot);
        /* TODO: potentially namespace the refer */
        return cons(S, intern(S, "refer", 5),
                    cons(S, module, 
                         cons(S, identifier, S->NIL)));
      }

      return intern(S, buffer, bufi);
    }
    else {
      fprintf(stderr, "ERROR: Invalid symbol character\n");
      return NULL;
    }
  }
}

static char
eat_space(FILE *in)
{
  int ch;
  do {
    ch = fgetc(in);
  } while (isspace(ch));
  return ch;
}

static obj_t *
read_list(sn_t *S, FILE *in)
{
  obj_t *obj;
  int ch;
  ch = fgetc(in);
  if (ch == EOF) {
    fprintf(stderr, "ERROR: EOF while reading list.\n");
    return NULL;
  }

  /* Is this just nil? */
  if (ch == ')') {
    return S->NIL;
  }
  else {
    ungetc(ch, in);
  }

  /* Ok. Legitimate list it seems. Let's read it recursively */
  obj = read_object(S, in);
  if (!obj) {
    return obj;
  }

  ch = eat_space(in);
  if (ch == ')') {
    return cons(S, obj, S->NIL);
  } 

  ungetc(ch, in);
  return cons(S, obj, read_list(S, in));
}

obj_t *
read_object(sn_t *S, FILE *in)
{
  obj_t *tmp;
  int ch, la;
 next:
  ch = fgetc(in);
  if (ch == EOF) {
    return NULL;
  }

  switch (ch) {
  case '(':
    return read_list(S, in);
  case ';': /* read til end of line */
    while ((ch = fgetc(in)) != '\n') {
      if (ch == EOF) {
        return NULL;
      }
    }
    goto next;
  case ' ':
  case '\t':
  case '\n':
  case '\r':
    goto next;
  case '"':
    return read_string(S, in);
  case '\'':
    tmp = read_object(S, in);
    if (tmp != NULL) {
      return cons(S, S->QUOTE, cons(S,tmp, S->NIL));
    }
    return tmp;
  case '-':
  case '+':
    la = fgetc(in);
    if (la == EOF) {
      fprintf(stderr, "ERROR: eof reached in mid form\n");
      return NULL;
    }
    if (isdigit(la)) {
      ungetc(la, in);
      return read_number(S, in, ch == '-');
    }
    else {
      ungetc(la, in);
    }
  default:
    ungetc(ch, in);
    if (isdigit(ch)) {
      return read_number(S, in, 0);
    }
  }
  return read_symbol(S, in);
}

obj_t *
mk_fixnum(sn_t *S, long d)
{
  obj_t *o = malloc(sizeof(*o));
  if (o == NULL) {
    perror("malloc");
    exit(1);
  }

  o->flag = ATOM_T;
  o->atom.flag = FIXNUM_T;
  o->atom.fixnum = d;

  return o;
}

obj_t *
mk_flonum(sn_t *S, double d)
{
  obj_t *o = malloc(sizeof(*o));
  if (o == NULL) {
    perror("malloc");
    exit(1);
  }

  o->flag = ATOM_T;
  o->atom.flag = FLONUM_T;
  o->atom.flonum = d;

  return o;
}

obj_t *
mk_str(sn_t *S, char *str, size_t len)
{
  obj_t *o = malloc(sizeof(*o));
  if (o == NULL) {
    perror("malloc");
    exit(1);
  }

  o->flag = ATOM_T;
  o->atom.flag = STRING_T;
  o->atom.string.data = strndup(str, len);
  o->atom.string.length = len;

  return o;
}

obj_t *
mk_sym(sn_t *S, char *str, size_t len, int keywordp)
{
  obj_t *o = malloc(sizeof(*o));
  if (o == NULL) {
    perror("malloc");
    exit(1);
  }

  o->flag = ATOM_T;
  o->atom.flag = keywordp ? KEYWORD_T : SYMBOL_T;
  o->atom.string.data = strndup(str, len);
  o->atom.string.length = len;

  return o;
}

obj_t *
intern(sn_t *S, char *str, size_t len)
{
  int i, keywordp = 0;
  obj_t *sym;
  if (S->Symtab_index > 0) {
    for (i = 0; i < S->Symtab_index; i++) {
      if (len != S->Symtab[i]->atom.string.length) {
        continue;
      }
      if (strncmp(S->Symtab[i]->atom.string.data, str, len) == 0) {
        return S->Symtab[i];
      }
    }
  }
  
  /* Make the symbol, or keyword. NOTE: Sym("foo") == Key("foo") */
  if (str[0] == ':') {
    keywordp = 1;
    sym = mk_sym(S, str, len, keywordp);
  }
  else {
    sym = mk_sym(S, str, len, keywordp);
  }

  if (S->Symtab_index < S->Symtab_alloc) {
    S->Symtab[S->Symtab_index++] = sym;
    return sym;
  }
  else if (S->Symtab_alloc == 0) {
    S->Symtab = malloc(sizeof(*S->Symtab) * SYMTAB_INIT_SIZE);
    if (S->Symtab == NULL) {
      perror("malloc");
      exit(1);
    }
    S->Symtab_alloc = SYMTAB_INIT_SIZE;
  }
  else { /* resize it */
    S->Symtab_alloc *= 2;
    S->Symtab = realloc(S->Symtab, sizeof(*S->Symtab) * S->Symtab_alloc);
    if (S->Symtab == NULL) {
      perror("realloc");
      exit(1);
    }
  }

  S->Symtab[S->Symtab_index++] = sym;
  return sym;
}

obj_t *
mk_clos(sn_t *S, obj_t *code, obj_t *env)
{
  obj_t *o = cons(S, code, env);
  o->flag = CLOS_T;

  return o;
}

obj_t *
mk_prim(sn_t *S, obj_t *(*func)(sn_t *, obj_t *), int arity, int max_arity)
{
  obj_t *o = malloc(sizeof(*o));
  if (o == NULL) {
    perror("malloc");
    exit(1);
  }

  o->flag = PRIM_T;
  o->prim.arity = arity;
  o->prim.max_arity = arity;
  o->prim.func = func;

  return o;
}


obj_t *
cons(sn_t *S, obj_t *a, obj_t *d)
{
  obj_t *o = malloc(sizeof(*o));
  if (o == NULL) {
    perror("malloc");
    exit(1);
  }

  o->flag = CONS_T;
  o->cons.car = a;
  o->cons.cdr = d;

  return o;
}

obj_t *
car(sn_t *S, obj_t *a)
{
  if (!a || a->flag != CONS_T) {
    fprintf(stderr, "ERROR: Attempt to take car of non-cons\n");
    return NULL;
  }

  if (a->cons.car == NULL) {
    return S->NIL;
  }
  return a->cons.car;
}

obj_t *
cdr(sn_t *S, obj_t *a)
{
  if (!a || a->flag != CONS_T) {
    fprintf(stderr, "ERROR: Attempt to take cdr of non-cons\n");
    return NULL;
  }

  if (a->cons.cdr == NULL) {
    return S->NIL;
  }
  return a->cons.cdr;
}

int
length(sn_t *S, obj_t *a)
{
  int i = 0;
  if (a->flag == CONS_T) {
    /* TODO this doesnt' handle cycles... */
    while (a != NULL && a != S->NIL) {
      i++;
      a = cdr(S, a);
    }
  }
  return i;
}

static obj_t *
env_extend(sn_t *S, obj_t *env, obj_t *names, obj_t *values)
{

#ifdef TRACE_DEBUG
  fprintf(stderr, "%d, %d\n\t", length(S, names), length(S, values));
  print_object(S, stderr, names);
  fputs(", ", stderr);
  print_object(S, stderr, values);
  fputc('\n', stderr);
#endif

  if (length(S, names) == length(S, values)) {
    return cons(S, cons(S, names, values), env);
  }
  fprintf(stderr, "FATAL: Too few names, or values in extend\n");
  exit(1);
}

static obj_t *
env_lookup(sn_t *S, obj_t *env, obj_t *sym)
{
  obj_t *names, *values, *frame;

  while (env != S->NIL && env != NULL) {
    frame = car(S, env);
    names = car(S, frame);
    values = cdr(S, frame);

    while (names != S->NIL) {
      if (car(S, names) == sym) {
        return car(S, values);
      }
      else {
        names = cdr(S, names);
        values = cdr(S, values);
      }
    }

    env = cdr(S, env);
  }

  return NULL;
}

static obj_t *
closure_env(sn_t *S, obj_t *a)
{
  if (!a || a->flag != CLOS_T) {
    fprintf(stderr, "ERROR: Attempt to take environment of non-closure\n");
    return NULL;
  }

  if (a->cons.cdr == NULL) {
    return S->NIL;
  }
  return a->cons.cdr;
}

static obj_t *
closure_params(sn_t *S, obj_t *a)
{
  if (!a || a->flag != CLOS_T) {
    fprintf(stderr, "ERROR: Attempt to take params of non-closure\n");
    return NULL;
  }

  return car(S, a->cons.car);
}

static obj_t *
closure_code(sn_t *S, obj_t *a)
{
  if (!a || a->flag != CLOS_T) {
    fprintf(stderr, "ERROR: Attempt to take code of non-closure\n");
    return NULL;
  }

  return cdr(S, a->cons.car);
}

/**
 * TODO: This is a temporary measure to get builtins installed.
 *       It should in the future actually use the module facility by
 *       creating a module, and adding the functions, and what not.
 *   
 *       For now, the module gets installed in a new frame in the
 *       Environment.
 */
obj_t *
module_install(sn_t *S, char *modname, module_entry_t *mod) 
{
  obj_t *names, *values, *name, *value;
  module_entry_t *current;
  int i = 0;

  if (mod == NULL) {
    return S->NIL;
  }

  names = S->NIL;
  values = S->NIL;

  while (mod[i].name != NULL) {
    if (mod[i].name == NULL || mod[i].name[0] == ':') {
      fprintf(stderr, "ERROR: can't bind value to a keyword\n");
      return NULL;
    }

    name = intern(S, mod[i].name, strlen(mod[i].name));
    names = cons(S, name, names);

    value = mk_prim(S, mod[i].func, mod[i].arity, mod[i].max_arity);
    values = cons(S, value, values);

    i++;
  }

  /* TODO: Should really flatten this, but OK for now... */
  S->Toplevel_Env = env_extend(S, S->Toplevel_Env, names, values);

  return S->NIL;
}


obj_t *
eval(sn_t *S, obj_t *a, obj_t *env)
{
#define NEXT(P) op = P; break;

  obj_t *ar, *dr;
  opcode_t op = OP_DISPATCH;
  if (!a || env->flag != CONS_T) {
    fprintf(stderr, "ERROR: Attempt to eval with improper arguments\n");
    print_object(S, stderr, a);
    return NULL;
  }

  S->Env = env;
  S->Exp = a;

  S->Opstack[S->Opstack_index++] = OP_DONE;

  for (;;) {
#ifdef TRACE_DEBUG
    fprintf(stderr, "TRACE: (Evaluating Exp)\n\t -> ");
    print_object(S, stderr, S->Exp);
    fputc('\n', stderr);
#endif

    switch (op) {
    case OP_DISPATCH:
#ifdef TRACE_DEBUG
      fprintf(stderr, "TRACE: OP_DISPATCH\n");
#endif
      if (S->Exp == S->NIL) {
        S->Val = S->NIL;
        NEXT(OP_POPJ_RET);
      }

      if (S->Exp->flag == ATOM_T) {
        if (S->Exp->atom.flag == KEYWORD_T) {
          S->Val = S->Exp;
          NEXT(OP_POPJ_RET);
        }
        else if (S->Exp->atom.flag == SYMBOL_T) {
          S->Val = env_lookup(S, S->Env, S->Exp);
          if (S->Val == NULL) {
            /* try the top level */
            S->Val = env_lookup(S, S->Toplevel_Env, S->Exp);
            if (S->Val == NULL) {
              fprintf(stderr, "FATAL: Unknown name: '%s'\n", S->Exp->atom.string.data);
              exit(1);
            }
          }
          NEXT(OP_POPJ_RET);
        }
        else {
          S->Val = S->Exp;
          NEXT(OP_POPJ_RET);
        }
      }
      else if (S->Exp->flag == CLOS_T || S->Exp->flag == PRIM_T) {
        S->Val = S->Exp;
        NEXT(OP_POPJ_RET);
      }
      else if (S->Exp->flag == CONS_T) {
        ar = car(S, S->Exp);

        if (ar == S->QUOTE) {
          S->Val = car(S, cdr(S, S->Exp));
          NEXT(OP_POPJ_RET);
        }
        else if (ar == S->IF) {
          dr = cdr(S, S->Exp);
          if (dr->flag == CONS_T) {
            S->Exp = car(S, dr);
#if TRACE_DEBUG
            fprintf(stderr, "Going to evaluate: ");
            print_object(S, stderr, S->Exp);
            fputc('\n', stderr);
#endif
            
            S->Clink = cons(S, S->Env, S->Clink);
            S->Clink = cons(S, cdr(S, dr), S->Clink);

#if TRACE_DEBUG
            fprintf(stderr, "Clinking: ");
            print_object(S, stderr, cdr(S, dr));
            fputc('\n', stderr);
#endif

            if (S->Opstack_index < S->Opstack_alloc) {
              S->Opstack[S->Opstack_index++] = OP_IF_DECIDE;
            }
            else {
              fprintf(stderr, "FATAL: Stack overflow in eval, OP_DISPATCH\n");
              exit(1);
            }
          }
          else {
            fprintf(stderr, "FATAL: Syntax error at 'if'\n");
            exit(1);
          }

          NEXT(OP_DISPATCH);
        }
        else if (ar == S->FN) {
          dr = cdr(S, S->Exp);
          if (dr && dr->flag == CONS_T) {
            ar = car(S, dr);
            if (ar && ar->flag == CONS_T) {
              S->Val = mk_clos(S, dr, S->Env);
            }
            else {
              fprintf(stderr, "FATAL: Syntax error in fn declaration\n");
              exit(1);
            }
          }
          else {
            fprintf(stderr, "FATAL: Syntax error in fn declaration\n");
            exit(1);
          }
          NEXT(OP_POPJ_RET);
        }
        else if (cdr(S, S->Exp) == S->NIL) {
          if (S->Opstack_index < S->Opstack_alloc) {
            S->Opstack[S->Opstack_index++] = OP_APPLY_NO_ARGS;
          }
          else {
            fprintf(stderr, "FATAL: Stack overflow in eval, OP_DISPATCH\n");
            exit(1);
          }

          S->Exp = car(S, S->Exp);
        }
        else {
          S->Clink = cons(S, S->Env, S->Clink);
          S->Clink = cons(S, S->Exp, S->Clink);

          if (S->Opstack_index < S->Opstack_alloc) {
            S->Opstack[S->Opstack_index++] = OP_ARGS;
          }
          else {
            fprintf(stderr, "FATAL: Stack overflow in eval, OP_DISPATCH\n");
            exit(1);
          }

          S->Exp = car(S, S->Exp);
        }

        NEXT(OP_DISPATCH);
      }

      fprintf(stderr, "FATAL: Got to end of dispatch, and did nothing.\n");
      exit(1);

    case OP_IF_DECIDE:
#ifdef TRACE_DEBUG
      fprintf(stderr, "TRACE: OP_IF_DECIDE\n");
#endif
      S->Exp = car(S, S->Clink);
      S->Clink = cdr(S, S->Clink);

      S->Env = car(S, S->Clink);
      S->Clink = cdr(S, S->Clink);

      if (S->Val == S->NIL) {
        S->Exp = cdr(S, S->Exp);
      }
      else {
        S->Exp = car(S, S->Exp);
      }
      NEXT(OP_DISPATCH);

    case OP_APPLY_NO_ARGS:

#ifdef TRACE_DEBUG
      fprintf(stderr, "TRACE: OP_APPLY_NO_ARGS\n");
#endif

      S->Args = S->NIL;
      NEXT(OP_APPLY);

    case OP_ARGS:

#ifdef TRACE_DEBUG
      fprintf(stderr, "TRACE: OP_ARGS\n");
#endif
      S->Exp = car(S, S->Clink);
      S->Clink = cdr(S, S->Clink);

      S->Env = car(S, S->Clink);
      S->Clink = cdr(S, S->Clink);

      S->Clink = cons(S, S->Val, S->Clink);

      S->Exp = cdr(S, S->Exp);
      S->Args = S->NIL;
      NEXT(OP_ARGS_1);

    case OP_ARGS_1:

#ifdef TRACE_DEBUG
      fprintf(stderr, "TRACE: OP_ARGS_1\n");
#endif

      if (cdr(S, S->Exp) == S->NIL) {
        S->Clink = cons(S, S->Args, S->Clink);

        if (S->Opstack_index < S->Opstack_alloc) {
          S->Opstack[S->Opstack_index++] = OP_LAST_ARG;
        }
        else {
          fprintf(stderr, "FATAL: Stack overflow in eval, OP_ARGS_1\n");
          exit(1);
        }
      }
      else {
        S->Clink = cons(S, S->Env, S->Clink);
        S->Clink = cons(S, S->Exp, S->Clink);
        S->Clink = cons(S, S->Args, S->Clink);

        if (S->Opstack_index < S->Opstack_alloc) {
          S->Opstack[S->Opstack_index++] = OP_ARGS_2;
        }
        else {
          fprintf(stderr, "FATAL: Stack overflow in eval, OP_ARGS_1\n");
          exit(1);
        }
      }
      S->Exp = car(S, S->Exp);
      NEXT(OP_DISPATCH);

    case OP_ARGS_2:

#ifdef TRACE_DEBUG
      fprintf(stderr, "TRACE: OP_ARGS_2\n");
#endif
      S->Args = car(S, S->Clink);
      S->Clink = cdr(S, S->Clink);

      S->Exp = car(S, S->Clink);
      S->Clink = cdr(S, S->Clink);

      S->Env = car(S, S->Clink);
      S->Clink = cdr(S, S->Clink);
      
      S->Args = cons(S, S->Val, S->Args);
      S->Exp = cdr(S, S->Exp);
      NEXT(OP_ARGS_1);

    case OP_LAST_ARG:

#ifdef TRACE_DEBUG
      fprintf(stderr, "TRACE: OP_LAST_ARG\n");
#endif
      S->Args = car(S, S->Clink);
      S->Clink = cdr(S, S->Clink);

      S->Args = cons(S, S->Val, S->Args);

      S->Val = car(S, S->Clink);
      S->Clink = cdr(S, S->Clink);

      NEXT(OP_APPLY);

    case OP_APPLY:
#ifdef TRACE_DEBUG
      fprintf(stderr, "TRACE: OP_APPLY\n");
#endif
      if (S->Val && S->Val->flag == PRIM_T) {
        S->Val = S->Val->prim.func(S, S->Args);

        NEXT(OP_POPJ_RET);
      }
      else if (S->Val && S->Val->flag == CLOS_T) {
#ifdef TRACE_DEBUG
        fprintf(stderr, "TRACE: Apply Closure\n\t params(Val): ");
        print_object(S, stderr, S->Val);
        print_object(S, stderr, closure_params(S, S->Val));
        fputc('\n', stderr);
#endif

        S->Env = env_extend(S, closure_env(S, S->Val), closure_params(S, S->Val), S->Args);
        S->Exp = closure_code(S, S->Val);

        NEXT(OP_DISPATCH);
      }
      /* TODO: Error: unable to apply this */
    case OP_POPJ_RET:
#ifdef TRACE_DEBUG
      fprintf(stderr, "TRACE: OP_POPJ_RET\n");
#endif
      if (S->Opstack_index > 0) {
        op = S->Opstack[--(S->Opstack_index)];
      }
      else {
        fprintf(stderr, "FATAL: No ops left on the stack\n");
        exit(1);
      }
      break;
    case OP_DONE:
#ifdef TRACE_DEBUG
      fprintf(stderr, "TRACE: OP_DONE\n");
#endif
    default:
      return S->Val;
    }
  }

  return S->NIL;
}


int
main(int argc, char **argv)
{
  sn_t S;
  obj_t *rd, *res;

  S.NIL = cons(&S, NULL, NULL);
  S.Toplevel_Env = S.NIL; /* this should more or less be the module */
  S.Env = S.NIL;
  S.Exp = S.NIL;
  S.Val = S.NIL;
  S.Clink = S.NIL;
  S.Opstack = malloc(sizeof(*S.Opstack) * OPSTACK_INIT_SIZE);
  S.Opstack_alloc = OPSTACK_INIT_SIZE;
  S.Opstack_index = 0;
  S.Args = S.NIL;
  S.Symtab = NULL;
  S.Symtab_index = 0;
  S.Symtab_alloc = 0;

  S.FN = intern(&S, "fn", 2);
  S.IF = intern(&S, "if", 2);
  S.QUOTE = intern(&S, "quote", 5);

  install_builtins(&S);

  while (!feof(stdin)) {
    fputs("lll> ", stdout);
    
    rd = read_object(&S, stdin);
    if (rd == NULL) {
      fflush(stdin);
    }
    else {
      res = eval(&S, rd, S.Env);
      if (res != NULL) {
        fputs("  => ", stdout);
        print_object(&S, stdout, res);
        fputc('\n', stdout);
      }
      else {
        fflush(stdin);
      }
    }
  }

  return 0;
}
