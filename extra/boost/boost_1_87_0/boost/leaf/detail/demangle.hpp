#ifndef BOOST_LEAF_DETAIL_DEMANGLE_HPP_INCLUDED
#define BOOST_LEAF_DETAIL_DEMANGLE_HPP_INCLUDED

// Copyright 2018-2024 Emil Dotchevski and Reverge Studios, Inc.
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// This file is based on boost::core::demangle
//
// Copyright 2014 Peter Dimov
// Copyright 2014 Andrey Semashev
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt

#include <boost/leaf/config.hpp>
#include <iosfwd>
#include <cstdlib>

#if BOOST_LEAF_CFG_DIAGNOSTICS

// __has_include is currently supported by GCC and Clang. However GCC 4.9 may have issues and
// returns 1 for 'defined( __has_include )', while '__has_include' is actually not supported:
// https://gcc.gnu.org/bugzilla/show_bug.cgi?id=63662
#if defined(__has_include) && (!defined(__GNUC__) || defined(__clang__) || (__GNUC__ + 0) >= 5)
#   if __has_include(<cxxabi.h>)
#       define BOOST_LEAF_HAS_CXXABI_H
#   endif
#elif defined(__GLIBCXX__) || defined(__GLIBCPP__)
#   define BOOST_LEAF_HAS_CXXABI_H
#endif

#if defined(BOOST_LEAF_HAS_CXXABI_H)
#   include <cxxabi.h>
//  For some archtectures (mips, mips64, x86, x86_64) cxxabi.h in Android NDK is implemented by gabi++ library
//  (https://android.googlesource.com/platform/ndk/+/master/sources/cxx-stl/gabi++/), which does not implement
//  abi::__cxa_demangle(). We detect this implementation by checking the include guard here.
#   if defined(__GABIXX_CXXABI_H__)
#       undef BOOST_LEAF_HAS_CXXABI_H
#   endif
#endif

#endif

namespace boost { namespace leaf {

namespace detail
{
    // The functions below are C++11 constexpr, but we use BOOST_LEAF_ALWAYS_INLINE to control object file
    // section count / template bleat. Evidently this makes a difference on gcc / windows at least.

    template <int S1, int S2, int I, bool = S1 >= S2>
    struct cpp11_prefix
    {
        BOOST_LEAF_ALWAYS_INLINE constexpr static bool check(char const (&)[S1], char const (&)[S2]) noexcept
        {
            return false;
        }
    };
    template <int S1, int S2, int I>
    struct cpp11_prefix<S1, S2, I, true>
    {
        BOOST_LEAF_ALWAYS_INLINE constexpr static bool check(char const (&str)[S1], char const (&prefix)[S2]) noexcept
        {
            return str[I] == prefix[I] && cpp11_prefix<S1, S2, I - 1>::check(str, prefix);
        }
    };
    template <int S1, int S2>
    struct cpp11_prefix<S1, S2, 0, true>
    {
        BOOST_LEAF_ALWAYS_INLINE constexpr static bool check(char const (&str)[S1], char const (&prefix)[S2]) noexcept
        {
            return str[0] == prefix[0];
        }
    };
    template <int S1, int S2>
    BOOST_LEAF_ALWAYS_INLINE constexpr int check_prefix(char const (&str)[S1], char const (&prefix)[S2]) noexcept
    {
        return cpp11_prefix<S1, S2, S2 - 2>::check(str, prefix) ? S2 - 1 : 0;
    }

    ////////////////////////////////////////

    template <int S1, int S2, int I1, int I2, bool = S1 >= S2>
    struct cpp11_suffix
    {
        BOOST_LEAF_ALWAYS_INLINE constexpr static bool check(char const (&)[S1], char const (&)[S2]) noexcept
        {
            return false;
        }
    };
    template <int S1, int S2, int I1, int I2>
    struct cpp11_suffix<S1, S2, I1, I2, true>
    {
        BOOST_LEAF_ALWAYS_INLINE constexpr static bool check(char const (&str)[S1], char const (&suffix)[S2]) noexcept
        {
            return str[I1] == suffix[I2] && cpp11_suffix<S1, S2, I1 - 1, I2 - 1>::check(str, suffix);
        }
    };
    template <int S1, int S2, int I1>
    struct cpp11_suffix<S1, S2, I1, 0, true>
    {
        BOOST_LEAF_ALWAYS_INLINE constexpr static bool check(char const (&str)[S1], char const (&suffix)[S2]) noexcept
        {
            return str[I1] == suffix[0];
        }
    };
    template <int S1, int S2>
    BOOST_LEAF_ALWAYS_INLINE constexpr int check_suffix(char const (&str)[S1], char const (&suffix)[S2]) noexcept
    {
        return cpp11_suffix<S1, S2, S1 - 2, S2 - 2>::check(str, suffix) ? S1 - S2 : 0;
    }
}

namespace n
{
    struct r
    {
        char const * name;
        int len;
        r(char const * name, int len) noexcept:
            name(name),
            len(len)
        {
        }
        template <class CharT, class Traits>
        friend std::ostream & operator<<(std::basic_ostream<CharT, Traits> & os, r const & pn)
        {
            return os.write(pn.name, pn.len);
        }
    };

