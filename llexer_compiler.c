#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "llex.h"
#include "llexer_compiler.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* Parser State */
typedef struct {
    lua_State *L;
    int table_idx;
    int current_idx;
    int num_tokens;
} ParserState;

/* Utility to read current token from Lua lexer table */
static void get_token(ParserState *ps, Token *t, char **val) {
    if (ps->current_idx > ps->num_tokens) {
        t->token = TK_EOS;
        *val = NULL;
        return;
    }

    lua_rawgeti(ps->L, ps->table_idx, ps->current_idx);

    lua_getfield(ps->L, -1, "token");
    t->token = lua_tointeger(ps->L, -1);
    lua_pop(ps->L, 1);

    lua_getfield(ps->L, -1, "value");
    if (lua_isstring(ps->L, -1)) {
        size_t len;
        const char *s = lua_tolstring(ps->L, -1, &len);
        *val = malloc(len + 1);
        strcpy(*val, s);
    } else {
        *val = NULL;
    }
    lua_pop(ps->L, 2); /* pop value and token table */
}

/* Very basic recursive descent to build a simple IR Tree */
static IRNode *new_node(IRNodeType type) {
    IRNode *n = malloc(sizeof(IRNode));
    memset(n, 0, sizeof(IRNode));
    n->type = type;
    return n;
}

static IRNode *parse_expr(ParserState *ps);

static IRNode *parse_stmt(ParserState *ps);

/* Forward declare parse_expr */
static IRNode *parse_expr(ParserState *ps);

