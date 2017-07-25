/*++
Copyright (c) 2006 Microsoft Corporation

Module Name:

    smt2scanner.cpp

Abstract:

    <abstract>

Author:

    Leonardo de Moura (leonardo) 2011-03-09.

Revision History:

--*/
#include"smt2scanner.h"

namespace smt2 {

    void scanner::next() {
        if (m_cache_input)
            m_cache.push_back(m_curr);
        SASSERT(m_curr != EOF);
        if (m_interactive) {
            m_curr = m_stream.get();
        }
        else if (m_bpos < m_bend) {
            m_curr = m_buffer[m_bpos];
            m_bpos++;
        }
        else {
            m_stream.read(m_buffer, SCANNER_BUFFER_SIZE);
            m_bend = static_cast<unsigned>(m_stream.gcount());
            m_bpos = 0;
            if (m_bpos == m_bend) {
                m_curr = EOF;
            }
            else {
                m_curr = m_buffer[m_bpos];
                m_bpos++;
            }
        }
        m_spos++;
    }
    
    void scanner::read_comment() {
        SASSERT(curr() == ';');
        next();
        while (true) {
            char c = curr();
            if (c == EOF)
                return;
            if (c == '\n') {
                new_line();
                next();
                return;
            }
            next();
        }
    }
    
    scanner::token scanner::read_quoted_symbol() {
        SASSERT(curr() == '|');
        bool escape = false;
        m_string.reset();
        next();
        while (true) {
            char c = curr();
            if (c == EOF) {
                throw scanner_exception("unexpected end of quoted symbol", m_line, m_spos);
            }
            else if (c == '\n') {
                new_line();
            }
            else if (c == '|' && !escape) {
                next();
                m_string.push_back(0);
                m_id = m_string.begin();
                TRACE("scanner", tout << "new quoted symbol: " << m_id << "\n";);
                return SYMBOL_TOKEN;
            }
            escape = (c == '\\');
            m_string.push_back(c);
            next();
        }
    }
    
    scanner::token scanner::read_symbol() {
        SASSERT(m_normalized[static_cast<unsigned>(curr())] == 'a' || curr() == ':');
        m_string.reset();
        m_string.push_back(curr());
        next();
        
        while (true) {
            char c = curr();
            char n = m_normalized[static_cast<unsigned char>(c)];
            if (n == 'a' || n == '0') {
                m_string.push_back(c);
                next();
            }
            else {
                m_string.push_back(0);
                m_id = m_string.begin();
                TRACE("scanner", tout << "new symbol: " << m_id << "\n";);
                return SYMBOL_TOKEN;
            }
        }
    }
    
    scanner::token scanner::read_number() {
        SASSERT('0' <= curr() && curr() <= '9');
        rational q(1);
        m_number = rational(curr() - '0');
        next();
        bool is_float = false;
        
        while (true) {
            char c = curr();
            if ('0' <= c && c <= '9') {
                m_number = rational(10)*m_number + rational(c - '0');
                if (is_float)
                    q *= rational(10);
                next();
            }
            else if (c == '.') {
                if (is_float)
                    break;
                is_float = true;
                next();
            }
            else {
                break;
            }
        }
        if (is_float) 
            m_number /= q;
        TRACE("scanner", tout << "new number: " << m_number << "\n";);
        return is_float ? FLOAT_TOKEN : INT_TOKEN;
    }
    
    scanner::token scanner::read_string() {
        SASSERT(curr() == '\"');
        next();
        m_string.reset();
        while (true) {
            char c = curr();
            if (c == EOF) 
                throw scanner_exception("unexpected end of string", m_line, m_spos);
            if (c == '\"') {
                next();
                m_string.push_back(0);
                return STRING_TOKEN;
            }
            if (c == '\n')
                new_line();
            else if (c == '\\') {
                next();
                c = curr();
                if (c == EOF) 
                    throw scanner_exception("unexpected end of string", m_line, m_spos);
                if (c != '\\' && c != '\"')
                    throw scanner_exception("invalid escape sequence", m_line, m_spos);
            }
            m_string.push_back(c);
            next();
        }
    }
    
