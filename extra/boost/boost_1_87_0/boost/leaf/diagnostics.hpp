#ifndef BOOST_LEAF_DIAGNOSTICS_HPP_INCLUDED
#define BOOST_LEAF_DIAGNOSTICS_HPP_INCLUDED

// Copyright 2018-2024 Emil Dotchevski and Reverge Studios, Inc.
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/leaf/config.hpp>
#include <boost/leaf/context.hpp>
#include <boost/leaf/handle_errors.hpp>

namespace boost { namespace leaf {

#if BOOST_LEAF_CFG_DIAGNOSTICS

class diagnostic_info: public error_info
{
    void const * tup_;
    void (*print_tuple_contents_)(std::ostream &, void const * tup, error_id to_print, char const * & prefix);

protected:

    diagnostic_info( diagnostic_info const & ) noexcept = default;

    template <class Tup>
    BOOST_LEAF_CONSTEXPR diagnostic_info( error_info const & ei, Tup const & tup ) noexcept:
        error_info(ei),
        tup_(&tup),
        print_tuple_contents_(&detail::print_tuple_contents<Tup>)
    {
    }

    template <class CharT, class Traits>
    void print_diagnostic_info(std::basic_ostream<CharT, Traits> & os) const
    {
        print_error_info(os);
        char const * prefix = exception() ? nullptr : "\nCaught:" BOOST_LEAF_CFG_DIAGNOSTICS_FIRST_DELIMITER;
        print_tuple_contents_(os, tup_, error(), prefix);
    }

    template <class CharT, class Traits>
    friend std::ostream & operator<<( std::basic_ostream<CharT, Traits> & os, diagnostic_info const & x )
    {
        x.print_diagnostic_info(os);
        return os << '\n';
    }
};

namespace detail
{
    struct diagnostic_info_: diagnostic_info
    {
        template <class Tup>
        BOOST_LEAF_CONSTEXPR diagnostic_info_( error_info const & ei, Tup const & tup ) noexcept:
            diagnostic_info(ei, tup)
        {
        }
    };

    template <>
    struct handler_argument_traits<diagnostic_info const &>: handler_argument_always_available<e_source_location>
    {
        template <class Tup>
        BOOST_LEAF_CONSTEXPR static diagnostic_info_ get( Tup const & tup, error_info const & ei ) noexcept
        {
            return diagnostic_info_(ei, tup);
        }
    };
}

#else

class diagnostic_info: public error_info
{
protected:

    diagnostic_info( diagnostic_info const & ) noexcept = default;

    BOOST_LEAF_CONSTEXPR diagnostic_info( error_info const & ei ) noexcept:
        error_info(ei)
    {
    }

    template <class CharT, class Traits>
    void print_diagnostic_info( std::basic_ostream<CharT, Traits> & os ) const
    {
        print_error_info(os);
        os << "\nboost::leaf::diagnostic_info N/A due to BOOST_LEAF_CFG_DIAGNOSTICS=0";
    }

    template <class CharT, class Traits>
    friend std::ostream & operator<<( std::basic_ostream<CharT, Traits> & os, diagnostic_info const & x )
    {
        x.print_diagnostic_info(os);
        return os << "\n";
    }
};

namespace detail
{
    struct diagnostic_info_: diagnostic_info
    {
        BOOST_LEAF_CONSTEXPR diagnostic_info_( error_info const & ei ) noexcept:
            diagnostic_info(ei)
        {
        }
    };

    template <>
    struct handler_argument_traits<diagnostic_info const &>: handler_argument_always_available<e_source_location>
    {
        template <class Tup>
        BOOST_LEAF_CONSTEXPR static diagnostic_info_ get( Tup const &, error_info const & ei ) noexcept
        {
            return diagnostic_info_(ei);
        }
    };
}

#endif

////////////////////////////////////////

#if BOOST_LEAF_CFG_DIAGNOSTICS

#if BOOST_LEAF_CFG_CAPTURE

class diagnostic_details: public diagnostic_info
{
    detail::dynamic_allocator const * const da_;

protected:

    diagnostic_details( diagnostic_details const & ) noexcept = default;

    template <class Tup>
    BOOST_LEAF_CONSTEXPR diagnostic_details( error_info const & ei, Tup const & tup, detail::dynamic_allocator const * da ) noexcept:
        diagnostic_info(ei, tup),
        da_(da)
    {
    }