/* Parsing prefix and suffixes */
static IRNode *parse_primary(ParserState *ps) {
    Token t; char *val;
    get_token(ps, &t, &val);
    ps->current_idx++;

    IRNode *n = NULL;
    if (t.token == TK_NAME) {
        n = new_node(IR_EXPR_NAME);
        n->str_val = val;
    } else if (t.token == TK_INT) {
        n = new_node(IR_EXPR_LITERAL_INT);
        n->int_val = val ? atoi(val) : 0;
        if(val) free(val);
    } else if (t.token == TK_FLT) {
        n = new_node(IR_EXPR_LITERAL_FLT);
        n->flt_val = val ? atof(val) : 0.0;
        if(val) free(val);
    } else if (t.token == TK_STRING || t.token == TK_RAWSTRING || t.token == TK_INTERPSTRING) {
        n = new_node(IR_EXPR_LITERAL_STR);
        n->str_val = val;
    } else if (t.token == TK_NIL) {
        n = new_node(IR_EXPR_LITERAL_NIL);
        if(val) free(val);
    } else if (t.token == TK_TRUE) {
        n = new_node(IR_EXPR_LITERAL_BOOL);
        n->int_val = 1;
        if(val) free(val);
    } else if (t.token == TK_FALSE) {
        n = new_node(IR_EXPR_LITERAL_BOOL);
        n->int_val = 0;
        if(val) free(val);
    } else if (t.token == '{') {
        n = new_node(IR_EXPR_TABLE);
        if(val) free(val);

        IRNode *fields_head = NULL, *fields_tail = NULL;
        while (ps->current_idx <= ps->num_tokens) {
            get_token(ps, &t, &val);
            if (t.token == '}') {
                ps->current_idx++;
                if(val) free(val);
                break;
            }
            if(val) free(val);

            /* parse field. Could be `[expr] = expr`, `name = expr`, or `expr` */
            IRNode *field = new_node(IR_STMT_ASSIGN); /* use assign as key/value pair container */
            get_token(ps, &t, &val);
            if (t.token == '[') {
                ps->current_idx++;
                if(val) free(val);
                field->children[0] = parse_expr(ps);
                get_token(ps, &t, &val);
                if (t.token == ']') ps->current_idx++;
                if(val) free(val);
                get_token(ps, &t, &val);
                if (t.token == '=') ps->current_idx++;
                if(val) free(val);
                field->children[1] = parse_expr(ps);
            } else if (t.token == TK_NAME) {
                /* check next to see if '=' */
                int is_named_key = 0;
                int save_idx = ps->current_idx;
                ps->current_idx++; /* consume name */
                Token next_t; char *next_val;
                get_token(ps, &next_t, &next_val);
                if (next_t.token == '=') {
                    is_named_key = 1;
                }
                if(next_val) free(next_val);
                ps->current_idx = save_idx; /* revert */

                if (is_named_key) {
                    field->children[0] = new_node(IR_EXPR_LITERAL_STR);
                    /* string token value needs quotes since our generator outputs raw string without adding them unless it's a LITERAL_STR but wait, LITERAL_STR adds quotes. */
                    /* actually, we can just use the raw name and let LITERAL_STR generator add the quotes */
                    field->children[0]->str_val = strdup(val); /* Duplicate the string */
                    ps->current_idx++; /* consume name */
                    get_token(ps, &t, &val); /* get '=' */
                    ps->current_idx++; /* consume '=' */
                    if(val) free(val);
                    field->children[1] = parse_expr(ps);
                } else {
                    /* fallback to parsing a normal expression (it was a name but not a named key) */
                    /* we need to restore the index and parse the expression normally */
                    ps->current_idx = save_idx - 1; /* point back to the TK_NAME */
                    if(val) free(val);
                    field->children[1] = parse_expr(ps); /* value only */
                }
            } else {
                /* not a bracket or a name-key, must be a plain value */
                if(val) free(val);
                field->children[1] = parse_expr(ps); /* value only */
            }

            if (!fields_head) fields_head = fields_tail = field;
            else { fields_tail->next = field; fields_tail = field; }

            get_token(ps, &t, &val);
            if (t.token == ',' || t.token == ';') {
                ps->current_idx++;
                if(val) free(val);
            }
        }
        n->children[0] = fields_head;
    } else if (t.token == '(') {
        if(val) free(val);
        n = parse_expr(ps);
        get_token(ps, &t, &val);
        if (t.token == ')') ps->current_idx++;
        if(val) free(val);
    } else {
        if(val) free(val);
    }

    /* Parse suffixes: '.', '[', ':', '(' */
    while (1) {
        get_token(ps, &t, &val);
        if (t.token == '.') {
            ps->current_idx++;
            if(val) free(val);
            get_token(ps, &t, &val);
            if (t.token == TK_NAME) {
                ps->current_idx++;
                IRNode *index = new_node(IR_EXPR_INDEX);
                index->children[0] = n;
                index->children[1] = new_node(IR_EXPR_LITERAL_STR);
                index->children[1]->str_val = val;
                n = index;
            } else {
                if(val) free(val);
                break;
            }
        } else if (t.token == '[') {
            ps->current_idx++;
            if(val) free(val);
            IRNode *inner = parse_expr(ps);
            get_token(ps, &t, &val);
            if (t.token == ']') ps->current_idx++;
            if(val) free(val);

            IRNode *index = new_node(IR_EXPR_INDEX);
            index->children[0] = n;
            index->children[1] = inner;
            n = index;
        } else if (t.token == ':') {
            ps->current_idx++;
            if(val) free(val);
            get_token(ps, &t, &val);
            if (t.token == TK_NAME) {
                ps->current_idx++;
                IRNode *method = new_node(IR_EXPR_METHOD);
                method->children[0] = n;
                method->children[1] = new_node(IR_EXPR_LITERAL_STR);
                method->children[1]->str_val = val;

                /* Parse args */
                get_token(ps, &t, &val);
                if (t.token == '(') {
                    ps->current_idx++;
                    if(val) free(val);

                    get_token(ps, &t, &val);
                    if (t.token == ')') {
                        ps->current_idx++;
                        if(val) free(val);
                    } else {
                        if(val) free(val);
                        IRNode *args_head = NULL, *args_tail = NULL;
                        while (1) {
                            IRNode *arg = parse_expr(ps);
                            if (!args_head) args_head = args_tail = arg;
                            else { args_tail->next = arg; args_tail = arg; }

                            get_token(ps, &t, &val);
                            if (t.token == ',') {
                                ps->current_idx++;
                                if(val) free(val);
                            } else {
                                break;
                            }
                        }

                        IRNode *arg_list = new_node(IR_STMT_LIST);
                        arg_list->children[0] = args_head; /* Just attach the list to children[0] or whatever, generate_code handles IR_STMT_LIST */
                        /* Actually, generate_code for IR_STMT_LIST uses children[0] and children[1] as a linked list. Wait. */
                        /* The existing AST uses `next` pointers for lists, let's just use `next` */
                        method->children[2] = args_head;

                        get_token(ps, &t, &val);
                        if (t.token == ')') ps->current_idx++;
                        if(val) free(val);
                    }
                } else if (t.token == TK_STRING || t.token == TK_RAWSTRING || t.token == TK_INTERPSTRING) {
                    method->children[2] = parse_primary(ps);
                } else if (t.token == '{') {
                    method->children[2] = parse_primary(ps);
                } else {
                    if(val) free(val);
                }
                n = method;
            } else {
                if(val) free(val);
                break;
            }
        } else if (t.token == '(' || t.token == TK_STRING || t.token == TK_RAWSTRING || t.token == TK_INTERPSTRING || t.token == '{') {
            IRNode *call = new_node(IR_EXPR_CALL);
            call->children[0] = n;
            if (t.token == '(') {
                ps->current_idx++;
                if(val) free(val);
                get_token(ps, &t, &val);
                if (t.token == ')') {
                    ps->current_idx++;
                    if(val) free(val);
                } else {
                    if(val) free(val);
                    IRNode *args_head = NULL, *args_tail = NULL;
                    while (1) {
                        IRNode *arg = parse_expr(ps);
                        if (!args_head) args_head = args_tail = arg;
                        else { args_tail->next = arg; args_tail = arg; }

                        get_token(ps, &t, &val);
                        if (t.token == ',') {
                            ps->current_idx++;
                            if(val) free(val);
                        } else {
                            break;
                        }
                    }
                    call->children[1] = args_head;

                    get_token(ps, &t, &val);
                    if (t.token == ')') ps->current_idx++;
                    if(val) free(val);
                }
            } else {
                call->children[1] = parse_primary(ps);
                if(val) free(val);
            }
            n = call;
        } else {
            if(val) free(val);
            break;
        }
    }
    return n;
}

/* Naive expression parsing */
static IRNode *parse_expr(ParserState *ps) {
    Token t; char *val;
    get_token(ps, &t, &val);
    if(val) free(val);

    IRNode *n = NULL;

    /* Unary operators */
    if (t.token == '-' || t.token == TK_NOT || t.token == '#') {
        ps->current_idx++;
        n = new_node(IR_EXPR_UNOP);
        n->token = t.token;
        n->children[0] = parse_expr(ps); /* unary binds tightly but let's just recurse */
    } else {
        n = parse_primary(ps);
    }

    /* Binary Op lookup (simplified) */
    get_token(ps, &t, &val);
    if (t.token == '+' || t.token == '-' || t.token == '*' || t.token == '/' || t.token == '%' || t.token == '^' ||
        t.token == TK_CONCAT || t.token == TK_EQ || t.token == TK_NE || t.token == '<' || t.token == '>' ||
        t.token == TK_LE || t.token == TK_GE || t.token == TK_AND || t.token == TK_OR) {
        int op = t.token;
        ps->current_idx++;
        if(val) free(val);

        IRNode *binop = new_node(IR_EXPR_BINOP);
        binop->token = op;
        binop->children[0] = n;
        binop->children[1] = parse_expr(ps);
        return binop;
    }
    if(val) free(val);

    return n;
}

