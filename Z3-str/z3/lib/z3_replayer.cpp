/*++
Copyright (c) 2011 Microsoft Corporation

Module Name:

    z3_replayer.cpp

Abstract:

    Interpreter for Z3 logs

Author:

    Leonardo de Moura (leonardo) 2011-09-22

Notes:
    
--*/
#include"vector.h"
#include"map.h"
#include"z3_replayer.h"
#include"stream_buffer.h"
#include"symbol.h"
#include"trace.h"

void register_z3_replayer_cmds(z3_replayer & in);

void throw_invalid_reference() {
    throw z3_replayer_exception("invalid argument reference");
}

struct z3_replayer::imp {
    z3_replayer &            m_owner;
    std::istream &           m_stream;
    char                     m_curr;  // current char;
    int                      m_line;  // line
    svector<char>            m_string;
    symbol                   m_id;
    __int64                  m_int64;
    __uint64                 m_uint64;
    double                   m_double;
    size_t                   m_ptr;
    size_t_map<void *>       m_heap;
    svector<z3_replayer_cmd> m_cmds;

    enum value_kind { INT64, UINT64, DOUBLE, STRING, SYMBOL, OBJECT, UINT_ARRAY, SYMBOL_ARRAY, OBJECT_ARRAY };

    struct value { 
        value_kind m_kind;
        union {
            __int64      m_int;
            __uint64     m_uint;
            double       m_double;
            char const * m_str;
            void *       m_obj;
        };
        value():m_kind(OBJECT), m_int(0) {}
        value(void * obj):m_kind(OBJECT), m_obj(obj) {}
        value(value_kind k, char const * str):m_kind(k), m_str(str) {}
        value(value_kind k, __uint64 u):m_kind(k), m_uint(u) {}
        value(value_kind k, __int64 i):m_kind(k), m_int(i) {}
        value(value_kind k, double d):m_kind(k), m_double(d) {}
    };

    svector<value>              m_args;
    void *                      m_result;
    vector<ptr_vector<void> >   m_obj_arrays;
    vector<svector<Z3_symbol> > m_sym_arrays;
    vector<unsigned_vector>     m_unsigned_arrays;

    imp(z3_replayer & o, std::istream & in):
        m_owner(o),
        m_stream(in),
        m_curr(0),
        m_line(1) {
        next();
    }

    void display_arg(std::ostream & out, value const & v) const {
        switch (v.m_kind) {
        case INT64:
            out << v.m_int;
            break;
        case UINT64:
            out << v.m_uint;
            break;
        case DOUBLE:
            out << v.m_double;
            break;
        case STRING:
            out << v.m_str;
            break;
        case SYMBOL:
            out << symbol::mk_symbol_from_c_ptr(v.m_str);
            break;
        case OBJECT:
            out << v.m_obj;
            break;
        case UINT_ARRAY:
        case OBJECT_ARRAY:
        case SYMBOL_ARRAY:
            out << "<array>";
            break;
        default:
            out << "<unknown>";
            break;
        }
    }

    void display_args(std::ostream & out) const {
        for (unsigned i = 0; i < m_args.size(); i++) {
            if (i > 0) out << " ";
            display_arg(out, m_args[i]);
        }
    }

    char curr() const { return m_curr; }
    void new_line() { m_line++; }
    void next() { m_curr = m_stream.get(); }
    