    scanner::token scanner::read_bv_literal() {
        SASSERT(curr() == '#');
        next();
        char c = curr();
        if (c == 'x') {
            next();
            c = curr();
            m_number  = rational(0);
            m_bv_size = 0;
            while (true) {
                if ('0' <= c && c <= '9') {
                    m_number *= rational(16);
                    m_number += rational(c - '0');
                }
                else if ('a' <= c && c <= 'f') {
                    m_number *= rational(16);
                    m_number += rational(10 + (c - 'a')); 
                }
                else if ('A' <= c && c <= 'F') {
                    m_number *= rational(16);
                    m_number += rational(10 + (c - 'A'));
                }
                else {
                    if (m_bv_size == 0)
                        throw scanner_exception("invalid empty bit-vector literal", m_line, m_spos);
                    return BV_TOKEN;
                }
                m_bv_size += 4;
                next();
                c = curr();
            }
        }
        else if (c == 'b') {
            next();
            c = curr();
            m_number  = rational(0);
            m_bv_size = 0;
            while (c == '0' || c == '1') {
                m_number *= rational(2);
                m_number += rational(c - '0');
                m_bv_size++;
                next();
                c = curr();
            }
            if (m_bv_size == 0)
                throw scanner_exception("invalid empty bit-vector literal", m_line, m_spos);
            return BV_TOKEN;
        }
        else {
            throw scanner_exception("invalid bit-vector literal, expecting 'x' or 'b'", m_line, m_spos);
        }
    }
    
    scanner::scanner(std::istream& stream, bool interactive):
        m_interactive(interactive), 
        m_spos(0),
        m_curr(0), // avoid Valgrind warning
        m_line(1),
        m_pos(0),
        m_bv_size(UINT_MAX),
        m_bpos(0),
        m_bend(0),
        m_stream(stream),
        m_cache_input(false) {
        for (int i = 0; i < 256; ++i) {
            m_normalized[i] = (char) i;
        }
        m_normalized[static_cast<int>('\t')] = ' ';
        m_normalized[static_cast<int>('\r')] = ' ';
        // assert ('a' < 'z');
        for (char ch = 'b'; ch <= 'z'; ++ch) {
            m_normalized[static_cast<int>(ch)] = 'a';
        }
        for (char ch = 'A'; ch <= 'Z'; ++ch) {
            m_normalized[static_cast<int>(ch)] = 'a';
        }
        // assert ('0' < '9', '9' - '0' == 9);
        for (char ch = '1'; ch <= '9'; ++ch) {
            m_normalized[static_cast<int>(ch)] = '0';
        }
        // SMT2 "Symbols": ~ ! @ $ % ^ & * _ - + = < > . ? /
        m_normalized[static_cast<int>('~')] = 'a';
        m_normalized[static_cast<int>('!')] = 'a';
        m_normalized[static_cast<int>('@')] = 'a';
        m_normalized[static_cast<int>('$')] = 'a';
        m_normalized[static_cast<int>('%')] = 'a';
        m_normalized[static_cast<int>('^')] = 'a';
        m_normalized[static_cast<int>('&')] = 'a';
        m_normalized[static_cast<int>('*')] = 'a';
        m_normalized[static_cast<int>('_')] = 'a';
        m_normalized[static_cast<int>('-')] = 'a';
        m_normalized[static_cast<int>('+')] = 'a';
        m_normalized[static_cast<int>('=')] = 'a';
        m_normalized[static_cast<int>('<')] = 'a';
        m_normalized[static_cast<int>('>')] = 'a';
        m_normalized[static_cast<int>('.')] = 'a';
        m_normalized[static_cast<int>('?')] = 'a';
        m_normalized[static_cast<int>('/')] = 'a';
        next();
    }
    
    scanner::token scanner::scan() {
        while (true) {
            char c = curr();
            m_pos = m_spos;
            switch (m_normalized[(unsigned char) c]) {
            case ' ':
                next();
                break;
            case '\n':
                next();
                new_line();
                break;
            case ';':
                read_comment();
                break;
            case ':':
                read_symbol();
                return KEYWORD_TOKEN;
            case '(':
                next();
                return LEFT_PAREN;
            case ')':
                next();
                return RIGHT_PAREN;
            case '|':
                return read_quoted_symbol();
            case 'a':
                return read_symbol();
            case '"':
                return read_string();
            case '0':
                return read_number();
            case '#':
                return read_bv_literal();
            case -1:
                return EOF_TOKEN;
            default: {
                scanner_exception ex("unexpected character", m_line, m_spos);
                next();
                throw ex;
            }}
        }
    }

    char const * scanner::cached_str(unsigned begin, unsigned end) {
        m_cache_result.reset();
        while (isspace(m_cache[begin]) && begin < end)
            begin++;
        while (begin < end && isspace(m_cache[end-1]))
            end--;
        for (unsigned i = begin; i < end; i++)
            m_cache_result.push_back(m_cache[i]);
        m_cache_result.push_back(0);
        return m_cache_result.begin();
    }

};

