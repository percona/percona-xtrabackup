#ifndef BOOST_LEAF_DETAIL_PRINT_HPP_INCLUDED
#define BOOST_LEAF_DETAIL_PRINT_HPP_INCLUDED

// Copyright 2018-2024 Emil Dotchevski and Reverge Studios, Inc.
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/leaf/config.hpp>
#include <boost/leaf/detail/demangle.hpp>

#include <type_traits>
#include <exception>
#include <iosfwd>
#include <cstring>

namespace boost { namespace leaf {

template <class E>
struct show_in_diagnostics: std::true_type
{
};

namespace detail
{
    template <class T, class E = void>
    struct is_printable: std::false_type
    {
    };

    template <class T>
    struct is_printable<T, decltype(std::declval<std::ostream&>()<<std::declval<T const &>(), void())>: show_in_diagnostics<T>
    {
    };

    ////////////////////////////////////////

    template <class T, class E = void>
    struct has_printable_member_value: std::false_type
    {
    };

    template <class T>
    struct has_printable_member_value<T, decltype(std::declval<std::ostream&>()<<std::declval<T const &>().value, void())>: show_in_diagnostics<T>
    {
    };

    ////////////////////////////////////////

    template <class T, class CharT, class Traits>
    void print_name(std::basic_ostream<CharT, Traits> & os, char const * & prefix, char const * delimiter)
    {
        static_assert(show_in_diagnostics<T>::value, "show_in_diagnostics violation");
        BOOST_LEAF_ASSERT(delimiter);
        char const * p = prefix;
        prefix = nullptr;
        os << (p ? p : delimiter) << parse<T>();
    }

    template <class T, class PrintableInfo, class CharT, class Traits>
    bool print_impl(std::basic_ostream<CharT, Traits> & os, char const * & prefix, char const * delimiter, char const * mid, PrintableInfo const & x)
    {
        print_name<T>(os, prefix, delimiter);
        if( mid )
            os << mid << x;
        return true;
    }

    template <class T, class PrintableInfo, class CharT, class Traits>
    bool print_impl(std::basic_ostream<CharT, Traits> & os, char const * & prefix, char const * delimiter, char const * mid, PrintableInfo const * x)
    {
        print_name<T>(os, prefix, delimiter);
        if( mid )
        {
            os << mid;
            if( x )
                os << x;
            else
                os << "<nullptr>";
        }
        return true;
    }

    ////////////////////////////////////////

    template <
        class Wrapper,
        bool ShowInDiagnostics = show_in_diagnostics<Wrapper>::value,
        bool WrapperPrintable = is_printable<Wrapper>::value,
        bool ValuePrintable = has_printable_member_value<Wrapper>::value,
        bool IsException = std::is_base_of<std::exception,Wrapper>::value,
        bool IsEnum = std::is_enum<Wrapper>::value>
    struct diagnostic;

    template <class Wrapper, bool WrapperPrintable, bool ValuePrintable, bool IsException, bool IsEnum>
    struct diagnostic<Wrapper, false, WrapperPrintable, ValuePrintable, IsException, IsEnum>
    {
        template <class CharT, class Traits>
        static bool print(std::basic_ostream<CharT, Traits> &, char const * &, char const *, Wrapper const & x) noexcept
        {
            return false;
        }
    };

    template <class Wrapper, bool ValuePrintable, bool IsEnum>
    struct diagnostic<Wrapper, true, true, ValuePrintable, false, IsEnum>
    {
        template <class CharT, class Traits>
        static bool print(std::basic_ostream<CharT, Traits> & os, char const * & prefix, char const * delimiter, Wrapper const & x)
        {
            return print_impl<Wrapper>(os, prefix, delimiter, ": ", x);
        }
    };

    template <class Wrapper>
    struct diagnostic<Wrapper, true, false, true, false, false>
    {
        template <class CharT, class Traits>
        static bool print(std::basic_ostream<CharT, Traits> & os, char const * & prefix, char const * delimiter, Wrapper const & x)
        {
            return print_impl<Wrapper>(os, prefix, delimiter, ": ", x.value);
        }
    };

    template <class Exception, bool WrapperPrintable, bool ValuePrintable>
    struct diagnostic<Exception, true, WrapperPrintable, ValuePrintable, true, false>
    {
        template <class CharT, class Traits>
        static bool print(std::basic_ostream<CharT, Traits> & os, char const * & prefix, char const * delimiter, Exception const & ex)
        {
            if( print_impl<Exception>(os, prefix, delimiter, ": \"", static_cast<std::exception const &>(ex).what()) )
            {
                os << '"';
                return true;
            }
            return false;
        }
    };

    template <class Wrapper>
    struct diagnostic<Wrapper, true, false, false, false, false>
    {
        template <class CharT, class Traits>
        static bool print(std::basic_ostream<CharT, Traits> & os, char const * & prefix, char const * delimiter, Wrapper const &)
        {
            return print_impl<Wrapper>(os, prefix, delimiter, nullptr, 0);
        }
    };

    template <class Enum>
    struct diagnostic<Enum, true, false, false, false, true>
    {
        template <class CharT, class Traits>
        static bool print(std::basic_ostream<CharT, Traits> & os, char const * & prefix, char const * delimiter, Enum const & enum_)
        {
            return print_impl<Enum>(os, prefix, delimiter, ": ", static_cast<typename std::underlying_type<Enum>::type>(enum_));
        }
    };
}

} }

#endif // BOOST_LEAF_DETAIL_PRINT_HPP_INCLUDED