    void read_string_core(char delimiter) {
        if (curr() != delimiter)
            throw z3_replayer_exception("invalid string/symbol");
        m_string.reset();
        next();
        while (true) {
            char c = curr();
            if (c == EOF) {
                throw z3_replayer_exception("unexpected end of file");
            }
            else if (c == '\n') {
                throw z3_replayer_exception("unexpected end of line");
            }
            else if (c == '\\') {
                next();
                unsigned val = 0;
                unsigned sz  = 0;
                while (sz < 3) {
                    c = curr();
                    if ('0' <= c && c <= '9') {
                        val *= 10;
                        val += c - '0';
                        sz++;
                    }
                    else {
                        throw z3_replayer_exception("invalid scaped character");
                    }
                    if (val > 255)
                        throw z3_replayer_exception("invalid scaped character");
                    next();
                }
                TRACE("z3_replayer_escape", tout << "val: " << val << "\n";);
                m_string.push_back(static_cast<char>(val));
            }
            else if (c == delimiter) {
                next();
                m_string.push_back(0);
                return;
            }
            else {
                m_string.push_back(c);
                next();
            }
        }
    }

    void read_string() {
        read_string_core('"');
    }

    void read_quoted_symbol() {
        read_string_core('|');
        m_id = m_string.begin();
    }

    void read_int64() {
        if (!(curr() == '-' || ('0' <= curr() && curr() <= '9')))
            throw z3_replayer_exception("invalid integer");
        bool sign = false;
        if (curr() == '-') {
            sign = true;
            next();
            if (!('0' <= curr() && curr() <= '9'))
                throw z3_replayer_exception("invalid integer");
        }
        m_int64 = 0;
        while (true) {
            char c = curr();
            if ('0' <= c && c <= '9') {
                m_int64 = 10*m_int64 + (c - '0');
                next();
            }
            else {
                break;
            }
        }
        if (sign)
            m_int64 = -m_int64;
    }

    void read_uint64() {
        if (!('0' <= curr() && curr() <= '9'))
            throw z3_replayer_exception("invalid unsigned");
        m_uint64 = 0;
        while (true) {
            char c = curr();
            if ('0' <= c && c <= '9') {
                m_uint64 = 10*m_uint64 + (c - '0');
                next();
            }
            else {
                break;
            }
        }
    }

    bool is_double_char() const {
        return curr() == '-' || curr() == '.' || ('0' <= curr() && curr() <= '9') || curr() == 'e' || curr() == 'E'; 
    }

    void read_double() {
        m_string.reset();
        while (is_double_char()) {
            m_string.push_back(curr());
            next();
        }
        if (m_string.empty())
            throw z3_replayer_exception("invalid double");
        m_string.push_back(0);
        char * ptr;
        m_double = strtod(m_string.begin(), &ptr);
    }

    void read_ptr() {
        if (!(('0' <= curr() && curr() <= '9') || ('A' <= curr() && curr() <= 'F') || ('a' <= curr() && curr() <= 'f')))
            throw z3_replayer_exception("invalid ptr");
        m_ptr = 0;
        while (true) {
            char c = curr();
            if ('0' <= c && c <= '9') {
                m_ptr = m_ptr * 16 + (c - '0');
            }
            else if ('a' <= c && c <= 'f') {
                m_ptr = m_ptr * 16 + 10 + (c - 'a');
            }
            else if ('A' <= c && c <= 'F') {
                m_ptr = m_ptr * 16 + 10 + (c - 'A');
            }
            else {
                return;
            }
            next();
        }
    }

    void skip_blank() {
        while (true) {
            char c = curr();
            if (c == '\n') {
                new_line();
                next();
            }
            else if (c == ' ' || c == '\t') {
                next();
            }
            else {
                break;
            }
        }
    }