static IRNode *parse_stmt(ParserState *ps) {
    Token t; char *val;
    get_token(ps, &t, &val);

        if (t.token == TK_LOCAL) {
        ps->current_idx++;
        if(val) free(val);

        get_token(ps, &t, &val);
        if (t.token == TK_FUNCTION) {
            ps->current_idx++;
            if(val) free(val);

            get_token(ps, &t, &val);
            IRNode *n = new_node(IR_STMT_FUNCTION);
            n->str_val = val;
            ps->current_idx++;

            get_token(ps, &t, &val);
            if (t.token == '(') ps->current_idx++;
            if(val) free(val);

            /* parse params - simple 1 param for now */
            get_token(ps, &t, &val);
            if (t.token == TK_NAME) {
                n->children[0] = new_node(IR_EXPR_NAME);
                n->children[0]->str_val = val;
                ps->current_idx++;
            } else {
                if(val) free(val);
            }

            get_token(ps, &t, &val);
            if (t.token == ')') ps->current_idx++;
            if(val) free(val);

            /* parse body */
            IRNode *body_head = NULL, *body_tail = NULL;
            while (ps->current_idx <= ps->num_tokens) {
                get_token(ps, &t, &val);
                if (t.token == TK_END) {
                    if(val) free(val);
                    ps->current_idx++;
                    break;
                }
                if(val) free(val);
                IRNode *s = parse_stmt(ps);
                if (s) {
                    if (!body_head) body_head = body_tail = s;
                    else { body_tail->next = s; body_tail = s; }
                }
            }
            n->children[1] = body_head;
            return n;
        }

        /* Not a function, just a local variable */
        IRNode *n = new_node(IR_STMT_LOCAL);
        n->str_val = val;
        ps->current_idx++;

        get_token(ps, &t, &val);
        if (t.token == '=') {
            ps->current_idx++;
            if(val) free(val);
            n->children[0] = parse_expr(ps);
        } else {
            if(val) free(val);
        }
        return n;
    }
 else if (t.token == TK_IF) {
        ps->current_idx++;
        if(val) free(val);

        IRNode *n = new_node(IR_STMT_IF);
        n->children[0] = parse_expr(ps); /* Cond */

        get_token(ps, &t, &val);
        if (t.token == TK_THEN) ps->current_idx++;
        if(val) free(val);

                /* Parse body */
        IRNode *body_head = NULL, *body_tail = NULL;
        IRNode *else_head = NULL, *else_tail = NULL;
        int in_else = 0;

        while (ps->current_idx <= ps->num_tokens) {
            get_token(ps, &t, &val);
            if (t.token == TK_END) {
                if(val) free(val);
                ps->current_idx++;
                break;
            } else if (t.token == TK_ELSE) {
                in_else = 1;
                ps->current_idx++;
                if(val) free(val);
                continue;
            }
            if(val) free(val);
            IRNode *s = parse_stmt(ps);
            if (s) {
                if (in_else) {
                    if (!else_head) else_head = else_tail = s;
                    else { else_tail->next = s; else_tail = s; }
                } else {
                    if (!body_head) body_head = body_tail = s;
                    else { body_tail->next = s; body_tail = s; }
                }
            } else {
                ps->current_idx++; /* fallback skip */
            }
        }
        n->children[1] = body_head;
        n->children[2] = else_head;


        return n;
    } else if (t.token == TK_WHILE) {
        ps->current_idx++;
        if(val) free(val);

        IRNode *n = new_node(IR_STMT_WHILE);
        n->children[0] = parse_expr(ps);

        get_token(ps, &t, &val);
        if (t.token == TK_DO) ps->current_idx++;
        if(val) free(val);

        IRNode *body_head = NULL, *body_tail = NULL;
        while (ps->current_idx <= ps->num_tokens) {
            get_token(ps, &t, &val);
            if (t.token == TK_END) {
                if(val) free(val);
                ps->current_idx++;
                break;
            }
            if(val) free(val);
            IRNode *s = parse_stmt(ps);
            if (s) {
                if (!body_head) body_head = body_tail = s;
                else { body_tail->next = s; body_tail = s; }
            } else {
                ps->current_idx++;
            }
        }
        n->children[1] = body_head;
        return n;
    } else if (t.token == TK_REPEAT) {
        ps->current_idx++;
        if(val) free(val);

        IRNode *n = new_node(IR_STMT_REPEAT);

        IRNode *body_head = NULL, *body_tail = NULL;
        while (ps->current_idx <= ps->num_tokens) {
            get_token(ps, &t, &val);
            if (t.token == TK_UNTIL) {
                if(val) free(val);
                ps->current_idx++;
                break;
            }
            if(val) free(val);
            IRNode *s = parse_stmt(ps);
            if (s) {
                if (!body_head) body_head = body_tail = s;
                else { body_tail->next = s; body_tail = s; }
            } else {
                ps->current_idx++;
            }
        }
        n->children[0] = body_head;
        n->children[1] = parse_expr(ps);
        return n;
    } else if (t.token == TK_FOR) {
        ps->current_idx++;
        if(val) free(val);

        /* Check if it's numeric for or generic for */
        IRNode *names_head = NULL, *names_tail = NULL;
        while (1) {
            get_token(ps, &t, &val);
            if (t.token == TK_NAME) {
                IRNode *name = new_node(IR_EXPR_NAME);
                name->str_val = val;
                ps->current_idx++;
                if (!names_head) names_head = names_tail = name;
                else { names_tail->next = name; names_tail = name; }

                get_token(ps, &t, &val);
                if (t.token == ',') {
                    ps->current_idx++;
                    if(val) free(val);
                } else {
                    if(val) free(val);
                    break;
                }
            } else {
                if(val) free(val);
                break;
            }
        }

        get_token(ps, &t, &val);
        if (t.token == '=') {
            /* Numeric for */
            ps->current_idx++;
            if(val) free(val);

            IRNode *n = new_node(IR_STMT_FOR);
            if (names_head) n->str_val = strdup(names_head->str_val); /* Just take first name */

            n->children[0] = parse_expr(ps); /* start */

            get_token(ps, &t, &val);
            if (t.token == ',') ps->current_idx++;
            if(val) free(val);

            n->children[1] = parse_expr(ps); /* end */

            /* optional step */
            get_token(ps, &t, &val);
            if (t.token == ',') {
                ps->current_idx++;
                if(val) free(val);
                n->children[2] = parse_expr(ps);
            } else {
                if(val) free(val);
            }

            get_token(ps, &t, &val);
            if (t.token == TK_DO) ps->current_idx++;
            if(val) free(val);

            IRNode *body_head = NULL, *body_tail = NULL;
            while (ps->current_idx <= ps->num_tokens) {
                get_token(ps, &t, &val);
                if (t.token == TK_END) {
                    if(val) free(val);
                    ps->current_idx++;
                    break;
                }
                if(val) free(val);
                IRNode *s = parse_stmt(ps);
                if (s) {
                    if (!body_head) body_head = body_tail = s;
                    else { body_tail->next = s; body_tail = s; }
                } else {
                    ps->current_idx++;
                }
            }
            n->children[3] = body_head;
            return n;
        } else if (t.token == TK_IN) {
            /* Generic for */
            ps->current_idx++;
            if(val) free(val);

            IRNode *n = new_node(IR_STMT_FOR_IN);
            n->children[0] = names_head;

            IRNode *exprs_head = NULL, *exprs_tail = NULL;
            while (ps->current_idx <= ps->num_tokens) {
                get_token(ps, &t, &val);
                if (t.token == TK_DO) {
                    if(val) free(val);
                    break; /* break out of expr parsing when DO is found */
                }
                if(val) free(val);

                IRNode *expr = parse_expr(ps);
                if (!exprs_head) exprs_head = exprs_tail = expr;
                else { exprs_tail->next = expr; exprs_tail = expr; }

                get_token(ps, &t, &val);
                if (t.token == ',') {
                    ps->current_idx++;
                    if(val) free(val);
                } else if (t.token == TK_DO) {
                    /* don't consume DO here, let outer loop break */
                    if(val) free(val);
                    break;
                } else {
                    if(val) free(val);
                    break;
                }
            }
            n->children[1] = exprs_head;

            get_token(ps, &t, &val);
            if (t.token == TK_DO) ps->current_idx++;
            if(val) free(val);

            IRNode *body_head = NULL, *body_tail = NULL;
            while (ps->current_idx <= ps->num_tokens) {
                get_token(ps, &t, &val);
                if (t.token == TK_END) {
                    if(val) free(val);
                    ps->current_idx++;
                    break;
                }
                if(val) free(val);
                IRNode *s = parse_stmt(ps);
                if (s) {
                    if (!body_head) body_head = body_tail = s;
                    else { body_tail->next = s; body_tail = s; }
                } else {
                    ps->current_idx++;
                }
            }
            n->children[2] = body_head;
            return n;
        } else {
            if(val) free(val);
            return NULL; /* Syntax error */
        }
    } else if (t.token == TK_RETURN) {
        ps->current_idx++;
        if(val) free(val);
        IRNode *n = new_node(IR_STMT_RETURN);
        n->children[0] = parse_expr(ps);
        return n;
    } else if (t.token == TK_NAME || t.token == '(') {
        /* Could be assignment or call, delegate to parse_expr */
        if(val) free(val);
        IRNode *lhs = parse_expr(ps);

        get_token(ps, &t, &val);
        if (t.token == '=') {
            ps->current_idx++;
            if(val) free(val);
            IRNode *n = new_node(IR_STMT_ASSIGN);
            n->children[0] = lhs;
            n->children[1] = parse_expr(ps);
            return n;
        } else {
            if(val) free(val);
            if (lhs && (lhs->type == IR_EXPR_CALL || lhs->type == IR_EXPR_METHOD)) {
                /* valid expression statement */
                IRNode *expr_stmt = new_node(IR_STMT_EXPR);
                expr_stmt->children[0] = lhs;
                return expr_stmt;
            } else if (lhs) {
                /* potentially invalid expression statement, but return it for now */
                IRNode *expr_stmt = new_node(IR_STMT_EXPR);
                expr_stmt->children[0] = lhs;
                return expr_stmt;
            }
        }
    }

    /* Fallback skip */
    ps->current_idx++;
    if(val) free(val);
    return NULL;
}