    template <class CharT, class Traits>
    void print_diagnostic_details( std::basic_ostream<CharT, Traits> & os) const
    {
        print_diagnostic_info(os);
        if( da_ )
        {
            char const * prefix = "\nDiagnostic details:" BOOST_LEAF_CFG_DIAGNOSTICS_FIRST_DELIMITER;
            da_->print(os, error(), prefix);
        }
    }

    template <class CharT, class Traits>
    friend std::ostream & operator<<( std::basic_ostream<CharT, Traits> & os, diagnostic_details const & x )
    {
        x.print_diagnostic_details(os);
        return os << '\n';
    }
};

namespace detail
{
    struct diagnostic_details_: diagnostic_details
    {
        template <class Tup>
        BOOST_LEAF_CONSTEXPR diagnostic_details_( error_info const & ei, Tup const & tup, dynamic_allocator const * da ) noexcept:
            diagnostic_details(ei, tup, da)
        {
        }
    };

    template <>
    struct handler_argument_traits<diagnostic_details const &>: handler_argument_always_available<e_source_location, dynamic_allocator>
    {
        template <class Tup>
        BOOST_LEAF_CONSTEXPR static diagnostic_details_ get( Tup const & tup, error_info const & ei ) noexcept
        {
            slot<dynamic_allocator> const * da = find_in_tuple<slot<dynamic_allocator>>(tup);
            return diagnostic_details_(ei, tup, da ? da->has_value_any_key() : nullptr );
        }
    };
}

#else

class diagnostic_details: public diagnostic_info
{
protected:

    diagnostic_details( diagnostic_details const & ) noexcept = default;

    template <class Tup>
    BOOST_LEAF_CONSTEXPR diagnostic_details( error_info const & ei, Tup const & tup ) noexcept:
        diagnostic_info(ei, tup)
    {
    }

    template <class CharT, class Traits>
    void print_diagnostic_details( std::basic_ostream<CharT, Traits> & os ) const
    {
        print_diagnostic_info(os);
        os << "\nboost::leaf::diagnostic_details N/A due to BOOST_LEAF_CFG_CAPTURE=0";
    }

    template <class CharT, class Traits>
    friend std::ostream & operator<<( std::basic_ostream<CharT, Traits> & os, diagnostic_details const & x )
    {
        x.print_diagnostic_details(os);
        return os << "\n";
    }
};

namespace detail
{
    struct diagnostic_details_: diagnostic_details
    {
        template <class Tup>
        BOOST_LEAF_CONSTEXPR diagnostic_details_( error_info const & ei, Tup const & tup ) noexcept:
            diagnostic_details(ei, tup)
        {
        }
    };

    template <>
    struct handler_argument_traits<diagnostic_details const &>: handler_argument_always_available<e_source_location>
    {
        template <class Tup>
        BOOST_LEAF_CONSTEXPR static diagnostic_details_ get( Tup const & tup, error_info const & ei ) noexcept
        {
            return diagnostic_details_(ei, tup);
        }
    };
}

#endif

#else

class diagnostic_details: public diagnostic_info
{
protected:

    diagnostic_details( diagnostic_details const & ) noexcept = default;

    BOOST_LEAF_CONSTEXPR diagnostic_details( error_info const & ei ) noexcept:
        diagnostic_info(ei)
    {
    }

    template <class CharT, class Traits>
    void print_diagnostic_details( std::basic_ostream<CharT, Traits> & os ) const
    {
        print_error_info(os);
        os << "\nboost::leaf::diagnostic_details N/A due to BOOST_LEAF_CFG_DIAGNOSTICS=0";
    }

    template <class CharT, class Traits>
    friend std::ostream & operator<<( std::basic_ostream<CharT, Traits> & os, diagnostic_details const & x )
    {
        x.print_diagnostic_details(os);
        return os << "\n";
    }
};

namespace detail
{
    struct diagnostic_details_: diagnostic_details
    {
        BOOST_LEAF_CONSTEXPR diagnostic_details_( error_info const & ei ) noexcept:
            diagnostic_details(ei)
        {
        }
    };

    template <>
    struct handler_argument_traits<diagnostic_details const &>: handler_argument_always_available<e_source_location>
    {
        template <class Tup>
        BOOST_LEAF_CONSTEXPR static diagnostic_details_ get( Tup const &, error_info const & ei ) noexcept
        {
            return diagnostic_details_(ei);
        }
    };
}

#endif

using verbose_diagnostic_info = diagnostic_details;

} }

#endif // BOOST_LEAF_DIAGNOSTICS_HPP_INCLUDED