    void push_array(unsigned sz, value_kind k) {
        unsigned   asz = m_args.size();
        if (sz > asz)
            throw z3_replayer_exception("invalid array size");
        __uint64   aidx;
        value_kind nk;
        for (unsigned i = asz - sz; i < asz; i++) {
            if (m_args[i].m_kind != k)
                throw z3_replayer_exception("invalid array: mixed value types");
        }
        if (k == UINT64) {
            aidx = m_unsigned_arrays.size();
            nk   = UINT_ARRAY;
            m_unsigned_arrays.push_back(unsigned_vector());
            unsigned_vector & v = m_unsigned_arrays.back();
            for (unsigned i = asz - sz; i < asz; i++) {
                v.push_back(static_cast<unsigned>(m_args[i].m_uint));
            }
        }
        else if (k == SYMBOL) {
            aidx = m_sym_arrays.size();
            nk   = SYMBOL_ARRAY;
            m_sym_arrays.push_back(svector<Z3_symbol>());
            svector<Z3_symbol> & v = m_sym_arrays.back();
            for (unsigned i = asz - sz; i < asz; i++) {
                v.push_back(reinterpret_cast<Z3_symbol>(const_cast<char*>(m_args[i].m_str)));
            }
        }
        else if (k == OBJECT) {
            TRACE("z3_replayer_bug", 
                  tout << "args: "; display_args(tout); tout << "\n";
                  tout << "push_back, sz: " << sz << ", m_obj_arrays.size(): " << m_obj_arrays.size() << "\n";
                  for (unsigned i = asz - sz; i < asz; i++) {
                      tout << "pushing: " << m_args[i].m_obj << "\n";
                  });
            aidx = m_obj_arrays.size();
            nk   = OBJECT_ARRAY;
            m_obj_arrays.push_back(ptr_vector<void>());
            ptr_vector<void> & v = m_obj_arrays.back();
            for (unsigned i = asz - sz; i < asz; i++) {
                v.push_back(m_args[i].m_obj);
            }
        }
        else {
            throw z3_replayer_exception("unsupported array type");
        }
        m_args.shrink(asz - sz);
        m_args.push_back(value(nk, aidx));
    }

#define TICK_FREQUENCY 100000