static IRNode *parse_chunk(ParserState *ps) {
    IRNode *head = NULL, *tail = NULL;
    while (ps->current_idx <= ps->num_tokens) {
        IRNode *s = parse_stmt(ps);
        if (s) {
            if (!head) head = tail = s;
            else { tail->next = s; tail = s; }
        }
    }
    return head;
}

/* Semantic / Scope Analysis */
typedef struct Scope {
    char *locals[100];
    int count;
    struct Scope *parent;
} Scope;

static void analyze_scope(IRNode *node, Scope *s) {
    if (!node) return;

    if (node->type == IR_STMT_LOCAL) {
        if (s->count < 100 && node->str_val) {
            s->locals[s->count++] = node->str_val;
        }
        analyze_scope(node->children[0], s);
        } else if (node->type == IR_EXPR_NAME) {
        /* Check if variable exists in scope chain */
        int found = 0;
        Scope *curr = s;
        while (curr && !found) {
            for (int i = 0; i < curr->count; i++) {
                if (curr->locals[i] && node->str_val && strcmp(curr->locals[i], node->str_val) == 0) {
                    found = 1;
                    break;
                }
            }
            curr = curr->parent;
        }
        /* Optionally warn/error on implicit global if `found == 0` */
    } else {
        /* Recursively analyze children */
        Scope child_scope;
        child_scope.count = 0;
        child_scope.parent = s;

        for (int i = 0; i < 4; i++) {
            if (node->children[i]) {
                if (node->type == IR_STMT_IF && i == 1) {
                    /* Body gets a new scope */
                    analyze_scope(node->children[i], &child_scope);
                } else {
                    analyze_scope(node->children[i], s);
                }
            }
        }
    }

    analyze_scope(node->next, s);
}

