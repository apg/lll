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

obj_t *NIL;
obj_t *Env;
obj_t *Exp;
obj_t *Clink;
obj_t *Val;
obj_t *Args;
obj_t *FN;
obj_t *IF;
obj_t *QUOTE;
obj_t **Symtab = NULL;
size_t Symtab_alloc = 0;
int Symtab_index = 0;
opcode_t *Opstack = NULL;
size_t Opstack_alloc = 0;
int Opstack_index = 0;


static void print_atom(FILE *out, atom_t a);
static void print_cons(FILE *out, cons_t a);

static void
print_atom(FILE *out, atom_t a)
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
print_cons(FILE *out, cons_t a)
{
  obj_t *obj;

  if (ISNIL(a)) {
    fprintf(out, "()");
    return;
  } 
  fputc('(', out);
  for (obj = a.car; obj; ) {
    print_object(out, obj);
    
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
      print_object(out, obj);
      obj = NULL;
    }
  }
  fputc(')', out);
}

void
print_object(FILE *out, obj_t *o)
{
  switch (o->flag) {
  case ATOM_T:
    print_atom(out, o->atom);
    break;
  case CONS_T:
    print_cons(out, o->cons);
    break;
  case CLOS_T:
    fputs("<#Closure: ", out);
    print_cons(out, o->cons);
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
read_string(FILE *in)
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
      return mk_str(buffer, bufi);
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
read_number(FILE *in, int negative)
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
    else if (isspace(ch) || ch == '(' || ch == ')' || ch == '"') {
      ungetc(ch, in);
      /* have our number. Let's do it */
      buffer[bufi] = '\0';
      if (bufi > 0) {
        if (sawdot) {
          floval = strtod(buffer, NULL);
          if (negative) {
            floval *= -1.0;
          }
          return mk_flonum(floval);
        }
        else {
          fixval = strtol(buffer, NULL, 10);
          if (negative) {
            fixval *= -1L;
          }
          return mk_fixnum(fixval);
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
read_symbol(FILE *in)
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

    if (isalnum(ch)) {
      buffer[bufi++] = ch;
    }
    else if (ch == '.') { /* TODO: Need to turn this into a refer form ideally ... */
      buffer[bufi++] = ch;
    }
    else if (isgraph(ch) && !isspace(ch) && ch != '(' && ch != ')' && ch != '"') {
      buffer[bufi++] = ch;
    }
    else if (isspace(ch) || ch == '(' || ch == ')' || ch == '"') {
      ungetc(ch, in);
      buffer[bufi] = '\0';
      return intern(buffer, bufi);
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
read_list(FILE *in)
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
    return NIL;
  }
  else {
    ungetc(ch, in);
  }

  /* Ok. Legitimate list it seems. Let's read it recursively */
  obj = read_object(in);
  if (!obj) {
    return obj;
  }

  print_object(stdout, obj);

  ch = eat_space(in); printf("Ate space\n");

  if (ch == ')') {
    return cons(obj, NIL);
  } 

  ungetc(ch, in);
  return cons(obj, read_list(in));
}

obj_t *
read_object(FILE *in)
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
    return read_list(in);
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
    return read_string(in);
  case '\'':
    tmp = read_object(in);
    if (tmp != NULL) {
      return cons(QUOTE, cons(tmp, NIL));
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
      return read_number(in, ch == '-');
    }
    else {
      ungetc(la, in);
    }
  default:
    ungetc(ch, in);
    if (isdigit(ch)) {
      return read_number(in, 0);
    }
  }
  fprintf(stderr, "Reading a symbol.\n");
  return read_symbol(in);
}

obj_t *
mk_fixnum(long d)
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
mk_flonum(double d)
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
mk_str(char *str, size_t len)
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
mk_sym(char *str, size_t len, int keywordp)
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
intern(char *str, size_t len)
{
  int i, keywordp = 0;
  obj_t *sym;
  if (Symtab_index > 0) {
    for (i = 0; i < Symtab_index; i++) {
      if (len != Symtab[i]->atom.string.length) {
        continue;
      }
      if (strncmp(Symtab[i]->atom.string.data, str, len) == 0) {
        return Symtab[i];
      }
    }
  }
  
  /* Make the symbol, or keyword. NOTE: Sym("foo") == Key("foo") */
  if (str[0] == ':') {
    keywordp = 1;
    sym = mk_sym(str, len, keywordp);
  }
  else {
    sym = mk_sym(str, len, keywordp);
  }

  if (Symtab_index < Symtab_alloc) {
    Symtab[Symtab_index++] = sym;
    return sym;
  }
  else if (Symtab_alloc == 0) {
    Symtab = malloc(sizeof(*Symtab) * SYMTAB_INIT_SIZE);
    if (Symtab == NULL) {
      perror("malloc");
      exit(1);
    }
    Symtab_alloc = SYMTAB_INIT_SIZE;
  }
  else { /* resize it */
    Symtab_alloc *= 2;
    Symtab = realloc(Symtab, sizeof(*Symtab) * Symtab_alloc);
    if (Symtab == NULL) {
      perror("realloc");
      exit(1);
    }
  }

  Symtab[Symtab_index++] = sym;
  return sym;
}

obj_t *
mk_clos(obj_t *code, obj_t *env)
{
  obj_t *o = cons(code, env);
  o->flag = CLOS_T;

  return o;
}

obj_t *
cons(obj_t *a, obj_t *d)
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
car(obj_t *a)
{
  if (!a || a->flag != CONS_T) {
    fprintf(stderr, "ERROR: Attempt to take car of non-cons\n");
    return NULL;
  }

  if (a->cons.car == NULL) {
    return NIL;
  }
  return a->cons.car;
}

obj_t *
cdr(obj_t *a)
{
  if (!a || a->flag != CONS_T) {
    fprintf(stderr, "ERROR: Attempt to take cdr of non-cons\n");
    return NULL;
  }

  if (a->cons.cdr == NULL) {
    return NIL;
  }
  return a->cons.cdr;
}

static int
length(obj_t *a)
{
  int i = 0;
  if (a->flag == CONS_T) {
    while (a != NULL && a != NIL) {
      i++;
      a = cdr(a);
    }
  }
  return i;
}

static obj_t *
env_extend(obj_t *env, obj_t *names, obj_t *values)
{
  fprintf(stderr, "%d, %d\n\t", length(names), length(values));
  print_object(stderr, names);
  fputs(", ", stderr);
  print_object(stderr, values);
  fputc('\n', stderr);

  if (length(names) == length(values)) {
    return cons(cons(names, values), env);
  }
  fprintf(stderr, "FATAL: Too few names, or values in extend\n");
  exit(1);
}

static obj_t *
env_lookup(obj_t *env, obj_t *sym)
{
  obj_t *names, *values, *frame;

  while (env != NIL && env != NULL) {
    frame = car(env);
    names = car(frame);
    values = cdr(frame);

    while (names != NIL) {
      if (car(names) == sym) {
        return car(values);
      }
      else {
        names = cdr(names);
        values = cdr(values);
      }
    }

    env = cdr(env);
  }

  return NULL;
}

static obj_t *
closure_env(obj_t *a)
{
  if (!a || a->flag != CLOS_T) {
    fprintf(stderr, "ERROR: Attempt to take environment of non-closure\n");
    return NULL;
  }

  if (a->cons.cdr == NULL) {
    return NIL;
  }
  return a->cons.cdr;
}

static obj_t *
closure_params(obj_t *a)
{
  if (!a || a->flag != CLOS_T) {
    fprintf(stderr, "ERROR: Attempt to take params of non-closure\n");
    return NULL;
  }

  return car(a->cons.car);
}

static obj_t *
closure_code(obj_t *a)
{
  if (!a || a->flag != CLOS_T) {
    fprintf(stderr, "ERROR: Attempt to take code of non-closure\n");
    return NULL;
  }

  return cdr(a->cons.car);
}

obj_t *
eval(obj_t *a, obj_t *env)
{
#define NEXT(P) op = P; break;

  obj_t *ar, *dr;
  opcode_t op = OP_DISPATCH;
  if (!a || env->flag != CONS_T) {
    fprintf(stderr, "ERROR: Attempt to eval with improper arguments\n");
    print_object(stderr, a);
    return NULL;
  }

  Env = env;
  Exp = a;

  Opstack[Opstack_index++] = OP_DONE;

  for (;;) {
    fprintf(stderr, "TRACE: (Evaluating Exp)\n\t -> ");
    print_object(stderr, Exp);
    fputc('\n', stderr);
    switch (op) {
    case OP_DISPATCH:
      fprintf(stderr, "TRACE: OP_DISPATCH\n");
      if (Exp == NIL) {
        Val = NIL;
        NEXT(OP_POPJ_RET);
      }

      if (Exp->flag == ATOM_T) {
        if (Exp->atom.flag == KEYWORD_T) {
          Val = Exp;
          NEXT(OP_POPJ_RET);
        }
        else if (Exp->atom.flag == SYMBOL_T) {
          Val = env_lookup(Env, Exp);
          if (Val == NULL) {
            fprintf(stderr, "FATAL: Unknown name: '%s'\n", Exp->atom.string.data);
            exit(1);
          }
          NEXT(OP_POPJ_RET);
        }
        else {
          Val = Exp;
          NEXT(OP_POPJ_RET);
        }
      }
      else if (Exp->flag == CLOS_T || Exp->flag == PRIM_T) {
        Val = Exp;
        NEXT(OP_POPJ_RET);
      }
      else if (Exp->flag == CONS_T) {
        ar = car(Exp);

        if (ar == QUOTE) {
          Val = car(cdr(Exp));
          NEXT(OP_POPJ_RET);
        }
        else if (ar == IF) {
          dr = cdr(Exp);
          if (dr->flag == CONS_T) {
            Exp = car(dr);
            fprintf(stderr, "Going to evaluate: ");
            print_object(stderr, Exp);
            fputc('\n', stderr);
            
            Clink = cons(Env, Clink);
            Clink = cons(cdr(dr), Clink);

            fprintf(stderr, "Clinking: ");
            print_object(stderr, cdr(dr));
            fputc('\n', stderr);

            if (Opstack_index < Opstack_alloc) {
              Opstack[Opstack_index++] = OP_IF_DECIDE;
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
        else if (ar == FN) {
          dr = cdr(Exp);
          if (dr && dr->flag == CONS_T) {
            ar = car(dr);
            if (ar && ar->flag == CONS_T) {
              Val = mk_clos(dr, Env);
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
        else if (cdr(Exp) == NIL) {
          if (Opstack_index < Opstack_alloc) {
            Opstack[Opstack_index++] = OP_APPLY_NO_ARGS;
          }
          else {
            fprintf(stderr, "FATAL: Stack overflow in eval, OP_DISPATCH\n");
            exit(1);
          }

          Exp = car(Exp);
        }
        else {
          Clink = cons(Env, Clink);
          Clink = cons(Exp, Clink);

          if (Opstack_index < Opstack_alloc) {
            Opstack[Opstack_index++] = OP_ARGS;
          }
          else {
            fprintf(stderr, "FATAL: Stack overflow in eval, OP_DISPATCH\n");
            exit(1);
          }

          Exp = car(Exp);
        }

        NEXT(OP_DISPATCH);
      }

      fprintf(stderr, "FATAL: Got to end of dispatch, and did nothing.\n");
      exit(1);

    case OP_IF_DECIDE:
      fprintf(stderr, "TRACE: OP_IF_DECIDE\n");
      Exp = car(Clink);
      Clink = cdr(Clink);

      Env = car(Clink);
      Clink = cdr(Clink);

      if (Val == NIL) {
        Exp = cdr(Exp);
      }
      else {
        Exp = car(Exp);
      }
      NEXT(OP_DISPATCH);

    case OP_APPLY_NO_ARGS:
      fprintf(stderr, "TRACE: OP_APPLY_NO_ARGS\n");
      Args = NIL;
      NEXT(OP_APPLY);

    case OP_ARGS:
      fprintf(stderr, "TRACE: OP_ARGS\n");
      Exp = car(Clink);
      Clink = cdr(Clink);

      Env = car(Clink);
      Clink = cdr(Clink);

      Clink = cons(Val, Clink);

      Exp = cdr(Exp);
      Args = NIL;
      NEXT(OP_ARGS_1);

    case OP_ARGS_1:
      fprintf(stderr, "TRACE: OP_ARGS_1\n");
      if (cdr(Exp) == NIL) {
        Clink = cons(Args, Clink);

        if (Opstack_index < Opstack_alloc) {
          Opstack[Opstack_index++] = OP_LAST_ARG;
        }
        else {
          fprintf(stderr, "FATAL: Stack overflow in eval, OP_ARGS_1\n");
          exit(1);
        }
      }
      else {
        Clink = cons(Env, Clink);
        Clink = cons(Exp, Clink);
        Clink = cons(Args, Clink);

        if (Opstack_index < Opstack_alloc) {
          Opstack[Opstack_index++] = OP_ARGS_2;
        }
        else {
          fprintf(stderr, "FATAL: Stack overflow in eval, OP_ARGS_1\n");
          exit(1);
        }
      }
      Exp = car(Exp);
      NEXT(OP_DISPATCH);

    case OP_ARGS_2:
      fprintf(stderr, "TRACE: OP_ARGS_2\n");
      Args = car(Clink);
      Clink = cdr(Clink);

      Exp = car(Clink);
      Clink = cdr(Clink);

      Env = car(Clink);
      Clink = cdr(Clink);
      
      Args = cons(Val, Args);
      Exp = cdr(Exp);
      NEXT(OP_ARGS_1);

    case OP_LAST_ARG:
      fprintf(stderr, "TRACE: OP_LAST_ARG\n");
      Args = car(Clink);
      Clink = cdr(Clink);

      Args = cons(Val, Args);

      Val = car(Clink);
      Clink = cdr(Clink);

      NEXT(OP_APPLY);

    case OP_APPLY:
      fprintf(stderr, "TRACE: OP_APPLY\n");
      if (Val && Val->flag == PRIM_T) {
        Val = Val->prim.func(Args);

        NEXT(OP_POPJ_RET);
      }
      else if (Val && Val->flag == CLOS_T) {
        fprintf(stderr, "TRACE: Apply Closure\n\t params(Val): ");
        print_object(stderr, Val);
        print_object(stderr, closure_params(Val));
        fputc('\n', stderr);

        Env = env_extend(closure_env(Val), closure_params(Val), Args);
        Exp = closure_code(Val);

        NEXT(OP_DISPATCH);
      }
      /* TODO: Error: unable to apply this */
    case OP_POPJ_RET:
      fprintf(stderr, "TRACE: OP_POPJ_RET\n");
      if (Opstack_index > 0) {
        op = Opstack[--(Opstack_index)];
      }
      else {
        fprintf(stderr, "FATAL: No ops left on the stack\n");
        exit(1);
      }
      break;
    case OP_DONE:
      fprintf(stderr, "TRACE: OP_DONE\n");
    default:
      return Val;
    }
  }

  return NIL;
}


int
main(int argc, char **argv)
{
  sn_t S;
  obj_t *rd, *res;

  NIL = cons(NULL, NULL);
  Env = NIL;
  Exp = NIL;
  Val = NIL;
  Clink = NIL;
  Opstack = malloc(sizeof(*Opstack) * OPSTACK_INIT_SIZE);
  Opstack_alloc = OPSTACK_INIT_SIZE;
  Opstack_index = 0;
  Args = NIL;
  Symtab = NULL;
  Symtab_index = 0;
  Symtab_alloc = 0;

  FN = intern("fn", 2);
  IF = intern("if", 2);
  QUOTE = intern("quote", 5);

  while (!feof(stdin)) {
    fputs("lll> ", stdout);
    
    rd = read_object(stdin);
    if (rd == NULL) {
      fflush(stdin);
    }
    else {
      res = eval(rd, Env);
      if (res != NULL) {
        fputs("  => ", stdout);
        print_object(stdout, res);
        fputc('\n', stdout);
      }
      else {
        fflush(stdin);
      }
    }
  }

  return 0;
}