    void parse() {
        unsigned long long counter = 0;
        unsigned tick = 0;
        while (true) {
            IF_VERBOSE(1, {
                counter++; tick++;
                if (tick >= TICK_FREQUENCY) {
                    std::cout << "[replayer] " << counter << " operations executed" << std::endl;
                    tick = 0;
                }
            });
            skip_blank();
            char c = curr();
            if (c == EOF)
                return;
            switch (c) {
            case 'R':
                // reset
                next(); 
                TRACE("z3_replayer", tout << "[" << m_line << "] " << "R\n";);
                reset();
                break;
            case 'P': {
                // push pointer
                next(); skip_blank(); read_ptr();
                TRACE("z3_replayer", tout << "[" << m_line << "] " << "P " << m_ptr << "\n";);
                if (m_ptr == 0) {
                    m_args.push_back(0);
                }
                else { 
                    void * obj = 0;
                    if (!m_heap.find(m_ptr, obj))
                        throw z3_replayer_exception("invalid pointer");
                    m_args.push_back(value(obj));
                    TRACE("z3_replayer_bug", tout << "args after 'P':\n"; display_args(tout); tout << "\n";);
                }
                break;
            }
            case 'S': {
                // push string
                next(); skip_blank(); read_string();
                TRACE("z3_replayer", tout << "[" << m_line << "] "  << "S " << m_string.begin() << "\n";);
                symbol sym(m_string.begin()); // save string
                m_args.push_back(value(STRING, sym.bare_str()));
                break;
            }
            case 'N':
                // push null symbol
                next();
                TRACE("z3_replayer", tout << "[" << m_line << "] " << "N\n";);
                m_args.push_back(value(SYMBOL, static_cast<char const *>(0)));
                break;
            case '$': {
                // push symbol
                next(); skip_blank(); read_quoted_symbol();
                TRACE("z3_replayer", tout << "[" << m_line << "] " << "$ " << m_id << "\n";);
                m_args.push_back(value(SYMBOL, m_id.bare_str()));
                break;
            }
            case '#': {
                // push numeral symbol
                next(); skip_blank(); read_uint64();
                TRACE("z3_replayer", tout << "[" << m_line << "] " << "# " << m_uint64 << "\n";);
                symbol sym(static_cast<unsigned>(m_uint64));
                m_args.push_back(value(SYMBOL, static_cast<char const *>(sym.c_ptr())));
                break;
            }
            case 'I':
                // push integer;
                next(); skip_blank(); read_int64();
                TRACE("z3_replayer", tout << "[" << m_line << "] " << "I " << m_int64 << "\n";);
                m_args.push_back(value(INT64, m_int64));
                break;
            case 'U':
                // push unsigned;
                next(); skip_blank(); read_uint64();
                TRACE("z3_replayer", tout << "[" << m_line << "] " << "U " << m_uint64 << "\n";);
                m_args.push_back(value(UINT64, m_uint64));
                break;
            case 'D':
                // push double
                next(); skip_blank(); read_double();
                TRACE("z3_replayer", tout << "[" << m_line << "] " << "D " << m_double << "\n";);
                m_args.push_back(value(DOUBLE, m_double));
                break;
            case 'p':
            case 's':
            case 'u':
                // push array
                next(); skip_blank(); read_uint64();
                TRACE("z3_replayer", tout << "[" << m_line << "] " << "A " << m_uint64 << "\n";);
                if (c == 'p')
                    push_array(static_cast<unsigned>(m_uint64), OBJECT);
                else if (c == 's')
                    push_array(static_cast<unsigned>(m_uint64), SYMBOL);
                else
                    push_array(static_cast<unsigned>(m_uint64), UINT64);
                break;
            case 'C': {
                // call procedure
                next(); skip_blank(); read_uint64();
                TRACE("z3_replayer", tout << "[" << m_line << "] " << "C " << m_uint64 << "\n";);
                unsigned idx = static_cast<unsigned>(m_uint64);
                if (idx >= m_cmds.size())
                    throw z3_replayer_exception("invalid command");
                try {
                    m_cmds[idx](m_owner);
                }
                catch (z3_error & ex) {
                    throw ex;
                }
                catch (z3_exception & ex) {
                    std::cout << "[z3 exception]: " << ex.msg() << std::endl;
                }
                break;
            }
            case '=':
                // save result
                // = obj_id
                next(); skip_blank(); read_ptr();
                TRACE("z3_replayer", tout << "[" << m_line << "] " << "= " << m_ptr << "\n";);
                m_heap.insert(m_ptr, m_result);
                break;
            case '*': {
                // save out
                // @ obj_id pos
                next(); skip_blank(); read_ptr(); skip_blank(); read_uint64();
                unsigned pos = static_cast<unsigned>(m_uint64);
                TRACE("z3_replayer", tout << "[" << m_line << "] " << "* " << m_ptr << " " << pos << "\n";);
                if (pos >= m_args.size() || m_args[pos].m_kind != OBJECT)
                    throw_invalid_reference();
                m_heap.insert(m_ptr, m_args[pos].m_obj);
                break;
            }
            case '@': {
                // save array out
                // @ obj_id array_pos idx
                next(); skip_blank(); read_ptr(); skip_blank(); read_uint64();
                unsigned pos = static_cast<unsigned>(m_uint64);
                if (pos >= m_args.size() || m_args[pos].m_kind != OBJECT_ARRAY)
                    throw_invalid_reference();
                unsigned aidx = static_cast<unsigned>(m_args[pos].m_uint);
                ptr_vector<void> & v = m_obj_arrays[aidx];
                skip_blank(); read_uint64();
                unsigned idx = static_cast<unsigned>(m_uint64);
                TRACE("z3_replayer", tout << "[" << m_line << "] " << "@ " << m_ptr << " " << pos << " " << idx << "\n";);
                TRACE("z3_replayer_bug", tout << "v[idx]: " << v[idx] << "\n";);
                m_heap.insert(m_ptr, v[idx]);
                break;
            }
            case 'M':
                // user message
                next(); skip_blank(); read_string();
                TRACE("z3_replayer", tout << "[" << m_line << "] " << "M " << m_string.begin() << "\n";);
                std::cout << m_string.begin() << "\n"; std::cout.flush();
                break;
            default:
                TRACE("z3_replayer", tout << "unknown command " << c << "\n";);
                throw z3_replayer_exception("unknown log command");
                break;
            }
        }
    }

