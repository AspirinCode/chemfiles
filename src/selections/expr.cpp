/* Chemfiles, an efficient IO library for chemistry file formats
 * Copyright (C) 2015 Guillaume Fraux
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/
 */
#include <cmath>
#include "chemfiles/selections/parser.hpp"
#include "chemfiles/selections/expr.hpp"

using namespace chemfiles;
using namespace chemfiles::selections;

std::ostream& operator<<(std::ostream& out, const std::unique_ptr<Expr>& expr) {
    out << expr->print();
    return out;
}

//! Get the associated string to a binary operator as opstr<Op>::str;
static std::string binop_str(BinOp op) {
    switch (op) {
    case BinOp::EQ:
        return "==";
    case BinOp::NEQ:
        return "!=";
    case BinOp::LT:
        return "<";
    case BinOp::LE:
        return "<=";
    case BinOp::GT:
        return ">";
    case BinOp::GE:
        return ">=";
    default:
        throw std::runtime_error("Hit the default case in binop_str");
    }
}

/****************************************************************************************/
std::string NameExpr::print(unsigned) const {
    if (equals_) {
        return "name == " + name_;
    } else {
        return "name != " + name_;
    }
}

template<> Ast selections::parse<NameExpr>(token_iterator_t& begin, const token_iterator_t& end) {
    assert(end - begin >= 3);
    assert(begin[2].type() == Token::IDENT);
    assert(begin[2].ident() == "name");
    if (begin[1].type() != Token::IDENT || !(begin->type() == Token::EQ || begin->type() == Token::NEQ)) {
        throw ParserError("Name selection must follow the pattern: 'name == {name} | name != {name}'");
    }
    auto equals = (begin->type() == Token::EQ);
    auto name = begin[1].ident();
    begin += 3;
    return Ast(new NameExpr(name, equals));
}

/****************************************************************************************/
std::string PositionExpr::print(unsigned) const {
    std::string coord;
    if (coord_ == X) {
        coord += "x ";
    } else if (coord_ == Y) {
        coord += "y ";
    } else if (coord_ == Z) {
        coord += "z ";
    };
    return coord + binop_str(op_) + " " + std::to_string(val_);
}

template<> Ast selections::parse<PositionExpr>(token_iterator_t& begin, const token_iterator_t& end) {
    assert(end - begin >= 3);
    assert(begin[2].type() == Token::IDENT);
    assert(begin[2].ident() == "x" || begin[2].ident() == "y" || begin[2].ident() == "z");
    assert(begin->is_binary_op());

    decltype(PositionExpr::X) coord;
    auto coord_name = begin[2].ident();
    if (coord_name == "x") {
        coord = PositionExpr::X;
    } else if (coord_name == "y") {
        coord = PositionExpr::Y;
    } else if (coord_name == "z") {
        coord = PositionExpr::Z;
    } else {
        throw std::runtime_error("Hit the default case in coordinate identification");
    }

    auto op = BinOp(begin->type());
    if (begin[1].type() != Token::NUM) {
        throw ParserError("Position selection can only contain number as criterium.");
    }
    auto val = begin[1].number();
    begin += 3;
    return Ast(new PositionExpr(coord, op, val));
}

/****************************************************************************************/
std::string VelocityExpr::print(unsigned) const {
    std::string coord;
    if (coord_ == X) {
        coord = "vx ";
    } else if (coord_ == Y) {
        coord = "vy ";
    } else if (coord_ == Z) {
        coord = "vz ";
    };
    return coord + binop_str(op_) + " " + std::to_string(val_);
}

template<> Ast selections::parse<VelocityExpr>(token_iterator_t& begin, const token_iterator_t& end) {
    assert(end - begin >= 3);
    assert(begin[2].type() == Token::IDENT);
    assert(begin[2].ident() == "vx" || begin[2].ident() == "vy" || begin[2].ident() == "vz");
    assert(begin->is_binary_op());

    decltype(VelocityExpr::X) coord;
    auto coord_name = begin[2].ident();
    if (coord_name == "vx") {
        coord = VelocityExpr::X;
    } else if (coord_name == "vy") {
        coord = VelocityExpr::Y;
    } else if (coord_name == "vz") {
        coord = VelocityExpr::Z;
    } else {
        throw std::runtime_error("Hit the default case in coordinate identification");
    }

    auto op = BinOp(begin->type());
    if (begin[1].type() != Token::NUM) {
        throw ParserError("Veclocity selection can only contain number as criterium.");
    }
    auto val = begin[1].number();
    begin += 3;
    return Ast(new VelocityExpr(coord, op, val));;
}

