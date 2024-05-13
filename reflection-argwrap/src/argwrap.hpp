#include <any>
#include <array>
#include <functional>
#include <iostream>
#include <tuple>
#include <type_traits>
#include <utility>

namespace reflect
{

    class ArgWrap
    {
    public:
        template<typename T>
        ArgWrap(T&& val)
        {
            // Debug type T
            // static_assert(std::is_same<T, void>::value, "Hoi!");
            m_ref_type = std::is_reference_v<T>;
            m_is_const = std::is_const_v<T>;
            if (m_ref_type == 1)
            {
                m_storage = &val;
            }
            else
            {
                m_storage = val;
            }
        }

        template<typename T>
        T Cast()
        {
            using RawT                       = std::decay_t<T>;
            constexpr int  k_cast_T_ref_type = std::is_reference_v<T>;
            constexpr bool k_cast_T_is_const = std::is_const_v<T>;
            if constexpr (k_cast_T_ref_type == 0)
            {
                if (m_ref_type == 1)
                {
                    // want copy, self is const-ref
                    if (m_is_const)
                        return *std::any_cast<const RawT*>(m_storage);
                    // want copy, self is mut-ref
                    else
                        return *std::any_cast<RawT*>(m_storage);
                }
                // want copy, self is copy
                return std::any_cast<RawT>(m_storage);
            }

            if (m_ref_type == 0)
            {
                // want const-ref, self is copy
                // want mut-ref, self is copy
                return *std::any_cast<RawT>(&m_storage);
            }
            if constexpr (k_cast_T_is_const)
            {
                // want const-ref, self is const-ref
                if (m_is_const)
                    return *std::any_cast<const RawT*>(m_storage);
                // want const-ref, self is mut-ref
                else
                    return *std::any_cast<RawT*>(m_storage);
            }
            else
            {
                if (m_is_const)
                {
                    throw std::runtime_error("Cannot cast const-ref to non-const ref");
                }
                // want mut-ref, self is mut-ref
                return *std::any_cast<RawT*>(m_storage);
            }
        }

    private:
        std::any m_storage {};
        int      m_ref_type {0};
        bool     m_is_const {false};
    };

    template<typename... Args, size_t N, size_t... Is>
    std::tuple<Args...> UnwarpAsTuple(std::array<ArgWrap, N>& arr, std::index_sequence<Is...>)
    {
        // Dont use std::make_tuple, which can't easily support reference.
        return std::forward_as_tuple(arr[Is].template Cast<Args>()...);
    }

    template<typename... Args, size_t N, typename = std::enable_if_t<(N == sizeof...(Args))>>
    std::tuple<Args...> UnwarpAsTuple(std::array<ArgWrap, N>& arr)
    {
        return UnwarpAsTuple<Args...>(arr, std::make_index_sequence<N> {});
    }
} // namespace reflect
