// SPDX-License-Identifier: MIT
#include "parser/grammars.h"

namespace Elica::Parsing {

using Earley::Grammar;

namespace {

void addSharedExpressionRules(Grammar &g) {
    // ---------------- expressions (precedence climbing via levels) --------
    g.addRule("expr", {"assign_expr"});

    g.addRule("assign_expr", {"ternary"});
    g.addRule("assign_expr", {"ternary", "assign_op", "assign_expr"});
    for (const char *op : {"=",  "+=", "-=", "*=", "/=", "%=",
                           "&=", "|=", "^=", "<<=", ">>="}) {
        g.addRule("assign_op", {op});
    }

    g.addRule("ternary", {"logical_or"});
    g.addRule("ternary", {"logical_or", "?", "expr", ":", "expr"});

    g.addRule("logical_or", {"logical_or", "||", "logical_and"});
    g.addRule("logical_or", {"logical_and"});
    g.addRule("logical_and", {"logical_and", "&&", "bit_or"});
    g.addRule("logical_and", {"bit_or"});
    g.addRule("bit_or", {"bit_or", "|", "bit_xor"});
    g.addRule("bit_or", {"bit_xor"});
    g.addRule("bit_xor", {"bit_xor", "^", "bit_and"});
    g.addRule("bit_xor", {"bit_and"});
    g.addRule("bit_and", {"bit_and", "&", "equality"});
    g.addRule("bit_and", {"equality"});

    g.addRule("equality", {"equality", "eq_op", "relational"});
    g.addRule("equality", {"relational"});
    g.addRule("eq_op", {"=="});
    g.addRule("eq_op", {"!="});
    g.addRule("relational", {"relational", "rel_op", "shift"});
    g.addRule("relational", {"shift"});
    for (const char *op : {"<", ">", "<=", ">="}) g.addRule("rel_op", {op});
    g.addRule("shift", {"shift", "shift_op", "additive"});
    g.addRule("shift", {"additive"});
    for (const char *op : {"<<", ">>"}) g.addRule("shift_op", {op});
    g.addRule("additive", {"additive", "add_op", "multiplicative"});
    g.addRule("additive", {"multiplicative"});
    for (const char *op : {"+", "-"}) g.addRule("add_op", {op});
    g.addRule("multiplicative", {"multiplicative", "mul_op", "unary"});
    g.addRule("multiplicative", {"unary"});
    for (const char *op : {"*", "/", "%"}) g.addRule("mul_op", {op});

    // unary / postfix / primary
    g.addRule("unary", {"unary_op", "unary"});
    g.addRule("unary", {"new_expr"});
    g.addRule("unary", {"sizeof_expr"});
    g.addRule("unary", {"delete_expr"});
    g.addRule("unary", {"postfix"});
    for (const char *op : {"-", "+", "!", "~", "&", "*", "++", "--"}) {
        g.addRule("unary_op", {op});
    }
    g.addRule("sizeof_expr", {"sizeof", "(", "type", ")"});
    g.addRule("sizeof_expr", {"sizeof", "unary"});
    g.addRule("delete_expr", {"delete", "unary"});
    g.addRule("delete_expr", {"delete", "[", "]", "unary"});

    g.addRule("new_expr", {"new", "new_body"});
    g.addRule("new_body", {"type", "arr_suffix", "brace_init?"});
    g.addRule("new_body", {"primary", "(", "args", ")"});
    g.addRule("new_body", {"primary", "brace_init"});

    g.addRule("postfix", {"postfix", "(", "args", ")"});
    g.addRule("postfix", {"postfix", "[", "expr", "]"});
    g.addRule("postfix", {"postfix", ".", "id"});
    g.addRule("postfix", {"postfix", "->", "id"});
    g.addRule("postfix", {"postfix", "::", "id"});
    g.addRule("postfix", {"postfix", "++"});
    g.addRule("postfix", {"postfix", "--"});
    g.addRule("postfix", {"postfix", "<", "template_args", ">", "(", "args", ")"});
    g.addRule("postfix", {"postfix", "<", "template_args", ">>", "(", "args", ")"});
    g.addRule("postfix", {"primary"});

    g.addRule("template_args", {"template_args", ",", "template_arg"});
    g.addRule("template_args", {"template_arg"});
    g.addRule("template_arg", {"type"});
    g.addRule("template_arg", {"num"});

    g.addRule("primary", {"id"});
    g.addRule("primary", {"num"});
    g.addRule("primary", {"str_lit"});
    g.addRule("primary", {"char_lit"});
    g.addRule("primary", {"true"});
    g.addRule("primary", {"false"});
    g.addRule("primary", {"this"});
    g.addRule("primary", {"nullptr"});
    g.addRule("primary", {"(", "expr", ")"});
    g.addRule("primary", {"cpp_code"});
    // C++ casts: static_cast<T>(x) etc.
    for (const char *cast : {"static_cast", "reinterpret_cast", "const_cast", "dynamic_cast"}) {
        g.addRule("primary", {cast, "<", "type", ">", "(", "expr", ")"});
    }
    // lambdas: [cap](params) -> ret { body }
    g.addRule("primary", {"lambda_expr"});
    g.addRule("lambda_expr", {"[", "lambda_capture", "]", "lambda_params", "lambda_ret", "block"});
    g.addRule("lambda_params", {"(", "c_param_list", ")"});
    g.addRule("lambda_params", {});
    g.addRule("lambda_ret", {"->", "type"});
    g.addRule("lambda_ret", {});
    g.addRule("lambda_capture", {"lambda_capture_list"});
    g.addRule("lambda_capture", {});
    g.addRule("lambda_capture_list", {"lambda_capture_list", ",", "lambda_capture_item"});
    g.addRule("lambda_capture_list", {"lambda_capture_item"});
    g.addRule("lambda_capture_item", {"id"});
    g.addRule("lambda_capture_item", {"&", "id"});
    g.addRule("lambda_capture_item", {"this"});
    g.addRule("lambda_capture_item", {"="});
    g.addRule("lambda_capture_item", {"&"});

    g.addRule("args", {"args_list"});
    g.addRule("args", {});
    g.addRule("args_list", {"args_list", ",", "expr"});
    g.addRule("args_list", {"expr"});

    // initializer lists
    g.addRule("brace_init?", {});
    g.addRule("brace_init?", {"brace_init"});
    g.addRule("brace_init", {"{", "brace_items", "}"});
    g.addRule("brace_items", {"brace_items", ",", "brace_item"});
    g.addRule("brace_items", {"brace_item"});
    g.addRule("brace_items", {});
    g.addRule("brace_item", {"expr"});
    g.addRule("brace_item", {"brace_init"});
}

void addSharedStatementRules(Grammar &g) {
    // ---------------- types ----------------------------------------------
    g.addRule("type", {"type_tail", "ptr_tail"});
    // type_tail deliberately does NOT chain arbitrary words: `size_type kMax`
    // must split as type=size_type, declarator=kMax. Only const/volatile and
    // the fixed multi-word C/C++ types chain.
    g.addRule("type_tail", {"type_word"});
    g.addRule("type_tail", {"type_multi"});
    g.addRule("type_tail", {"const", "type_tail"});
    g.addRule("type_tail", {"volatile", "type_tail"});
    for (const char *words : {"unsigned int", "unsigned long", "unsigned long long",
                              "unsigned short", "unsigned char", "signed int",
                              "signed long", "signed long long", "signed short",
                              "signed char", "long long", "long double",
                              "short int", "long int"}) {
        std::vector<::Elica::Parsing::Earley::Symbol> rhs;
        {
            std::string w(words);
            size_t p = 0;
            for (;;) {
                size_t sp = w.find(' ', p);
                rhs.push_back((sp == std::string::npos) ? w.substr(p) : w.substr(p, sp - p));
                if (sp == std::string::npos) break;
                p = sp + 1;
            }
        }
        g.addRule("type_multi", rhs);
    }
    g.addRule("ptr_tail", {"*", "ptr_tail"});
    g.addRule("ptr_tail", {"&", "ptr_tail"});
    g.addRule("ptr_tail", {"&&", "ptr_tail"});
    g.addRule("ptr_tail", {});
    g.addRule("type_word", {"type_kw"});
    g.addRule("type_word", {"id_path"});
    // template instantiation: std::vector<int>, std::vector<std::vector<int>>
    g.addRule("type_word", {"id_path", "<", "template_args", ">"});
    g.addRule("type_word", {"id_path", "<", "template_args", ">>"});
    g.addRule("type_word", {"struct", "id_path"});
    g.addRule("type_word", {"enum", "id_path"});
    g.addRule("type_word", {"class", "id_path"});
    g.addRule("type_word", {"typename", "id_path"});
    g.addRule("type_word", {"typename", "id_path", "<", "template_args", ">", "::", "id_path"});
    for (const char *kw : {"int", "void", "char", "bool", "str", "flt", "double",
                           "float", "byte", "ubyte", "short", "ushort", "uint",
                           "more", "umore", "any", "sentinel", "atom", "noised",
                           "unsigned", "signed", "long", "auto", "wchar_t",
                           "char16_t", "char32_t", "size_t"}) {
        g.addRule("type_kw", {kw});
    }
    g.addRule("id_path", {"id"});
    g.addRule("id_path", {"id_path", ".", "id"});
    g.addRule("id_path", {"id_path", "::", "id"});

    // ---------------- declarators / variables ----------------------------
    g.addRule("var_decl", {"type", "init_declarator", "decl_rest", ";"});
    g.addRule("decl_rest", {"decl_rest", ",", "init_declarator"});
    g.addRule("decl_rest", {});
    g.addRule("init_declarator", {"ptrs", "id", "arr_suffix", "init_part"});
    g.addRule("init_declarator", {"ptrs", "(", "*", "id", ")", "(", "c_param_list", ")", "cv_qual"});
    g.addRule("init_declarator", {"ptrs", "(", "&", "id", ")", "(", "c_param_list", ")", "cv_qual"});
    g.addRule("ptrs", {"*", "ptrs"});
    g.addRule("ptrs", {"&", "ptrs"});
    g.addRule("ptrs", {"&&", "ptrs"});
    g.addRule("ptrs", {});
    g.addRule("arr_suffix", {"[", "expr", "]"});
    g.addRule("arr_suffix", {"[", "]"});
    g.addRule("arr_suffix", {});
    g.addRule("init_part", {"=", "expr"});
    g.addRule("init_part", {"(", "args", ")"});
    g.addRule("init_part", {});

    // ---------------- statements ------------------------------------------
    g.addRule("stmt", {"var_decl"});
    g.addRule("stmt", {"expr_stmt"});
    g.addRule("stmt", {"block"});
    g.addRule("stmt", {"if_stmt"});
    g.addRule("stmt", {"while_stmt"});
    g.addRule("stmt", {"do_stmt"});
    g.addRule("stmt", {"for_stmt"});
    g.addRule("stmt", {"switch_stmt"});
    g.addRule("stmt", {"return_stmt"});
    g.addRule("stmt", {"break_stmt"});
    g.addRule("stmt", {"continue_stmt"});
    g.addRule("stmt", {"goto_stmt"});
    g.addRule("stmt", {"label_stmt"});
    g.addRule("stmt", {"empty_stmt"});
    g.addRule("stmt", {"try_stmt"});
    g.addRule("stmt", {"cpp_code"});

    g.addRule("expr_stmt", {"expr", ";"});
    g.addRule("block", {"{", "stmt_list", "}"});
    g.addRule("stmt_list", {"stmt_list", "stmt"});
    g.addRule("stmt_list", {});
    g.addRule("if_stmt", {"if", "(", "expr", ")", "stmt"});
    g.addRule("if_stmt", {"if", "(", "expr", ")", "stmt", "else", "stmt"});
    g.addRule("while_stmt", {"while", "(", "expr", ")", "stmt"});
    g.addRule("do_stmt", {"do", "stmt", "while", "(", "expr", ")", ";"});
    g.addRule("for_stmt", {"for", "(", "for_init", "expr_opt", ";", "expr_opt", ")", "stmt"});
    g.addRule("switch_stmt", {"switch", "(", "expr", ")", "stmt"});
    g.addRule("case_label", {"case", "expr", ":", "stmt"});
    g.addRule("case_label", {"default", ":", "stmt"});
    // case/default appear as statements inside switch bodies
    g.addRule("stmt", {"case_label"});
    g.addRule("goto_stmt", {"goto", "id", ";"});
    g.addRule("label_stmt", {"id", ":", "stmt"});
    g.addRule("try_stmt", {"try", "block", "catch_clause_list"});
    g.addRule("catch_clause_list", {"catch_clause_list", "catch_clause"});
    g.addRule("catch_clause_list", {"catch_clause"});
    g.addRule("catch_clause", {"catch", "(", "c_param", ")", "block"});
    g.addRule("catch_clause", {"catch", "(", "ellipsis_tok", ")", "block"});
    g.addRule("ellipsis_tok", {"..."});

    g.addRule("for_init", {"var_decl"});
    g.addRule("for_init", {"expr", ";"});
    g.addRule("for_init", {";"});
    g.addRule("expr_opt", {"expr"});
    g.addRule("expr_opt", {});
    g.addRule("return_stmt", {"return", "expr_opt", ";"});
    g.addRule("break_stmt", {"break", ";"});
    g.addRule("continue_stmt", {"continue", ";"});
    g.addRule("empty_stmt", {";"});

    // ---------------- C-like parameters (shared C / C++) ------------------
    g.addRule("c_param_list", {"c_param_list", ",", "c_param"});
    g.addRule("c_param_list", {"c_param"});
    g.addRule("c_param_list", {"void"});
    g.addRule("c_param_list", {});
    g.addRule("c_param", {"type"});
    g.addRule("c_param", {"type", "init_declarator"});

    // ---------------- enums / typedefs (shared C / C++) -------------------
    g.addRule("c_enum_decl", {"enum", "id", "{", "enum_items", "}", ";"});
    g.addRule("c_enum_decl", {"enum", "id", "{", "enum_items", "}", "init_declarator", ";"});
    g.addRule("c_enum_decl", {"enum", "id", ";"});
    g.addRule("c_enum_decl", {"enum", "class", "id", "{", "enum_items", "}", ";"});
    g.addRule("c_enum_decl", {"enum", "class", "id", ":", "type", "{", "enum_items", "}", ";"});
    g.addRule("c_enum_decl", {"enum", "struct", "id", "{", "enum_items", "}", ";"});
    g.addRule("c_enum_decl", {"enum", "struct", "id", ":", "type", "{", "enum_items", "}", ";"});
    g.addRule("c_enum_decl", {"enum", "id", ":", "type", "{", "enum_items", "}", ";"});
    g.addRule("enum_items", {"enum_items", ",", "id"});
    g.addRule("enum_items", {"enum_items", ",", "id", "=", "expr"});
    g.addRule("enum_items", {"id"});
    g.addRule("enum_items", {"id", "=", "expr"});
    g.addRule("enum_items", {});
    g.addRule("c_typedef_decl", {"typedef", "type", "init_declarator", ";"});
}

} // namespace

// ===========================================================================
// Ely
// ===========================================================================

::Elica::Parsing::Earley::Grammar buildElyGrammar() {
    Grammar g;
    addSharedExpressionRules(g);
    addSharedStatementRules(g);

    g.start = "ely_program";
    g.addRule("ely_program", {"ely_top_list"});
    g.addRule("ely_top_list", {"ely_top_list", "ely_top"});
    g.addRule("ely_top_list", {});

    g.addRule("ely_top", {"using_decl"});
    g.addRule("ely_top", {"ely_func_decl"});
    g.addRule("ely_top", {"var_decl"});
    g.addRule("ely_top", {"ely_class_decl"});
    g.addRule("ely_top", {"struct_decl"});
    g.addRule("ely_top", {"cpp_code"});
    g.addRule("ely_top", {";"});

    g.addRule("using_decl", {"using", "using_target", ";"});
    g.addRule("using_target", {"id_path"});
    g.addRule("using_target", {"str_lit"});

    g.addRule("ely_func_decl", {"ely_mods", "type", "func", "id", "(", "param_list", ")", "block"});
    g.addRule("ely_func_decl", {"ely_mods", "type", "func", "id", "(", "param_list", ")", ";"});
    g.addRule("ely_mods", {"ely_mods", "ely_mod"});
    g.addRule("ely_mods", {});
    for (const char *m : {"public", "private", "mine", "static", "const", "sealed",
                          "abstract", "wait", "unwait", "stack", "heap", "copa", "nocopa"}) {
        g.addRule("ely_mod", {m});
    }

    g.addRule("param_list", {"param_list", ",", "param"});
    g.addRule("param_list", {"param"});
    g.addRule("param_list", {});
    g.addRule("param", {"type", "id"});
    g.addRule("param", {"type", "id", "=", "expr"});

    g.addRule("ely_class_decl", {"ely_mods", "class", "id", "{", "ely_class_members", "}"});
    g.addRule("ely_class_members", {"ely_class_members", "ely_class_member"});
    g.addRule("ely_class_members", {});
    g.addRule("ely_class_member", {"ely_func_decl"});
    g.addRule("ely_class_member", {"var_decl"});

    g.addRule("struct_decl", {"ely_mods", "struct", "id", "{", "var_decl_list", "}"});
    g.addRule("var_decl_list", {"var_decl_list", "var_decl"});
    g.addRule("var_decl_list", {});
    return g;
}

// ===========================================================================
// C
// ===========================================================================

::Elica::Parsing::Earley::Grammar buildCGrammar() {
    Grammar g;
    addSharedExpressionRules(g);
    addSharedStatementRules(g);

    g.start = "c_program";
    g.addRule("c_program", {"c_top_list"});
    g.addRule("c_top_list", {"c_top_list", "c_top"});
    g.addRule("c_top_list", {});

    g.addRule("c_top", {"c_func_decl"});
    g.addRule("c_top", {"var_decl"});
    g.addRule("c_top", {"c_struct_decl"});
    g.addRule("c_top", {"c_enum_decl"});
    g.addRule("c_top", {"c_typedef_decl"});
    g.addRule("c_top", {"cpp_code"});
    g.addRule("c_top", {";"});

    g.addRule("c_func_decl", {"c_mods", "type", "id", "(", "c_param_list", ")", "block"});
    g.addRule("c_func_decl", {"c_mods", "type", "id", "(", "c_param_list", ")", ";"});
    g.addRule("c_mods", {"c_mods", "c_mod"});
    g.addRule("c_mods", {});
    for (const char *m : {"static", "const", "extern", "inline", "volatile", "register"}) {
        g.addRule("c_mod", {m});
    }

    g.addRule("c_struct_decl", {"struct", "id", "{", "c_field_list", "}", ";"});
    g.addRule("c_struct_decl", {"struct", "id", ";"});
    g.addRule("c_field_list", {"c_field_list", "c_field"});
    g.addRule("c_field_list", {});
    g.addRule("c_field", {"type", "init_declarator", ";"});

    return g;
}

// ===========================================================================
// C++
// ===========================================================================

::Elica::Parsing::Earley::Grammar buildCppGrammar() {
    Grammar g;
    addSharedExpressionRules(g);
    addSharedStatementRules(g);

    g.start = "cpp_program";
    g.addRule("cpp_program", {"cpp_top_list"});
    g.addRule("cpp_top_list", {"cpp_top_list", "cpp_top"});
    g.addRule("cpp_top_list", {});

    g.addRule("cpp_top", {"cpp_func_decl"});
    g.addRule("cpp_top", {"var_decl"});
    g.addRule("cpp_top", {"cpp_mods", "var_decl"});
    g.addRule("cpp_top", {"cpp_class_decl"});
    g.addRule("cpp_top", {"cpp_struct_decl"});
    g.addRule("cpp_top", {"namespace_decl"});
    g.addRule("cpp_top", {"c_enum_decl"});
    g.addRule("cpp_top", {"c_typedef_decl"});
    g.addRule("cpp_top", {"cpp_using_decl"});
    g.addRule("cpp_top", {"template_decl"});
    g.addRule("cpp_top", {"extern_block"});
    g.addRule("cpp_top", {"cpp_code"});
    g.addRule("cpp_top", {";"});

    // ---- functions / constructors / destructors / operators --------------
    g.addRule("cpp_func_decl", {"cpp_mods", "type", "id_path", "(", "c_param_list", ")",
                                "cv_qual", "block_or_semi"});
    g.addRule("cpp_func_decl", {"cpp_mods", "id", "(", "c_param_list", ")",
                                "cv_qual", "ctor_init", "block_or_semi"});
    g.addRule("cpp_func_decl", {"cpp_mods", "~", "id", "(", ")", "cv_qual", "block_or_semi"});
    g.addRule("cpp_func_decl", {"cpp_mods", "type", "operator", "op_sym", "(", "c_param_list", ")",
                                "cv_qual", "block_or_semi"});
    g.addRule("cpp_func_decl", {"cpp_mods", "operator", "op_sym", "(", "c_param_list", ")",
                                "cv_qual", "block_or_semi"});

    g.addRule("cpp_mods", {"cpp_mods", "cpp_mod"});
    g.addRule("cpp_mods", {});
    for (const char *m : {"public", "private", "protected", "static", "const",
                          "virtual", "explicit", "inline", "constexpr", "friend",
                          "extern", "mutable", "volatile", "register", "constinit"}) {
        g.addRule("cpp_mod", {m});
    }

    g.addRule("op_sym", {"+"});   g.addRule("op_sym", {"-"});
    g.addRule("op_sym", {"*"});   g.addRule("op_sym", {"/"});
    g.addRule("op_sym", {"%"});   g.addRule("op_sym", {"=="});
    g.addRule("op_sym", {"!="});  g.addRule("op_sym", {"<"});
    g.addRule("op_sym", {">"});   g.addRule("op_sym", {"<="});
    g.addRule("op_sym", {">="});  g.addRule("op_sym", {"&&"});
    g.addRule("op_sym", {"||"});  g.addRule("op_sym", {"!"});
    g.addRule("op_sym", {"&"});   g.addRule("op_sym", {"|"});
    g.addRule("op_sym", {"^"});   g.addRule("op_sym", {"~"});
    g.addRule("op_sym", {"<<"});  g.addRule("op_sym", {">>"});
    g.addRule("op_sym", {"++"});  g.addRule("op_sym", {"--"});
    g.addRule("op_sym", {"="});   g.addRule("op_sym", {"new"});
    g.addRule("op_sym", {"delete"}); g.addRule("op_sym", {"(", ")"});
    g.addRule("op_sym", {"[", "]"});

    g.addRule("cv_qual", {"cv_qual", "cv"});
    g.addRule("cv_qual", {});
    for (const char *c : {"const", "volatile", "noexcept", "override", "final"}) g.addRule("cv", {c});
    g.addRule("block_or_semi", {"block"});
    g.addRule("block_or_semi", {";"});
    g.addRule("block_or_semi", {"=", "default", ";"});
    g.addRule("block_or_semi", {"=", "delete", ";"});
    g.addRule("block_or_semi", {"=", "num", ";"});    // pure virtual: = 0

    g.addRule("ctor_init", {":", "mem_init_list"});
    g.addRule("ctor_init", {});
    g.addRule("mem_init_list", {"mem_init_list", ",", "mem_init"});
    g.addRule("mem_init_list", {"mem_init"});
    g.addRule("mem_init", {"id", "(", "args", ")"});
    g.addRule("mem_init", {"id", "brace_init"});

    // ---- classes ----------------------------------------------------------
    g.addRule("cpp_class_decl", {"cpp_class_head", "{", "cpp_members", "}", ";"});
    g.addRule("cpp_class_decl", {"cpp_class_head", "{", "cpp_members", "}"});
    g.addRule("cpp_class_head", {"cpp_mods", "class", "id", "class_bases"});
    g.addRule("cpp_class_head", {"cpp_mods", "struct", "id", "class_bases"});
    g.addRule("cpp_class_head", {"cpp_mods", "union", "id", "class_bases"});
    g.addRule("cpp_struct_decl", {"cpp_class_head", "{", "cpp_members", "}", ";"});
    g.addRule("cpp_struct_decl", {"cpp_class_head", "{", "cpp_members", "}"});

    g.addRule("class_bases", {":", "base_list"});
    g.addRule("class_bases", {});
    g.addRule("base_list", {"base_list", ",", "base"});
    g.addRule("base_list", {"base"});
    g.addRule("base", {"public", "id_path"});
    g.addRule("base", {"protected", "id_path"});
    g.addRule("base", {"private", "id_path"});
    g.addRule("base", {"virtual", "base"});
    g.addRule("base", {"id_path"});

    g.addRule("cpp_members", {"cpp_members", "cpp_member"});
    g.addRule("cpp_members", {});
    g.addRule("cpp_member", {"access_spec"});
    g.addRule("cpp_member", {"cpp_func_decl"});
    g.addRule("cpp_member", {"var_decl"});
    g.addRule("cpp_member", {"cpp_mods", "var_decl"});
    g.addRule("cpp_member", {"cpp_class_decl"});
    g.addRule("cpp_member", {"c_enum_decl"});
    g.addRule("cpp_member", {"c_typedef_decl"});
    g.addRule("cpp_member", {"cpp_using_decl"});
    g.addRule("cpp_member", {"template_decl"});
    g.addRule("cpp_member", {";"});
    g.addRule("access_spec", {"public", ":"});
    g.addRule("access_spec", {"private", ":"});
    g.addRule("access_spec", {"protected", ":"});

    // ---- namespaces / using / templates / extern -------------------------
    g.addRule("namespace_decl", {"namespace", "id", "{", "cpp_top_list", "}"});
    g.addRule("namespace_decl", {"namespace", "id", ";"});
    g.addRule("cpp_using_decl", {"using", "namespace", "id_path", ";"});
    g.addRule("cpp_using_decl", {"using", "id_path", ";"});
    g.addRule("cpp_using_decl", {"using", "id_path", "=", "type", ";"});
    g.addRule("template_decl", {"template", "<", "template_params", ">", "cpp_top"});
    g.addRule("template_decl", {"template", "<", "template_params", ">>", "cpp_top"});
    g.addRule("template_params", {"template_params", ",", "template_param"});
    g.addRule("template_params", {"template_param"});
    g.addRule("template_params", {});
    g.addRule("template_param", {"typename", "id"});
    g.addRule("template_param", {"typename", "id", "=", "template_default"});
    g.addRule("template_param", {"class", "id"});
    g.addRule("template_param", {"class", "id", "=", "template_default"});
    g.addRule("template_param", {"type", "id"});
    g.addRule("template_param", {"type", "id", "=", "template_default"});
    g.addRule("template_param", {"id"});
    g.addRule("template_param", {"id", "=", "template_default"});
    g.addRule("template_default", {"type"});
    g.addRule("template_default", {"expr"});
    g.addRule("extern_block", {"extern", "str_lit", "{", "cpp_top_list", "}"});

    return g;
}

} // namespace Elica::Parsing