/* Control Flow Graph Builder */
static BasicBlock *new_block(CFG *cfg) {
    BasicBlock *b = malloc(sizeof(BasicBlock));
    memset(b, 0, sizeof(BasicBlock));
    b->id = cfg->next_block_id++;
    return b;
}

static BasicBlock *build_cfg(CFG *cfg, IRNode *stmt, BasicBlock *current) {
    while (stmt) {
        if (stmt->type == IR_STMT_IF) {
            /* If condition ends the block */
            IRNode *cond = stmt->children[0];
            IRNode *body = stmt->children[1];

            BasicBlock *true_block = new_block(cfg);
            BasicBlock *end_block = new_block(cfg);

            current->next_true = true_block;
            current->next_false = end_block; /* Simplification: no else block */

            /* Build true branch */
            BasicBlock *true_end = build_cfg(cfg, body, true_block);
            if (true_end) {
                true_end->next = end_block;
            }

            current = end_block;
        } else {
            /* Add statement to current block */
            IRNode *clone = malloc(sizeof(IRNode));
            *clone = *stmt;
            clone->next = NULL;

            if (!current->stmts) {
                current->stmts = clone;
            } else {
                IRNode *tail = current->stmts;
                while (tail->next) tail = tail->next;
                tail->next = clone;
            }

            if (stmt->type == IR_STMT_RETURN) {
                current->next = NULL;
                break;
            }
        }
        stmt = stmt->next;
    }
    return current;
}

/* Control Flow Flattening (CFF) */
/* This converts the CFG back into a single flat AST state machine */
static IRNode *transform_cff(CFG *cfg, BasicBlock *entry) {
    if (!entry) return NULL;

    IRNode *state_var = new_node(IR_STMT_LOCAL);
    state_var->str_val = strdup("__cff_state");

    IRNode *init_val = new_node(IR_EXPR_LITERAL_INT);
    init_val->int_val = entry->id;
    state_var->children[0] = init_val;

    IRNode *while_loop = new_node(IR_STMT_WHILE);
    while_loop->children[0] = new_node(IR_EXPR_LITERAL_BOOL);
    while_loop->children[0]->int_val = 1; /* while true */

    IRNode *head = state_var;
    head->next = while_loop;

    IRNode *switch_head = NULL;
    IRNode *switch_tail = NULL;

    BasicBlock *curr = entry;
    while (curr) {
        IRNode *if_stmt = new_node(IR_STMT_IF);

        IRNode *cond = new_node(IR_EXPR_BINOP);
        cond->token = TK_EQ;
        cond->children[0] = new_node(IR_EXPR_NAME);
        cond->children[0]->str_val = strdup("__cff_state");
        cond->children[1] = new_node(IR_EXPR_LITERAL_INT);
        cond->children[1]->int_val = curr->id;

        if_stmt->children[0] = cond;

        IRNode *block_body = curr->stmts;
        IRNode *last_stmt = NULL;
        if (block_body) {
            IRNode *tmp = block_body;
            while (tmp->next) tmp = tmp->next;
            last_stmt = tmp;
        }

        /* State transition */
        if (!last_stmt || last_stmt->type != IR_STMT_RETURN) {
            IRNode *next_state = new_node(IR_STMT_ASSIGN);
            next_state->children[0] = new_node(IR_EXPR_NAME);
            next_state->children[0]->str_val = strdup("__cff_state");
            next_state->children[1] = new_node(IR_EXPR_LITERAL_INT);
            next_state->children[1]->int_val = curr->next ? curr->next->id : -1;

            if (last_stmt) last_stmt->next = next_state;
            else block_body = next_state;
            next_state->next = new_node(IR_STMT_BREAK);
        }

        if_stmt->children[1] = block_body;

        if (!switch_head) switch_head = switch_tail = if_stmt;
        else { switch_tail->next = if_stmt; switch_tail = if_stmt; }

        curr = curr->next;
    }

        IRNode *if_break = new_node(IR_STMT_IF);
    IRNode *cond_break = new_node(IR_EXPR_BINOP);
    cond_break->token = TK_EQ;
    cond_break->children[0] = new_node(IR_EXPR_NAME);
    cond_break->children[0]->str_val = strdup("__cff_state");
    cond_break->children[1] = new_node(IR_EXPR_LITERAL_INT);
    cond_break->children[1]->int_val = -1;
    if_break->children[0] = cond_break;

    IRNode *brk_stmt = new_node(IR_STMT_CALL);
    brk_stmt = new_node(IR_EXPR_NAME);
    brk_stmt->str_val = strdup("break");
    if_break->children[1] = brk_stmt;

    if (!switch_head) switch_head = switch_tail = if_break;
    else { switch_tail->next = if_break; switch_tail = if_break; }

    while_loop->children[1] = switch_head;

    return head;
}