    int get_int(unsigned pos) const {
        if (pos >= m_args.size() || m_args[pos].m_kind != INT64)
            throw_invalid_reference();
        return static_cast<int>(m_args[pos].m_int);
    }

    __int64 get_int64(unsigned pos) const {
        if (pos >= m_args.size() || m_args[pos].m_kind != INT64)
            throw_invalid_reference();
        return m_args[pos].m_int;
    }

    unsigned get_uint(unsigned pos) const {
        if (pos >= m_args.size() || m_args[pos].m_kind != UINT64)
            throw_invalid_reference();
        return static_cast<unsigned>(m_args[pos].m_uint);
    }

    __uint64 get_uint64(unsigned pos) const {
        if (pos >= m_args.size() || m_args[pos].m_kind != UINT64)
            throw_invalid_reference();
        return m_args[pos].m_uint;
    }

    double get_double(unsigned pos) const {
        if (pos >= m_args.size() || m_args[pos].m_kind != DOUBLE)
            throw_invalid_reference();
        return m_args[pos].m_double;
    }

    Z3_string get_str(unsigned pos) const {
        if (pos >= m_args.size() || m_args[pos].m_kind != STRING)
            throw_invalid_reference();
        return m_args[pos].m_str;
    }

    Z3_symbol get_symbol(unsigned pos) const {
        if (pos >= m_args.size() || m_args[pos].m_kind != SYMBOL)
            throw_invalid_reference();
        return reinterpret_cast<Z3_symbol>(const_cast<char*>(m_args[pos].m_str));
    }

    void * get_obj(unsigned pos) const {
        if (pos >= m_args.size() || m_args[pos].m_kind != OBJECT)
            throw_invalid_reference();
        return m_args[pos].m_obj;
    }

    unsigned * get_uint_array(unsigned pos) const {
        if (pos >= m_args.size() || m_args[pos].m_kind != UINT_ARRAY)
            throw_invalid_reference();
        unsigned idx = static_cast<unsigned>(m_args[pos].m_uint);
        return m_unsigned_arrays[idx].c_ptr();
    }

    Z3_symbol * get_symbol_array(unsigned pos) const {
        if (pos >= m_args.size() || m_args[pos].m_kind != SYMBOL_ARRAY)
            throw_invalid_reference();
        unsigned idx = static_cast<unsigned>(m_args[pos].m_uint);
        return m_sym_arrays[idx].c_ptr();
    }

    void ** get_obj_array(unsigned pos) const {
        if (pos >= m_args.size() || m_args[pos].m_kind != OBJECT_ARRAY)
            throw_invalid_reference();
        unsigned idx = static_cast<unsigned>(m_args[pos].m_uint);
        ptr_vector<void> const & v = m_obj_arrays[idx];
        TRACE("z3_replayer_bug", tout << "pos: " << pos << ", idx: " << idx << " size(): " << v.size() << "\n";
              for (unsigned i = 0; i < v.size(); i++) tout << v[i] << " "; tout << "\n";);
        return v.c_ptr();
    }

    int * get_int_addr(unsigned pos) {
        if (pos >= m_args.size() || m_args[pos].m_kind != INT64)
            throw_invalid_reference();
        return reinterpret_cast<int*>(&(m_args[pos].m_int));
    }

    __int64 * get_int64_addr(unsigned pos) {
        if (pos >= m_args.size() || m_args[pos].m_kind != INT64)
            throw_invalid_reference();
        return &(m_args[pos].m_int);
    }

    unsigned * get_uint_addr(unsigned pos) {
        if (pos >= m_args.size() || m_args[pos].m_kind != UINT64)
            throw_invalid_reference();
        return reinterpret_cast<unsigned*>(&(m_args[pos].m_uint));
    }

