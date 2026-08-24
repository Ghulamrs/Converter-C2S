#include "CParser.h"

#include "../Diagnostics.h"
#include "../Source.h"

namespace c2s {

namespace {

const CToken kEndToken;

// C's binary operator precedence, tightest first as the usual table has it;
// the climb in binary() works from loosest, so the numbers only need order.
// Assignment and the conditional are handled by their own methods - they
// associate rightward - and the comma by expression().
int precedenceOf(const std::string &op) {
    if (op == "*" || op == "/" || op == "%") return 10;
    if (op == "+" || op == "-") return 9;
    if (op == "<<" || op == ">>") return 8;
    if (op == "<" || op == ">" || op == "<=" || op == ">=") return 7;
    if (op == "==" || op == "!=") return 6;
    if (op == "&") return 5;
    if (op == "^") return 4;
    if (op == "|") return 3;
    if (op == "&&") return 2;
    if (op == "||") return 1;
    return 0;
}

bool isAssignOp(const std::string &op) {
    return op == "=" || op == "+=" || op == "-=" || op == "*=" || op == "/=" ||
           op == "%=" || op == "<<=" || op == ">>=" || op == "&=" || op == "^=" ||
           op == "|=";
}

}  // namespace

CParser::CParser(const Source &source, std::vector<CToken> tokens,
                 Diagnostics &diagnostics)
    : source_(source), tokens_(std::move(tokens)), diagnostics_(diagnostics) {
    typedefScopes_.push_back(std::set<std::string>());
}

// ---------------------------------------------------------------- plumbing

const CToken &CParser::current() const {
    return index_ < tokens_.size() ? tokens_[index_] : kEndToken;
}

const CToken &CParser::peek(std::size_t ahead) const {
    const std::size_t i = index_ + ahead;
    return i < tokens_.size() ? tokens_[i] : kEndToken;
}

void CParser::advance() {
    if (index_ < tokens_.size()) ++index_;
}

bool CParser::at(const char *text) const {
    return current().kind == CTokenKind::Punct && current().text == text;
}

bool CParser::atKeyword(const char *text) const {
    return current().kind == CTokenKind::Keyword && current().text == text;
}

bool CParser::accept(const char *text) {
    if ((current().kind == CTokenKind::Punct || current().kind == CTokenKind::Keyword) &&
        current().text == text) {
        advance();
        return true;
    }
    return false;
}

bool CParser::expect(const char *text, const char *where) {
    if (accept(text)) return true;
    fail(std::string("expected '") + text + "' " + where + ", found '" +
         (current().kind == CTokenKind::End ? "end of file" : current().spelling) + "'");
    return false;
}

void CParser::fail(const std::string &message) {
    if (failed_) return;               // the first error is the real one
    failed_ = true;
    diagnostics_.report(Severity::SyntaxError, source_,
                        source_.locate(current().offset), "C1000", message);
}

bool CParser::isTypedefName(const std::string &name) const {
    for (std::size_t i = typedefScopes_.size(); i > 0; --i) {
        if (typedefScopes_[i - 1].count(name) != 0) return true;
    }
    return false;
}

bool CParser::atTypeStart() const {
    const CToken &token = current();
    if (token.kind == CTokenKind::Keyword) {
        return token.text == "void" || token.text == "char" || token.text == "int" ||
               token.text == "float" || token.text == "double" ||
               token.text == "short" || token.text == "long" ||
               token.text == "signed" || token.text == "unsigned" ||
               token.text == "struct" || token.text == "union" ||
               token.text == "enum" || token.text == "const" ||
               token.text == "volatile" || token.text == "typedef" ||
               token.text == "extern" || token.text == "static" ||
               token.text == "auto" || token.text == "register";
    }
    return token.kind == CTokenKind::Identifier && isTypedefName(token.text);
}

// ------------------------------------------------------------ declarations

CTypePtr CParser::structOrUnion() {
    const bool isUnion = atKeyword("union");
    advance();                          // 'struct' or 'union'

    CTypePtr type(new CType(isUnion ? CType::Kind::Union : CType::Kind::Struct));

    if (current().kind == CTokenKind::Identifier) {
        type->setTag(current().text);
        advance();
    }

    if (accept("{")) {
        type->setHasMemberList();
        while (!at("}") && !failed_) {
            Specifiers specifiers;
            bool sawAny = false;
            if (!declarationSpecifiers(&specifiers, &sawAny) || !sawAny) {
                fail("expected a member declaration");
                return type;
            }
            // One or more declarators, each possibly a bitfield.
            for (;;) {
                CType::Member member;
                if (!at(":")) {
                    CTypePtr memberType;
                    if (!declarator(specifiers.type->clone(), &member.name,
                                    &memberType, false)) {
                        return type;
                    }
                    member.type = std::move(memberType);
                } else {
                    member.type = specifiers.type->clone();
                }
                if (accept(":")) {
                    member.bitWidth.reset(conditional().release());
                }
                type->members().push_back(std::move(member));
                if (!accept(",")) break;
            }
            if (!expect(";", "after a member")) return type;
        }
        expect("}", "to close the member list");
    } else if (type->tag().empty()) {
        fail("a struct or union needs a tag or a member list");
    }

    return type;
}

CTypePtr CParser::enumSpecifier() {
    advance();                          // 'enum'

    CTypePtr type(new CType(CType::Kind::Enum));

    if (current().kind == CTokenKind::Identifier) {
        type->setTag(current().text);
        advance();
    }

    if (accept("{")) {
        type->setHasMemberList();
        while (!at("}") && !failed_) {
            if (current().kind != CTokenKind::Identifier) {
                fail("expected an enumerator name");
                return type;
            }
            CType::Enumerator enumerator;
            enumerator.name = current().text;
            advance();
            if (accept("=")) {
                enumerator.value.reset(conditional().release());
            }
            type->enumerators().push_back(std::move(enumerator));
            if (!accept(",")) break;
        }
        expect("}", "to close the enumerator list");
    } else if (type->tag().empty()) {
        fail("an enum needs a tag or an enumerator list");
    }

    return type;
}

bool CParser::declarationSpecifiers(Specifiers *out, bool *sawAny) {
    *sawAny = false;
    bool sawType = false;
    bool isConst = false;
    bool isVolatile = false;
    bool isUnsigned = false;
    bool isSignedWord = false;
    bool isShort = false;
    int longCount = 0;
    CTypePtr type;

    for (;;) {
        const CToken &token = current();
        if (token.kind == CTokenKind::Keyword) {
            if (token.text == "typedef" || token.text == "extern" ||
                token.text == "static" || token.text == "auto" ||
                token.text == "register") {
                if (out->storage != CDeclaration::Storage::None) {
                    fail("two storage classes in one declaration");
                    return false;
                }
                out->storage = token.text == "typedef" ? CDeclaration::Storage::Typedef
                             : token.text == "extern"  ? CDeclaration::Storage::Extern
                             : token.text == "static"  ? CDeclaration::Storage::Static
                             : token.text == "auto"    ? CDeclaration::Storage::Auto
                                                       : CDeclaration::Storage::Register;
                advance();
                *sawAny = true;
                continue;
            }
            if (token.text == "const") { isConst = true; advance(); *sawAny = true; continue; }
            if (token.text == "volatile") { isVolatile = true; advance(); *sawAny = true; continue; }
            if (token.text == "void" || token.text == "char" || token.text == "int" ||
                token.text == "float" || token.text == "double") {
                if (sawType) { fail("two type names in one declaration"); return false; }
                sawType = true;
                type.reset(new CType(token.text == "void" ? CType::Kind::Void
                                   : token.text == "char" ? CType::Kind::Char
                                   : token.text == "int" ? CType::Kind::Int
                                   : token.text == "float" ? CType::Kind::Float
                                                           : CType::Kind::Double));
                advance();
                *sawAny = true;
                continue;
            }
            if (token.text == "short") { isShort = true; advance(); *sawAny = true; continue; }
            if (token.text == "long") { ++longCount; advance(); *sawAny = true; continue; }
            if (token.text == "signed") { isSignedWord = true; advance(); *sawAny = true; continue; }
            if (token.text == "unsigned") { isUnsigned = true; advance(); *sawAny = true; continue; }
            if (token.text == "struct" || token.text == "union") {
                if (sawType) { fail("two type names in one declaration"); return false; }
                sawType = true;
                type = structOrUnion();
                *sawAny = true;
                continue;
            }
            if (token.text == "enum") {
                if (sawType) { fail("two type names in one declaration"); return false; }
                sawType = true;
                type = enumSpecifier();
                *sawAny = true;
                continue;
            }
            break;
        }
        if (token.kind == CTokenKind::Identifier && !sawType &&
            isTypedefName(token.text)) {
            // A typedef name is a type only when no type has been seen -
            // 'typedef int T; T T;' declares a variable T of type T.
            sawType = true;
            type.reset(new CType(CType::Kind::Named));
            type->setTag(token.text);
            advance();
            *sawAny = true;
            continue;
        }
        break;
    }

    if (!*sawAny) return true;

    // 'unsigned', 'short' and 'long' alone imply int, as C89 says.
    if (type == nullptr) {
        if (!isUnsigned && !isSignedWord && !isShort && longCount == 0) {
            fail("a declaration needs a type");
            return false;
        }
        type.reset(new CType(CType::Kind::Int));
    }
    if (isUnsigned) type->setUnsigned();
    if (isSignedWord) type->setSignedExplicit();
    if (isShort) type->setShort();
    if (longCount > 0) type->setLong();
    if (longCount > 1) {
        fail("'long long' is not C89");
        return false;
    }
    if (isConst) type->setConst();
    if (isVolatile) type->setVolatile();

    out->type = std::move(type);
    return true;
}

bool CParser::parameterList(CType *fn) {
    // At the '(' already consumed. '()' is an empty prototype; '(void)' says
    // no parameters; otherwise parameters, possibly ending ', ...'.
    if (accept(")")) return true;

    if (atKeyword("void") && peek(1).is(")")) {
        advance();
        advance();
        fn->setProtoVoid();
        return true;
    }

    for (;;) {
        if (accept("...")) {
            fn->setVariadic();
            break;
        }
        Specifiers specifiers;
        bool sawAny = false;
        if (!declarationSpecifiers(&specifiers, &sawAny) || !sawAny) {
            fail("expected a parameter declaration");
            return false;
        }
        CType::Param param;
        CTypePtr paramType;
        if (!declarator(std::move(specifiers.type), &param.name, &paramType, true)) {
            return false;
        }
        param.type = std::move(paramType);
        fn->params().push_back(std::move(param));
        if (!accept(",")) break;
    }
    return expect(")", "to close the parameter list");
}

bool CParser::directDeclarator(CTypePtr base, std::string *name, CTypePtr *out,
                               bool abstractAllowed) {
    // A parenthesised declarator wraps whatever suffixes follow it around
    // whatever the inside says - '(*f)(int)' is the classic. The inside is
    // parsed against a hole, and the hole is filled once the suffixes have
    // been wrapped around the base.
    if (at("(") &&
        (peek(1).is("*") || peek(1).kind == CTokenKind::Identifier ||
         peek(1).is("("))) {
        advance();
        // Parse the inner declarator against a placeholder; remember where
        // the placeholder sits so the suffixed base can replace it.
        CTypePtr hole(new CType(CType::Kind::Void));  // stands in for the base
        CTypePtr inner;
        if (!declarator(std::move(hole), name, &inner, abstractAllowed)) return false;
        if (!expect(")", "to close the declarator")) return false;

        // Suffixes bind to the *outer* declarator: arrays and functions.
        CTypePtr suffixed = std::move(base);
        for (;;) {
            if (at("[")) {
                advance();
                CTypePtr array(new CType(CType::Kind::Array));
                if (!at("]")) array->setLength(std::unique_ptr<CNode>(conditional().release()));
                if (!expect("]", "to close the array bound")) return false;
                array->setBase(std::move(suffixed));
                // Array suffixes nest outside-in; append at the innermost.
                if (suffixed != nullptr) {}
                suffixed = std::move(array);
                continue;
            }
            if (at("(")) {
                advance();
                CTypePtr fn(new CType(CType::Kind::Function));
                fn->setBase(std::move(suffixed));
                if (!parameterList(fn.get())) return false;
                suffixed = std::move(fn);
                continue;
            }
            break;
        }

        // Replace the placeholder inside 'inner' with 'suffixed'. The
        // placeholder is the unique Void leaf reached through base links.
        CType *walk = inner.get();
        if (walk->kind() == CType::Kind::Void && walk->base() == nullptr) {
            *out = std::move(suffixed);
        } else {
            while (walk->base() != nullptr &&
                   !(walk->base()->kind() == CType::Kind::Void &&
                     walk->base()->base() == nullptr)) {
                walk = walk->base();
            }
            walk->setBase(std::move(suffixed));
            *out = std::move(inner);
        }
        return true;
    }

    if (current().kind == CTokenKind::Identifier) {
        *name = current().text;
        advance();
    } else if (!abstractAllowed) {
        fail("expected a name in the declarator");
        return false;
    }

    CTypePtr type = std::move(base);
    // Array and function suffixes read left to right; each wraps the type
    // built so far, and nested arrays land in declaration order: 'a[2][3]'
    // is an array of 2 arrays of 3.
    std::vector<CTypePtr> arrays;
    for (;;) {
        if (at("[")) {
            advance();
            CTypePtr array(new CType(CType::Kind::Array));
            if (!at("]")) array->setLength(std::unique_ptr<CNode>(conditional().release()));
            if (!expect("]", "to close the array bound")) return false;
            arrays.push_back(std::move(array));
            continue;
        }
        if (at("(")) {
            advance();
            CTypePtr fn(new CType(CType::Kind::Function));
            if (!parameterList(fn.get())) return false;
            // A function suffix binds before any arrays gathered so far
            // cannot exist - C89 has no arrays of functions - so arrays here
            // means the declaration is malformed; let the type speak anyway.
            fn->setBase(std::move(type));
            type = std::move(fn);
            continue;
        }
        break;
    }
    // 'int a[2][3]': the FIRST bracket is the outermost array.
    for (std::size_t i = arrays.size(); i > 0; --i) {
        arrays[i - 1]->setBase(std::move(type));
        type = std::move(arrays[i - 1]);
    }

    *out = std::move(type);
    return true;
}

bool CParser::declarator(CTypePtr base, std::string *name, CTypePtr *out,
                         bool abstractAllowed) {
    // Pointers wrap the base before the direct declarator sees it.
    while (at("*")) {
        advance();
        CTypePtr pointer(new CType(CType::Kind::Pointer));
        pointer->setBase(std::move(base));
        while (atKeyword("const") || atKeyword("volatile")) {
            if (atKeyword("const")) pointer->setConst();
            else pointer->setVolatile();
            advance();
        }
        base = std::move(pointer);
    }
    return directDeclarator(std::move(base), name, out, abstractAllowed);
}

CTypePtr CParser::typeName() {
    Specifiers specifiers;
    bool sawAny = false;
    if (!declarationSpecifiers(&specifiers, &sawAny) || !sawAny) {
        fail("expected a type name");
        return nullptr;
    }
    std::string name;
    CTypePtr type;
    if (!declarator(std::move(specifiers.type), &name, &type, true)) return nullptr;
    if (!name.empty()) fail("a type name must not declare '" + name + "'");
    return type;
}

bool CParser::initializer(CInit *out) {
    if (accept("{")) {
        while (!at("}") && !failed_) {
            CInit item;
            if (!initializer(&item)) return false;
            out->add(std::move(item));
            if (!accept(",")) break;
        }
        return expect("}", "to close the initialiser list");
    }
    CExprPtr value = assignment();
    if (value == nullptr) return false;
    *out = CInit(std::move(value));
    return true;
}

std::unique_ptr<CDeclaration> CParser::declaration(
    Specifiers specifiers, bool *wasFunctionDef,
    std::unique_ptr<CFunctionDef> *fnOut) {
    std::unique_ptr<CDeclaration> decl(new CDeclaration());
    decl->setOffset(current().offset);
    decl->setStorage(specifiers.storage);

    // 'struct point { ... };' - a shape with nothing declared of it.
    if (at(";")) {
        advance();
        decl->setBareType(std::move(specifiers.type));
        return decl;
    }

    bool first = true;
    for (;;) {
        CDeclaration::Declarator declarator;
        declarator.offset = current().offset;
        CTypePtr type;
        if (!this->declarator(specifiers.type->clone(), &declarator.name, &type, false)) {
            return nullptr;
        }
        declarator.type = std::move(type);

        // A function definition: first declarator, function type, then '{'.
        if (first && wasFunctionDef != nullptr &&
            declarator.type->kind() == CType::Kind::Function && at("{")) {
            *wasFunctionDef = true;
            const std::size_t offset = declarator.offset;
            typedefScopes_.push_back(std::set<std::string>());
            CStmtPtr body = compound();
            typedefScopes_.pop_back();
            if (body == nullptr) return nullptr;
            std::unique_ptr<CCompound> block(static_cast<CCompound *>(body.release()));
            fnOut->reset(new CFunctionDef(
                declarator.name, std::move(declarator.type), std::move(block),
                specifiers.storage == CDeclaration::Storage::Static));
            (*fnOut)->setOffset(offset);
            return nullptr;
        }
        first = false;

        if (accept("=")) {
            declarator.init.reset(new CInit());
            if (!initializer(declarator.init.get())) return nullptr;
        }

        if (specifiers.storage == CDeclaration::Storage::Typedef) {
            typedefScopes_.back().insert(declarator.name);
        }

        decl->add(std::move(declarator));
        if (!accept(",")) break;
    }

    if (!expect(";", "after the declaration")) return nullptr;
    return decl;
}

// -------------------------------------------------------------- statements

CStmtPtr CParser::compound() {
    const std::size_t offset = current().offset;
    if (!expect("{", "to open the block")) return nullptr;

    std::unique_ptr<CCompound> block(new CCompound());
    block->setOffset(offset);

    typedefScopes_.push_back(std::set<std::string>());
    while (!at("}") && current().kind != CTokenKind::End && !failed_) {
        CStmtPtr item = blockItem();
        if (item == nullptr) {
            typedefScopes_.pop_back();
            return nullptr;
        }
        block->add(std::move(item));
    }
    typedefScopes_.pop_back();

    if (!expect("}", "to close the block")) return nullptr;
    return CStmtPtr(block.release());
}

CStmtPtr CParser::blockItem() {
    if (atTypeStart()) {
        Specifiers specifiers;
        bool sawAny = false;
        if (!declarationSpecifiers(&specifiers, &sawAny)) return nullptr;
        std::unique_ptr<CDeclaration> decl = declaration(std::move(specifiers),
                                                         nullptr, nullptr);
        if (decl == nullptr) return nullptr;
        CStmtPtr stmt(new CDeclStmt(std::move(decl)));
        return stmt;
    }
    return statement();
}

CStmtPtr CParser::statement() {
    const std::size_t offset = current().offset;

    if (at("{")) return compound();

    if (at(";")) {
        advance();
        CStmtPtr stmt(new CEmpty());
        stmt->setOffset(offset);
        return stmt;
    }

    if (atKeyword("if")) {
        advance();
        if (!expect("(", "after 'if'")) return nullptr;
        CExprPtr cond = expression();
        if (cond == nullptr) return nullptr;
        if (!expect(")", "after the condition")) return nullptr;
        CStmtPtr thenArm = statement();
        if (thenArm == nullptr) return nullptr;
        CStmtPtr elseArm;
        if (accept("else")) {
            elseArm = statement();
            if (elseArm == nullptr) return nullptr;
        }
        CStmtPtr stmt(new CIf(std::move(cond), std::move(thenArm), std::move(elseArm)));
        stmt->setOffset(offset);
        return stmt;
    }

    if (atKeyword("while")) {
        advance();
        if (!expect("(", "after 'while'")) return nullptr;
        CExprPtr cond = expression();
        if (cond == nullptr) return nullptr;
        if (!expect(")", "after the condition")) return nullptr;
        CStmtPtr body = statement();
        if (body == nullptr) return nullptr;
        CStmtPtr stmt(new CWhile(std::move(cond), std::move(body)));
        stmt->setOffset(offset);
        return stmt;
    }

    if (atKeyword("do")) {
        advance();
        CStmtPtr body = statement();
        if (body == nullptr) return nullptr;
        if (!accept("while")) { fail("expected 'while' after the do body"); return nullptr; }
        if (!expect("(", "after 'while'")) return nullptr;
        CExprPtr cond = expression();
        if (cond == nullptr) return nullptr;
        if (!expect(")", "after the condition")) return nullptr;
        if (!expect(";", "after the do-while")) return nullptr;
        CStmtPtr stmt(new CDoWhile(std::move(body), std::move(cond)));
        stmt->setOffset(offset);
        return stmt;
    }

    if (atKeyword("for")) {
        advance();
        if (!expect("(", "after 'for'")) return nullptr;

        CStmtPtr init;
        if (at(";")) {
            advance();
        } else if (atTypeStart()) {
            // A declaration in a for header is C99; cc1 accepts it as an
            // extension, so it is parsed here and the conversion pass rules.
            Specifiers specifiers;
            bool sawAny = false;
            if (!declarationSpecifiers(&specifiers, &sawAny)) return nullptr;
            std::unique_ptr<CDeclaration> decl = declaration(std::move(specifiers),
                                                             nullptr, nullptr);
            if (decl == nullptr) return nullptr;
            init.reset(new CDeclStmt(std::move(decl)));
        } else {
            CExprPtr expr = expression();
            if (expr == nullptr) return nullptr;
            if (!expect(";", "after the for initialiser")) return nullptr;
            init.reset(new CExprStmt(std::move(expr)));
        }

        CExprPtr cond;
        if (!at(";")) {
            cond = expression();
            if (cond == nullptr) return nullptr;
        }
        if (!expect(";", "after the for condition")) return nullptr;

        CExprPtr step;
        if (!at(")")) {
            step = expression();
            if (step == nullptr) return nullptr;
        }
        if (!expect(")", "to close the for header")) return nullptr;

        CStmtPtr body = statement();
        if (body == nullptr) return nullptr;
        CStmtPtr stmt(new CFor(std::move(init), std::move(cond), std::move(step),
                               std::move(body)));
        stmt->setOffset(offset);
        return stmt;
    }

    if (atKeyword("switch")) {
        advance();
        if (!expect("(", "after 'switch'")) return nullptr;
        CExprPtr cond = expression();
        if (cond == nullptr) return nullptr;
        if (!expect(")", "after the switch expression")) return nullptr;
        CStmtPtr body = statement();
        if (body == nullptr) return nullptr;
        CStmtPtr stmt(new CSwitch(std::move(cond), std::move(body)));
        stmt->setOffset(offset);
        return stmt;
    }

    if (atKeyword("case")) {
        advance();
        CExprPtr value = conditional();
        if (value == nullptr) return nullptr;
        if (!expect(":", "after the case value")) return nullptr;
        CStmtPtr body = statement();
        if (body == nullptr) return nullptr;
        CStmtPtr stmt(new CCase(std::move(value), std::move(body)));
        stmt->setOffset(offset);
        return stmt;
    }

    if (atKeyword("default")) {
        advance();
        if (!expect(":", "after 'default'")) return nullptr;
        CStmtPtr body = statement();
        if (body == nullptr) return nullptr;
        CStmtPtr stmt(new CCase(nullptr, std::move(body)));
        stmt->setOffset(offset);
        return stmt;
    }

    if (atKeyword("break")) {
        advance();
        if (!expect(";", "after 'break'")) return nullptr;
        CStmtPtr stmt(new CBreak());
        stmt->setOffset(offset);
        return stmt;
    }

    if (atKeyword("continue")) {
        advance();
        if (!expect(";", "after 'continue'")) return nullptr;
        CStmtPtr stmt(new CContinue());
        stmt->setOffset(offset);
        return stmt;
    }

    if (atKeyword("return")) {
        advance();
        CExprPtr value;
        if (!at(";")) {
            value = expression();
            if (value == nullptr) return nullptr;
        }
        if (!expect(";", "after 'return'")) return nullptr;
        CStmtPtr stmt(new CReturn(std::move(value)));
        stmt->setOffset(offset);
        return stmt;
    }

    if (atKeyword("goto")) {
        advance();
        if (current().kind != CTokenKind::Identifier) {
            fail("expected a label after 'goto'");
            return nullptr;
        }
        std::string label = current().text;
        advance();
        if (!expect(";", "after the goto")) return nullptr;
        CStmtPtr stmt(new CGoto(std::move(label)));
        stmt->setOffset(offset);
        return stmt;
    }

    // 'name:' labels a statement; anything else is an expression statement.
    if (current().kind == CTokenKind::Identifier && peek(1).is(":")) {
        std::string name = current().text;
        advance();
        advance();
        CStmtPtr body = statement();
        if (body == nullptr) return nullptr;
        CStmtPtr stmt(new CLabel(std::move(name), std::move(body)));
        stmt->setOffset(offset);
        return stmt;
    }

    CExprPtr expr = expression();
    if (expr == nullptr) return nullptr;
    if (!expect(";", "after the expression")) return nullptr;
    CStmtPtr stmt(new CExprStmt(std::move(expr)));
    stmt->setOffset(offset);
    return stmt;
}

// ------------------------------------------------------------- expressions

CExprPtr CParser::expression() {
    CExprPtr left = assignment();
    if (left == nullptr) return nullptr;
    while (at(",")) {
        const std::size_t offset = current().offset;
        advance();
        CExprPtr right = assignment();
        if (right == nullptr) return nullptr;
        left.reset(new CComma(std::move(left), std::move(right)));
        left->setOffset(offset);
    }
    return left;
}

CExprPtr CParser::assignment() {
    CExprPtr left = conditional();
    if (left == nullptr) return nullptr;

    if (current().kind == CTokenKind::Punct && isAssignOp(current().text)) {
        const std::string op = current().text;
        const std::size_t offset = current().offset;
        advance();
        CExprPtr right = assignment();       // right-associative
        if (right == nullptr) return nullptr;
        left.reset(new CAssign(op, std::move(left), std::move(right)));
        left->setOffset(offset);
    }
    return left;
}

CExprPtr CParser::conditional() {
    CExprPtr cond = binary(1);
    if (cond == nullptr) return nullptr;

    if (at("?")) {
        const std::size_t offset = current().offset;
        advance();
        CExprPtr thenArm = expression();
        if (thenArm == nullptr) return nullptr;
        if (!expect(":", "in the conditional expression")) return nullptr;
        CExprPtr elseArm = conditional();
        if (elseArm == nullptr) return nullptr;
        cond.reset(new CTernary(std::move(cond), std::move(thenArm), std::move(elseArm)));
        cond->setOffset(offset);
    }
    return cond;
}

CExprPtr CParser::binary(int minPrecedence) {
    CExprPtr left = castExpression();
    if (left == nullptr) return nullptr;

    for (;;) {
        if (current().kind != CTokenKind::Punct) break;
        const int precedence = precedenceOf(current().text);
        if (precedence < minPrecedence || precedence == 0) break;

        const std::string op = current().text;
        const std::size_t offset = current().offset;
        advance();
        CExprPtr right = binary(precedence + 1);   // all left-associative
        if (right == nullptr) return nullptr;
        left.reset(new CBinary(op, std::move(left), std::move(right)));
        left->setOffset(offset);
    }
    return left;
}

CExprPtr CParser::castExpression() {
    // '(' type ')' cast-expression - told from a parenthesised expression by
    // what follows the '(': a type can only start with a keyword or a
    // typedef name.
    if (at("(")) {
        const CToken &next = peek(1);
        const bool looksLikeType =
            (next.kind == CTokenKind::Keyword &&
             (next.text == "void" || next.text == "char" || next.text == "int" ||
              next.text == "float" || next.text == "double" || next.text == "short" ||
              next.text == "long" || next.text == "signed" || next.text == "unsigned" ||
              next.text == "struct" || next.text == "union" || next.text == "enum" ||
              next.text == "const" || next.text == "volatile")) ||
            (next.kind == CTokenKind::Identifier && isTypedefName(next.text));
        if (looksLikeType) {
            const std::size_t offset = current().offset;
            advance();
            CTypePtr type = typeName();
            if (type == nullptr) return nullptr;
            if (!expect(")", "to close the cast")) return nullptr;
            CExprPtr operand = castExpression();
            if (operand == nullptr) return nullptr;
            CExprPtr cast(new CCast(std::move(type), std::move(operand)));
            cast->setOffset(offset);
            return cast;
        }
    }
    return unary();
}

CExprPtr CParser::unary() {
    const std::size_t offset = current().offset;

    if (at("++") || at("--")) {
        const std::string op = current().text;
        advance();
        CExprPtr operand = unary();
        if (operand == nullptr) return nullptr;
        CExprPtr expr(new CUnary(op, true, std::move(operand)));
        expr->setOffset(offset);
        return expr;
    }

    if (at("&") || at("*") || at("+") || at("-") || at("~") || at("!")) {
        const std::string op = current().text;
        advance();
        CExprPtr operand = castExpression();
        if (operand == nullptr) return nullptr;
        CExprPtr expr(new CUnary(op, true, std::move(operand)));
        expr->setOffset(offset);
        return expr;
    }

    if (atKeyword("sizeof")) {
        advance();
        if (at("(")) {
            const CToken &next = peek(1);
            const bool looksLikeType =
                (next.kind == CTokenKind::Keyword &&
                 next.text != "sizeof") ||
                (next.kind == CTokenKind::Identifier && isTypedefName(next.text));
            if (looksLikeType) {
                advance();
                CTypePtr type = typeName();
                if (type == nullptr) return nullptr;
                if (!expect(")", "to close sizeof")) return nullptr;
                CExprPtr expr(new CSizeof(std::move(type)));
                expr->setOffset(offset);
                return expr;
            }
        }
        CExprPtr operand = unary();
        if (operand == nullptr) return nullptr;
        CExprPtr expr(new CSizeof(std::move(operand)));
        expr->setOffset(offset);
        return expr;
    }

    return postfix();
}

CExprPtr CParser::postfix() {
    CExprPtr expr = primary();
    if (expr == nullptr) return nullptr;

    for (;;) {
        const std::size_t offset = current().offset;
        if (at("[")) {
            advance();
            CExprPtr index = expression();
            if (index == nullptr) return nullptr;
            if (!expect("]", "to close the subscript")) return nullptr;
            expr.reset(new CIndex(std::move(expr), std::move(index)));
            expr->setOffset(offset);
            continue;
        }
        if (at("(")) {
            advance();
            std::unique_ptr<CCall> call(new CCall(std::move(expr)));
            call->setOffset(offset);
            if (!at(")")) {
                for (;;) {
                    CExprPtr argument = assignment();
                    if (argument == nullptr) return nullptr;
                    call->add(std::move(argument));
                    if (!accept(",")) break;
                }
            }
            if (!expect(")", "to close the call")) return nullptr;
            expr.reset(call.release());
            continue;
        }
        if (at(".") || at("->")) {
            const bool arrow = at("->");
            advance();
            if (current().kind != CTokenKind::Identifier) {
                fail("expected a member name");
                return nullptr;
            }
            std::string name = current().text;
            advance();
            expr.reset(new CMember(std::move(expr), std::move(name), arrow));
            expr->setOffset(offset);
            continue;
        }
        if (at("++") || at("--")) {
            const std::string op = current().text;
            advance();
            expr.reset(new CUnary(op, false, std::move(expr)));
            expr->setOffset(offset);
            continue;
        }
        break;
    }
    return expr;
}

CExprPtr CParser::primary() {
    const CToken &token = current();
    const std::size_t offset = token.offset;

    switch (token.kind) {
        case CTokenKind::IntLiteral: {
            CExprPtr expr(new CIntLit(token.intValue, token.spelling));
            static_cast<CIntLit &>(*expr).setSuffixes(token.isUnsigned, token.isLong,
                                                      token.isHexOrOctal);
            expr->setOffset(offset);
            advance();
            return expr;
        }
        case CTokenKind::FloatLiteral: {
            CExprPtr expr(new CFloatLit(token.floatValue, token.spelling));
            expr->setOffset(offset);
            advance();
            return expr;
        }
        case CTokenKind::CharLiteral: {
            CExprPtr expr(new CCharLit(token.intValue, token.spelling));
            expr->setOffset(offset);
            advance();
            return expr;
        }
        case CTokenKind::StringLiteral: {
            // Adjacent string literals concatenate, C90 5.1.1.2 phase 6. The
            // spelling keeps each piece as written, joined with a space.
            std::string text = token.text;
            std::string spelling = token.spelling;
            advance();
            while (current().kind == CTokenKind::StringLiteral) {
                text += current().text;
                spelling += " ";
                spelling += current().spelling;
                advance();
            }
            CExprPtr expr(new CStringLit(std::move(text), std::move(spelling)));
            expr->setOffset(offset);
            return expr;
        }
        case CTokenKind::Identifier: {
            CExprPtr expr(new CIdent(token.text));
            expr->setOffset(offset);
            advance();
            return expr;
        }
        case CTokenKind::Punct:
            if (token.text == "(") {
                advance();
                CExprPtr inner = expression();
                if (inner == nullptr) return nullptr;
                if (!expect(")", "to close the expression")) return nullptr;
                return inner;
            }
            break;
        default:
            break;
    }

    fail("expected an expression, found '" +
         (token.kind == CTokenKind::End ? "end of file" : token.spelling) + "'");
    return nullptr;
}

// ---------------------------------------------------------------- top level

std::unique_ptr<CProgram> CParser::parse() {
    std::unique_ptr<CProgram> program(new CProgram());

    while (current().kind != CTokenKind::End && !failed_) {
        if (!atTypeStart()) {
            fail("expected a declaration at file scope, found '" +
                 current().spelling + "'");
            break;
        }
        Specifiers specifiers;
        bool sawAny = false;
        if (!declarationSpecifiers(&specifiers, &sawAny)) break;
        if (!sawAny) {
            fail("expected a declaration");
            break;
        }

        bool wasFunctionDef = false;
        std::unique_ptr<CFunctionDef> fn;
        std::unique_ptr<CDeclaration> decl =
            declaration(std::move(specifiers), &wasFunctionDef, &fn);

        if (wasFunctionDef) {
            if (fn == nullptr) break;    // the body failed to parse
            program->add(std::move(fn));
            continue;
        }
        if (decl == nullptr) break;
        program->add(std::move(decl));
    }

    if (failed_) return nullptr;
    return program;
}

}  // namespace c2s