/* Bogus Control Flow Insertion */
static void insert_bogus_branches(IRNode *node) {
    if (!node) return;

    if (node->type == IR_STMT_ASSIGN || node->type == IR_STMT_CALL) {
        if (rand() % 10 < 3) { /* 30% chance to insert bogus branch */
            IRNode *bogus_if = new_node(IR_STMT_IF);

            /* math.random() > 0.5 */
            IRNode *cond = new_node(IR_EXPR_BINOP);
            cond->token = '>';

            IRNode *call = new_node(IR_EXPR_CALL);
            call->children[0] = new_node(IR_EXPR_NAME);
            call->children[0]->str_val = strdup("math.random");

            cond->children[0] = call;
            cond->children[1] = new_node(IR_EXPR_LITERAL_FLT);
            cond->children[1]->flt_val = 0.5;

            bogus_if->children[0] = cond;

            /* Clone statement for true branch, do nothing for false */
            IRNode *clone = malloc(sizeof(IRNode));
            *clone = *node;
            clone->next = NULL;

            bogus_if->children[1] = clone;

            /* Replace current node with bogus if */
            *node = *bogus_if;
            free(bogus_if);
            return;
        }
    }

    for (int i = 0; i < 4; i++) {
        if (node->children[i]) insert_bogus_branches(node->children[i]);
    }
    insert_bogus_branches(node->next);
}

/* String Encryption */
static void encrypt_strings(IRNode *node) {
    if (!node) return;

    /* Do not encrypt method names (they must be raw syntax identifiers) */
    if (node->type == IR_EXPR_METHOD) {
        if (node->children[0]) encrypt_strings(node->children[0]);
        if (node->children[2]) encrypt_strings(node->children[2]);
        /* skip children[1] which is the method name */
    } else if (node->type == IR_EXPR_LITERAL_STR) {
        if (node->str_val) {
            /* Simple XOR encryption */
            size_t len = strlen(node->str_val);
            char *enc = malloc(len + 1);
            for (size_t i = 0; i < len; i++) {
                enc[i] = node->str_val[i] ^ 0x42;
            }
            enc[len] = '\0';

            IRNode *call = new_node(IR_EXPR_CALL);
            call->children[0] = new_node(IR_EXPR_NAME);
            call->children[0]->str_val = strdup("__decrypt");

            call->children[1] = new_node(IR_EXPR_LITERAL_STR);
            call->children[1]->str_val = enc;

            *node = *call;
            free(call);
        }
    } else {
        for (int i = 0; i < 4; i++) {
            if (node->children[i]) encrypt_strings(node->children[i]);
        }
    }
    encrypt_strings(node->next);
}