    __uint64 * get_uint64_addr(unsigned pos) {
        if (pos >= m_args.size() || m_args[pos].m_kind != UINT64)
            throw_invalid_reference();
        return &(m_args[pos].m_uint);
    }

    Z3_string * get_str_addr(unsigned pos) {
        if (pos >= m_args.size() || m_args[pos].m_kind != STRING)
            throw_invalid_reference();
        return &(m_args[pos].m_str);
    }

    void ** get_obj_addr(unsigned pos) {
        if (pos >= m_args.size() || m_args[pos].m_kind != OBJECT)
            throw_invalid_reference();
        return &(m_args[pos].m_obj);
    }

    void store_result(void * obj) {
        m_result = obj;
    }

    void register_cmd(unsigned id, z3_replayer_cmd cmd) {
        m_cmds.reserve(id+1, 0);
        m_cmds[id] = cmd;
    }

    void reset() {
        m_result = 0;
        m_args.reset();
        m_obj_arrays.reset();
        m_sym_arrays.reset();
        m_unsigned_arrays.reset();
    }
    
  
};

z3_replayer::z3_replayer(std::istream & in) {
    m_imp = alloc(imp, *this, in);
    register_z3_replayer_cmds(*this);
}
    
z3_replayer::~z3_replayer() {
    dealloc(m_imp);
}

unsigned z3_replayer::get_line() const {
    return m_imp->m_line;
}

bool z3_replayer::get_bool(unsigned pos) const {
    return get_int(pos) != 0;
}

int z3_replayer::get_int(unsigned pos) const {
    return m_imp->get_int(pos);
}

unsigned z3_replayer::get_uint(unsigned pos) const {
    return m_imp->get_uint(pos);
}

__int64 z3_replayer::get_int64(unsigned pos) const {
    return m_imp->get_int64(pos);
}

__uint64 z3_replayer::get_uint64(unsigned pos) const {
    return m_imp->get_uint64(pos);
}

double z3_replayer::get_double(unsigned pos) const {
    return m_imp->get_double(pos);
}

Z3_string z3_replayer::get_str(unsigned pos) const {
    return m_imp->get_str(pos);
}

Z3_symbol z3_replayer::get_symbol(unsigned pos) const {
    return m_imp->get_symbol(pos);
}

void * z3_replayer::get_obj(unsigned pos) const {
    return m_imp->get_obj(pos);
}

unsigned * z3_replayer::get_uint_array(unsigned pos) const {
    return m_imp->get_uint_array(pos);
}

Z3_symbol * z3_replayer::get_symbol_array(unsigned pos) const {
    return m_imp->get_symbol_array(pos);
}

void ** z3_replayer::get_obj_array(unsigned pos) const {
    return m_imp->get_obj_array(pos);
}

int * z3_replayer::get_int_addr(unsigned pos) {
    return m_imp->get_int_addr(pos);
}

__int64 * z3_replayer::get_int64_addr(unsigned pos) {
    return m_imp->get_int64_addr(pos);
}

unsigned * z3_replayer::get_uint_addr(unsigned pos) {
    return m_imp->get_uint_addr(pos);
}

__uint64 * z3_replayer::get_uint64_addr(unsigned pos) {
    return m_imp->get_uint64_addr(pos);
}

Z3_string * z3_replayer::get_str_addr(unsigned pos) {
    return m_imp->get_str_addr(pos);
}

void ** z3_replayer::get_obj_addr(unsigned pos) {
    return m_imp->get_obj_addr(pos);
}

void z3_replayer::store_result(void * obj) {
    return m_imp->store_result(obj);
}

void z3_replayer::register_cmd(unsigned id, z3_replayer_cmd cmd) {
    return m_imp->register_cmd(id, cmd);
}

void z3_replayer::parse() {
    return m_imp->parse();
}