    template <class T>
    BOOST_LEAF_ALWAYS_INLINE r p()
    {
        // C++11 compile-time parsing of __PRETTY_FUNCTION__/__FUNCSIG__. The sizeof hacks are a
        // workaround for older GCC versions, where __PRETTY_FUNCTION__ is not constexpr, which triggers
        // compile errors when used in constexpr expressinos, yet evaluating a sizeof exrpession works.

        // We don't try to recognize the compiler based on compiler-specific macros. Any compiler/version
        // is supported as long as it uses one of the formats we recognize.

        // Unrecognized __PRETTY_FUNCTION__/__FUNCSIG__ formats will result in compiler diagnostics.
        // In that case, please file an issue on https://github.com/boostorg/leaf.

#define BOOST_LEAF_P(P) (sizeof(char[1 + detail::check_prefix(BOOST_LEAF_PRETTY_FUNCTION, P)]) - 1)
        // clang style:
        int const p01 = BOOST_LEAF_P("r boost::leaf::n::p() [T = ");
        // old clang style:
        int const p02 = BOOST_LEAF_P("boost::leaf::n::r boost::leaf::n::p() [T = ");
        // gcc style:
        int const p03 = BOOST_LEAF_P("boost::leaf::n::r boost::leaf::n::p() [with T = ");
        // msvc style, struct:
        int const p04 = BOOST_LEAF_P("struct boost::leaf::n::r __cdecl boost::leaf::n::p<struct ");
        int const p05 = BOOST_LEAF_P("struct boost::leaf::n::r __stdcall boost::leaf::n::p<struct ");
        int const p06 = BOOST_LEAF_P("struct boost::leaf::n::r __fastcall boost::leaf::n::p<struct ");
        // msvc style, class:
        int const p07 = BOOST_LEAF_P("struct boost::leaf::n::r __cdecl boost::leaf::n::p<class ");
        int const p08 = BOOST_LEAF_P("struct boost::leaf::n::r __stdcall boost::leaf::n::p<class ");
        int const p09 = BOOST_LEAF_P("struct boost::leaf::n::r __fastcall boost::leaf::n::p<class ");
        // msvc style, enum:
        int const p10 = BOOST_LEAF_P("struct boost::leaf::n::r __cdecl boost::leaf::n::p<enum ");
        int const p11 = BOOST_LEAF_P("struct boost::leaf::n::r __stdcall boost::leaf::n::p<enum ");
        int const p12 = BOOST_LEAF_P("struct boost::leaf::n::r __fastcall boost::leaf::n::p<enum ");
        // msvc style, built-in type:
        int const p13 = BOOST_LEAF_P("struct boost::leaf::n::r __cdecl boost::leaf::n::p<");
        int const p14 = BOOST_LEAF_P("struct boost::leaf::n::r __stdcall boost::leaf::n::p<");
        int const p15 = BOOST_LEAF_P("struct boost::leaf::n::r __fastcall boost::leaf::n::p<");
#undef BOOST_LEAF_P

#define BOOST_LEAF_S(S) (sizeof(char[1 + detail::check_suffix(BOOST_LEAF_PRETTY_FUNCTION, S)]) - 1)
        // clang/gcc style:
        int const s01 = BOOST_LEAF_S("]");
        // msvc style:
        int const s02 = BOOST_LEAF_S(">(void)");
#undef BOOST_LEAF_S

        char static_assert_unrecognized_pretty_function_format_please_file_github_issue[sizeof(
            char[
                (s01 && (1 == (!!p01 + !!p02 + !!p03)))
                ||
                (s02 && (1 == (!!p04 + !!p05 + !!p06 + !!p07 + !!p08 + !!p09 + !!p10 + !!p11 + !!p12)))
                ||
                (s02 && (1 == (!!p13 + !!p14 + !!p15)))
            ]
        ) * 2 - 1];
        (void) static_assert_unrecognized_pretty_function_format_please_file_github_issue;

        if( int const p = sizeof(char[1 + !!s01 * (p01 + p02 + p03)]) - 1 )
            return { BOOST_LEAF_PRETTY_FUNCTION + p, s01 - p };

        if( int const p = sizeof(char[1 + !!s02 * (p04 + p05 + p06 + p07 + p08 + p09 + p10 + p11 + p12)]) - 1 )
            return { BOOST_LEAF_PRETTY_FUNCTION + p, s02 - p };

        int const p = sizeof(char[1 + !!s02 * (p13 + p14 + p15)]) - 1; // p is not zero, we've static asserted the hell out of it
        return { BOOST_LEAF_PRETTY_FUNCTION + p, s02 - p };
    }
}

using parsed = n::r;

template <class T>
parsed parse()
{
    return n::p<T>();
}

} }

////////////////////////////////////////

namespace boost { namespace leaf {

namespace detail
{
    template <class CharT, class Traits>
    std::ostream & demangle_and_print(std::basic_ostream<CharT, Traits> & os, char const * mangled_name)
    {
        BOOST_LEAF_ASSERT(mangled_name);
#if defined(BOOST_LEAF_CFG_DIAGNOSTICS) && defined(BOOST_LEAF_HAS_CXXABI_H)
        struct raii
        {
            char * demangled_name;
            raii(char const * mangled_name) noexcept 
            {
                int status = 0;
                demangled_name = abi::__cxa_demangle(mangled_name, nullptr, nullptr, &status);
            }
            ~raii() noexcept
            {
                std::free(demangled_name);
            }
        } d(mangled_name);
        if( d.demangled_name )
            return os << d.demangled_name;
#endif
        return os << mangled_name;
    }
}

} }

#endif // BOOST_LEAF_DETAIL_DEMANGLE_HPP_INCLUDED