/* Code Generation */
static void generate_code(IRNode *node, luaL_Buffer *B, int indent) {
    if (!node) return;

    char spaces[100];
    memset(spaces, ' ', indent);
    spaces[indent] = '\0';

    if (node->type == IR_STMT_LOCAL) {
        luaL_addstring(B, spaces);
        luaL_addstring(B, "local ");
        luaL_addstring(B, node->str_val);
        if (node->children[0]) {
            luaL_addstring(B, " = ");
            generate_code(node->children[0], B, 0);
        }
        luaL_addstring(B, "\n");
    } else if (node->type == IR_STMT_ASSIGN) {
        luaL_addstring(B, spaces);
        generate_code(node->children[0], B, 0);
        luaL_addstring(B, " = ");
        generate_code(node->children[1], B, 0);
        luaL_addstring(B, "\n");
    } else if (node->type == IR_STMT_CALL) {
        luaL_addstring(B, spaces);
        generate_code(node->children[0], B, 0);
        luaL_addstring(B, "(");
        generate_code(node->children[1], B, 0);
        luaL_addstring(B, ")\n");
        } else if (node->type == IR_STMT_IF) {
        luaL_addstring(B, spaces);
        luaL_addstring(B, "if ");
        generate_code(node->children[0], B, 0);
        luaL_addstring(B, " then\n");
        generate_code(node->children[1], B, indent + 2);
        if (node->children[2]) {
            luaL_addstring(B, spaces);
            luaL_addstring(B, "else\n");
            generate_code(node->children[2], B, indent + 2);
        }
        luaL_addstring(B, spaces);
        luaL_addstring(B, "end\n");
    }
 else if (node->type == IR_STMT_WHILE) {
        luaL_addstring(B, spaces);
        luaL_addstring(B, "while ");
        generate_code(node->children[0], B, 0);
        luaL_addstring(B, " do\n");
        generate_code(node->children[1], B, indent + 2);
        luaL_addstring(B, spaces);
        luaL_addstring(B, "end\n");
    } else if (node->type == IR_STMT_REPEAT) {
        luaL_addstring(B, spaces);
        luaL_addstring(B, "repeat\n");
        generate_code(node->children[0], B, indent + 2);
        luaL_addstring(B, spaces);
        luaL_addstring(B, "until ");
        generate_code(node->children[1], B, 0);
        luaL_addstring(B, "\n");
    } else if (node->type == IR_STMT_FOR) {
        luaL_addstring(B, spaces);
        luaL_addstring(B, "for ");
        if (node->str_val) luaL_addstring(B, node->str_val);
        luaL_addstring(B, " = ");
        generate_code(node->children[0], B, 0);
        luaL_addstring(B, ", ");
        generate_code(node->children[1], B, 0);
        if (node->children[2]) {
            luaL_addstring(B, ", ");
            generate_code(node->children[2], B, 0);
        }
        luaL_addstring(B, " do\n");
        generate_code(node->children[3], B, indent + 2);
        luaL_addstring(B, spaces);
        luaL_addstring(B, "end\n");
    } else if (node->type == IR_STMT_FOR_IN) {
        luaL_addstring(B, spaces);
        luaL_addstring(B, "for ");
        if (node->children[0]) {
            IRNode *name = node->children[0];
            while (name) {
                luaL_addstring(B, name->str_val);
                name = name->next;
                if (name) luaL_addstring(B, ", ");
            }
        }
        luaL_addstring(B, " in ");
        if (node->children[1]) {
            IRNode *expr = node->children[1];
            while (expr) {
                generate_code(expr, B, 0);
                expr = expr->next;
                if (expr) luaL_addstring(B, ", ");
            }
        }
        luaL_addstring(B, " do\n");
        generate_code(node->children[2], B, indent + 2);
        luaL_addstring(B, spaces);
        luaL_addstring(B, "end\n");
    } else if (node->type == IR_STMT_FUNCTION) {
        luaL_addstring(B, spaces);
        luaL_addstring(B, "local function ");
        luaL_addstring(B, node->str_val);
        luaL_addstring(B, "(");
        if (node->children[0]) generate_code(node->children[0], B, 0);
        luaL_addstring(B, ")\n");
        generate_code(node->children[1], B, indent + 2);
        luaL_addstring(B, spaces);
        luaL_addstring(B, "end\n");
    } else if (node->type == IR_EXPR_UNOP) {
        char buf[8];
        if (node->token == TK_NOT) strcpy(buf, "not ");
        else snprintf(buf, sizeof(buf), "%c", (char)node->token);
        luaL_addstring(B, buf);
        generate_code(node->children[0], B, 0);
    } else if (node->type == IR_STMT_BREAK) {
        luaL_addstring(B, spaces);
        luaL_addstring(B, "break\n");
    } else if (node->type == IR_STMT_LIST) {
        generate_code(node->children[0], B, indent);
        /* Check if it's the arg list vs block list */
        if (node->children[0] && node->children[0]->type == IR_EXPR_NAME) luaL_addstring(B, ", ");
        else luaL_addstring(B, "\n");
        generate_code(node->children[1], B, indent);
    } else if (node->type == IR_STMT_RETURN) {
        luaL_addstring(B, spaces);
        luaL_addstring(B, "return ");
        generate_code(node->children[0], B, 0);
        luaL_addstring(B, "\n");
    } else if (node->type == IR_STMT_EXPR) {
        luaL_addstring(B, spaces);
        generate_code(node->children[0], B, 0);
        luaL_addstring(B, "\n");
    } else if (node->type == IR_EXPR_NAME) {
        if (strcmp(node->str_val, "break") == 0) luaL_addstring(B, "break\n");
        else luaL_addstring(B, node->str_val);
    } else if (node->type == IR_EXPR_LITERAL_INT) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%lld", node->int_val);
        luaL_addstring(B, buf);
    } else if (node->type == IR_EXPR_LITERAL_FLT) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%f", node->flt_val);
        luaL_addstring(B, buf);
    } else if (node->type == IR_EXPR_LITERAL_STR) {
        luaL_addstring(B, "\"");
        luaL_addstring(B, node->str_val);
        luaL_addstring(B, "\"");
    } else if (node->type == IR_EXPR_LITERAL_NIL) {
        luaL_addstring(B, "nil");
    } else if (node->type == IR_EXPR_LITERAL_BOOL) {
        luaL_addstring(B, node->int_val ? "true" : "false");
    } else if (node->type == IR_EXPR_TABLE) {
        luaL_addstring(B, "{");
        if (node->children[0]) {
            IRNode *field = node->children[0];
            while (field) {
                if (field->children[0]) {
                    if (field->children[0]->type == IR_EXPR_LITERAL_STR) {
                        luaL_addstring(B, "[");
                        generate_code(field->children[0], B, 0);
                        luaL_addstring(B, "] = ");
                    } else {
                        luaL_addstring(B, "[");
                        generate_code(field->children[0], B, 0);
                        luaL_addstring(B, "] = ");
                    }
                }
                generate_code(field->children[1], B, 0);
                field = field->next;
                if (field) luaL_addstring(B, ", ");
            }
        }
        luaL_addstring(B, "}");
    } else if (node->type == IR_EXPR_INDEX) {
        generate_code(node->children[0], B, 0);
        luaL_addstring(B, "[");
        generate_code(node->children[1], B, 0);
        luaL_addstring(B, "]");
    } else if (node->type == IR_EXPR_METHOD) {
        generate_code(node->children[0], B, 0);
        luaL_addstring(B, ":");
        if (node->children[1]->type == IR_EXPR_LITERAL_STR) {
            luaL_addstring(B, node->children[1]->str_val);
        }
        luaL_addstring(B, "(");
        if (node->children[2]) {
            IRNode *arg = node->children[2];
            while (arg) {
                generate_code(arg, B, 0);
                arg = arg->next;
                if (arg) luaL_addstring(B, ", ");
            }
        }
        luaL_addstring(B, ")");
    } else if (node->type == IR_EXPR_BINOP) {
        generate_code(node->children[0], B, 0);
        char buf[8];
        if (node->token == TK_EQ) strcpy(buf, " == ");
        else if (node->token == TK_NE) strcpy(buf, " ~= ");
        else if (node->token == TK_LE) strcpy(buf, " <= ");
        else if (node->token == TK_GE) strcpy(buf, " >= ");
        else if (node->token == TK_AND) strcpy(buf, " and ");
        else if (node->token == TK_OR) strcpy(buf, " or ");
        else if (node->token == TK_CONCAT) strcpy(buf, " .. ");
        else snprintf(buf, sizeof(buf), " %c ", node->token);
        luaL_addstring(B, buf);
        generate_code(node->children[1], B, 0);
    } else if (node->type == IR_EXPR_CALL) {
        generate_code(node->children[0], B, 0);
        luaL_addstring(B, "(");
        if (node->children[1]) {
            IRNode *arg = node->children[1];
            while (arg) {
                generate_code(arg, B, 0);
                arg = arg->next;
                if (arg) luaL_addstring(B, ", ");
            }
        }
        luaL_addstring(B, ")");
    }

    /* Now that we use IR_STMT_EXPR for statements, any IR_EXPR node should NOT traverse its `next` pointer here. */
    if (node->type >= IR_EXPR_BINOP) {
        return; /* Expressions do not implicitly traverse `next`, the parent list/call does it. */
    }

    if (node->next && indent > 0) {
        generate_code(node->next, B, indent);
    } else if (node->next) {
        generate_code(node->next, B, 0);
    }
}

