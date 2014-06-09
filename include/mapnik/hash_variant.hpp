//-----------------------------------------------------------------------------
// boost variant/detail/hash_variant.hpp header file
// See http://www.boost.org for updates, documentation, and revision history.
//-----------------------------------------------------------------------------
//
// Copyright (c) 2011
// Antony Polukhin
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// TODO: Remove this file once the minimum Boost version is bumped to 1.50

#ifndef BOOST_HASH_VARIANT_FUNCTION_HPP
#define BOOST_HASH_VARIANT_FUNCTION_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif

// stl
#include <functional>
// variant
#include "/Users/artem/Projects/variant/variant.hpp"

using namespace mapbox;

namespace boost { namespace detail { namespace variant {

template <class T>
inline void hash_combine(std::size_t& seed, const T& v)
{
    std::hash<T> hasher;
    seed ^= hasher(v) + 0x9e3779b9 + (seed<<6) + (seed>>2);
}

struct variant_hasher: public util::static_visitor<std::size_t>
{
    template <class T>
    std::size_t operator()(T const& val) const
    {
        std::hash<T> hasher;
        return hasher(val);
    }
};
}}

template <typename T>
std::size_t hash_value(T const& val)
{
    std::size_t seed = util::apply_visitor(detail::variant::variant_hasher(), val);
    hash_combine(seed, val.which());
    return seed;
}

}

#endif // MAPNIK_HASH_VARIANT_HPP