/****************************************************************************************/
std::string IndexExpr::print(unsigned) const {
    return "index " + binop_str(op_) + " " + std::to_string(val_);
}

template<> Ast selections::parse<IndexExpr>(token_iterator_t& begin, const token_iterator_t& end) {
    assert(end - begin >= 3);
    assert(begin[2].type() == Token::IDENT);
    assert(begin[2].ident() == "index");
    assert(begin->is_binary_op());

    auto op = BinOp(begin->type());
    if (begin[1].type() == Token::NUM) {
        auto num = begin[1].number();
        if (ceil(num) != num) {
            throw ParserError("Index selection should contain an integer");
        }
    } else {
        throw ParserError("Index selection should contain an integer");
    }
    auto val = static_cast<std::size_t>(begin[1].number());
    begin += 3;
    return Ast(new IndexExpr(op, val));;
}

/****************************************************************************************/
std::string AndExpr::print(unsigned delta) const {
    auto lhs = lhs_->print(7);
    auto rhs = rhs_->print(7);
    return "and -> " + lhs + "\n" + std::string(delta, ' ') + "    -> " + rhs;
}

template<> Ast selections::parse<AndExpr>(token_iterator_t& begin, const token_iterator_t& end) {
    assert(begin->type() == Token::AND);
    begin += 1;
    if (begin == end) throw ParserError("Missing right-hand side operand to 'and'");

    Ast rhs = nullptr;
    try {
        rhs = dispatch_parsing(begin, end);
    } catch (const ParserError& e) {
        throw ParserError(std::string("Error in right-hand side operand to 'and': ") + e.what());
    }

    if (begin == end) throw ParserError("Missing left-hand side operand to 'and'");

    Ast lhs = nullptr;
    try {
        lhs = dispatch_parsing(begin, end);
    } catch (const ParserError& e) {
        throw ParserError(std::string("Error in left-hand side operand to 'and': ") + e.what());
    }
    return Ast(new AndExpr(std::move(lhs), std::move(rhs)));
}

/****************************************************************************************/
std::string OrExpr::print(unsigned delta) const {
    auto lhs = lhs_->print(6);
    auto rhs = rhs_->print(6);
    return "or -> " + lhs + "\n" + std::string(delta, ' ') + "   -> " + rhs;
}

template<> Ast selections::parse<OrExpr>(token_iterator_t& begin, const token_iterator_t& end) {
    assert(begin->type() == Token::OR);
    begin += 1;
    if (begin == end) throw ParserError("Missing right-hand side operand to 'or'");

    Ast rhs = nullptr;
    try {
        rhs = dispatch_parsing(begin, end);
    } catch (const ParserError& e) {
        throw ParserError(std::string("Error in right-hand side operand to 'or': ") + e.what());
    }

    if (begin == end) throw ParserError("Missing left-hand side operand to 'or'");

    Ast lhs = nullptr;
    try {
        lhs = dispatch_parsing(begin, end);
    } catch (const ParserError& e) {
        throw ParserError(std::string("Error in left-hand side operand to 'or': ") + e.what());
    }
    return Ast(new OrExpr(std::move(lhs), std::move(rhs)));
}

/****************************************************************************************/
std::string NotExpr::print(unsigned) const {
    auto ast = ast_->print(4);
    return "not " + ast;
}

template<> Ast selections::parse<NotExpr>(token_iterator_t& begin, const token_iterator_t& end) {
    assert(begin->type() == Token::NOT);
    begin += 1;
    if (begin == end) throw ParserError("Missing operand to 'not'");

    Ast ast = nullptr;
    try {
        ast = dispatch_parsing(begin, end);
    } catch (const ParserError& e) {
        throw ParserError(std::string("Error in operand of 'not': ") + e.what());
    }

    return Ast(new NotExpr(std::move(ast)));
}