/* Inject decryption stub as AST nodes */
static void inject_decrypt_stub(IRNode **ast) {
    IRNode *func = new_node(IR_STMT_FUNCTION);
    func->str_val = strdup("__decrypt");

    IRNode *param = new_node(IR_EXPR_NAME);
    param->str_val = strdup("s");
    func->children[0] = param;

    IRNode *body_list = new_node(IR_STMT_LIST);

    IRNode *loc_res = new_node(IR_STMT_LOCAL);
    loc_res->str_val = strdup("res");
    loc_res->children[0] = new_node(IR_EXPR_LITERAL_STR);
    loc_res->children[0]->str_val = strdup("");
    body_list->children[0] = loc_res;

    IRNode *loc_i = new_node(IR_STMT_LOCAL);
    loc_i->str_val = strdup("i");
    loc_i->children[0] = new_node(IR_EXPR_LITERAL_INT);
    loc_i->children[0]->int_val = 1;
    loc_res->next = loc_i;

    IRNode *while_loop = new_node(IR_STMT_WHILE);

    IRNode *cond = new_node(IR_EXPR_BINOP);
    cond->token = '<';
    cond->children[0] = new_node(IR_EXPR_NAME);
    cond->children[0]->str_val = strdup("i");

    IRNode *len_s = new_node(IR_EXPR_UNOP);
    len_s->token = '#';
    len_s->children[0] = new_node(IR_EXPR_NAME);
    len_s->children[0]->str_val = strdup("s");
    cond->children[1] = len_s;

    while_loop->children[0] = cond;
    loc_i->next = while_loop;

    IRNode *assign_res = new_node(IR_STMT_ASSIGN);
    assign_res->children[0] = new_node(IR_EXPR_NAME);
    assign_res->children[0]->str_val = strdup("res");

    IRNode *concat = new_node(IR_EXPR_BINOP);
    concat->token = '+'; /* Actually just emit string manually, since + is used for concat in basic AST test if TK_CONCAT fails */
    concat->children[0] = new_node(IR_EXPR_NAME);
    concat->children[0]->str_val = strdup("res");

    IRNode *char_call = new_node(IR_EXPR_CALL);
    char_call->children[0] = new_node(IR_EXPR_NAME);
    char_call->children[0]->str_val = strdup("string.char");

    IRNode *bxor = new_node(IR_EXPR_BINOP);
    bxor->token = '~';

    IRNode *byte_call = new_node(IR_EXPR_CALL);
    byte_call->children[0] = new_node(IR_EXPR_NAME);
    byte_call->children[0]->str_val = strdup("string.byte");

    IRNode *byte_args = new_node(IR_STMT_LIST);
    byte_args->children[0] = new_node(IR_EXPR_NAME);
    byte_args->children[0]->str_val = strdup("s");
    byte_args->children[1] = new_node(IR_EXPR_NAME);
    byte_args->children[1]->str_val = strdup("i");
    byte_call->children[1] = byte_args;

    bxor->children[0] = byte_call;
    bxor->children[1] = new_node(IR_EXPR_LITERAL_INT);
    bxor->children[1]->int_val = 0x42;

    char_call->children[1] = bxor;
    concat->children[1] = char_call;

    assign_res->children[1] = concat;

    IRNode *inc_i = new_node(IR_STMT_ASSIGN);
    inc_i->children[0] = new_node(IR_EXPR_NAME);
    inc_i->children[0]->str_val = strdup("i");

    IRNode *add_i = new_node(IR_EXPR_BINOP);
    add_i->token = '+';
    add_i->children[0] = new_node(IR_EXPR_NAME);
    add_i->children[0]->str_val = strdup("i");
    add_i->children[1] = new_node(IR_EXPR_LITERAL_INT);
    add_i->children[1]->int_val = 1;
    inc_i->children[1] = add_i;

    assign_res->next = inc_i;
    while_loop->children[1] = assign_res;

    IRNode *ret = new_node(IR_STMT_RETURN);
    ret->children[0] = new_node(IR_EXPR_NAME);
    ret->children[0]->str_val = strdup("res");
    while_loop->next = ret;

    func->children[1] = body_list;
    func->next = *ast;
    *ast = func;
}


/* API Entry Point */
LUAMOD_API int lua_lexer_obfuscate(lua_State *L, const char *code, int cff, int bogus, int str_enc) {
    /* 1. First run llexer to get tokens */
    lua_getglobal(L, "require");
    lua_pushstring(L, "lexer");
    lua_call(L, 1, 1);

    lua_pushstring(L, "lex");
    lua_gettable(L, -2);
    lua_pushstring(L, code);
    lua_call(L, 1, 1);

    if (!lua_istable(L, -1)) {
        luaL_error(L, "lexer returned non-table");
    }

    ParserState ps;
    ps.L = L;
    ps.table_idx = lua_gettop(L);
    ps.current_idx = 1;
    ps.num_tokens = luaL_len(L, ps.table_idx);

    /* 2. Parse into AST */
    IRNode *ast = parse_chunk(&ps);

    /* 3. Semantic Analysis */
    Scope global_scope;
    global_scope.count = 0;
    global_scope.parent = NULL;
    analyze_scope(ast, &global_scope);

    /* 4. Obfuscation Passes */
    if (str_enc) {
        encrypt_strings(ast);
        inject_decrypt_stub(&ast);
    }

    if (bogus) {
        insert_bogus_branches(ast);
    }

    if (cff) {
        CFG cfg;
        cfg.next_block_id = 1;
        BasicBlock *entry = new_block(&cfg);
        build_cfg(&cfg, ast, entry);
        ast = transform_cff(&cfg, entry);
    }

    /* 5. Code Generation */
    luaL_Buffer b;
    luaL_buffinit(L, &b);



    generate_code(ast, &b, 0);
    luaL_pushresult(&b);

    return 1;
}
