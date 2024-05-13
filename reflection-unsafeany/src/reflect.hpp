#pragma once

#include "unsafe_any.hpp"

#include <any>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

namespace reflect
{
    class MemberVariable
    {
    public:
        MemberVariable() = default;

        template<typename C, typename T>
        MemberVariable(T C::*var)
        {
            getter_ = [var](std::any obj) -> std::any { return std::any_cast<const C*>(obj)->*var; };
            setter_ = [var](std::any obj, std::any val) {
                // Syntax: https://stackoverflow.com/a/670744/12003165
                // `obj.*member_var`
                auto* self = std::any_cast<C*>(obj);
                self->*var = std::any_cast<T>(val);
            };
        }

        const std::string& name() const { return m_name; }

        template<typename T, typename C>
        T GetValue(const C& c) const
        {
            return std::any_cast<T>(getter_(&c));
        }

        template<typename C, typename T>
        void SetValue(C& c, T val)
        {
            setter_(&c, val);
        }

    private:
        friend class RawTypeDescriptorBuilder;

        std::string                             m_name;
        std::function<std::any(std::any)>       getter_ {nullptr};
        std::function<void(std::any, std::any)> setter_ {nullptr};
    };

    class MemberFunction
    {
    public:
        MemberFunction() = default;

        template<typename C, typename R, typename... Args>
        explicit MemberFunction(R (C::*func)(Args...))
        {
            m_function = [this, func](std::any obj_args) -> std::any {
                auto& warpped_args = *std::any_cast<std::array<UnsafeAny, sizeof...(Args) + 1>*>(obj_args);
                auto  tuple_args   = UnwarpAsTuple<C&, Args...>(warpped_args);
                return std::apply(func, tuple_args);
            };
            m_args_number = sizeof...(Args);
        }

        template<typename C, typename... Args>
        explicit MemberFunction(void (C::*func)(Args...))
        {
            m_function = [this, func](std::any obj_args) -> std::any {
                auto& warpped_args = *std::any_cast<std::array<UnsafeAny, sizeof...(Args) + 1>*>(obj_args);
                auto  tuple_args   = UnwarpAsTuple<C&, Args...>(warpped_args);
                std::apply(func, tuple_args);
                return std::any {};
            };
            m_args_number = sizeof...(Args);
        }

        template<typename C, typename R, typename... Args>
        explicit MemberFunction(R (C::*func)(Args...) const)
        {
            m_function = [this, func](std::any obj_args) -> std::any {
                auto& warpped_args = *std::any_cast<std::array<UnsafeAny, sizeof...(Args) + 1>*>(obj_args);
                auto  tuple_args   = UnwarpAsTuple<const C&, Args...>(warpped_args);
                return std::apply(func, tuple_args);
            };
            m_is_const    = true;
            m_args_number = sizeof...(Args);
        }

        template<typename C, typename... Args>
        explicit MemberFunction(void (C::*func)(Args...) const)
        {
            m_function = [this, func](std::any obj_args) -> std::any {
                auto& warpped_args = *std::any_cast<std::array<UnsafeAny, sizeof...(Args) + 1>*>(obj_args);
                auto  tuple_args   = UnwarpAsTuple<const C&, Args...>(warpped_args);
                std::apply(func, tuple_args);
                return std::any {};
            };
            m_is_const    = true;
            m_args_number = sizeof...(Args);
        }

        const std::string& name() const { return m_name; }

        bool is_const() const { return m_is_const; }

        template<typename C, typename... Args>
        std::any Invoke(C& c, Args&&... args)
        {
            if (m_args_number != sizeof...(Args))
            {
                throw std::runtime_error("Mismatching number of args!");
            }

            if (m_is_const)
            {
                std::array<UnsafeAny, sizeof...(Args) + 1> args_arr = {UnsafeAny {c},
                                                                       UnsafeAny {std::forward<Args>(args)}...};
                return m_function(&args_arr);
            }
            else
            {
                std::array<UnsafeAny, sizeof...(Args) + 1> args_arr = {UnsafeAny {c},
                                                                       UnsafeAny {std::forward<Args>(args)}...};
                return m_function(&args_arr);
            }
        }

    private:
        friend class RawTypeDescriptorBuilder;

        std::string                       m_name;
        std::function<std::any(std::any)> m_function {nullptr};
        bool                              m_is_const {false};
        int                               m_args_number {0};
    };

    class TypeDescriptor
    {
    public:
        const std::string& name() const { return m_name; }

        const std::vector<MemberVariable>& member_vars() const { return m_member_vars; }

        const std::vector<MemberFunction>& member_funcs() const { return m_member_funcs; }

        MemberVariable GetMemberVar(const std::string& name) const
        {
            for (const auto& member_var : m_member_vars)
            {
                if (member_var.name() == name)
                {
                    return member_var;
                }
            }
            return MemberVariable {};
        }

        MemberFunction GetMemberFunc(const std::string& name) const
        {
            for (const auto& member_func : m_member_funcs)
            {
                if (member_func.name() == name)
                {
                    return member_func;
                }
            }
            return MemberFunction {};
        }

    private:
        friend class RawTypeDescriptorBuilder;

        std::string                 m_name;
        std::vector<MemberVariable> m_member_vars;
        std::vector<MemberFunction> m_member_funcs;
    };

    class RawTypeDescriptorBuilder
    {
    public:
        explicit RawTypeDescriptorBuilder(const std::string& name);

        ~RawTypeDescriptorBuilder();
        RawTypeDescriptorBuilder(const RawTypeDescriptorBuilder&)            = delete;
        RawTypeDescriptorBuilder& operator=(const RawTypeDescriptorBuilder&) = delete;
        RawTypeDescriptorBuilder(RawTypeDescriptorBuilder&&)                 = default;
        RawTypeDescriptorBuilder& operator=(RawTypeDescriptorBuilder&&)      = default;

        template<typename C, typename T>
        void AddMemberVar(const std::string& name, T C::*var)
        {
            MemberVariable member_var {var};
            member_var.m_name = name;
            desc_->m_member_vars.push_back(std::move(member_var));
        }

        template<typename FUNC>
        void AddMemberFunc(const std::string& name, FUNC func)
        {
            MemberFunction member_func {func};
            member_func.m_name = name;
            desc_->m_member_funcs.push_back(std::move(member_func));
        }

    private:
        std::unique_ptr<TypeDescriptor> desc_ {nullptr};
    };

    template<typename T>
    class TypeDescriptorBuilder
    {
    public:
        explicit TypeDescriptorBuilder(const std::string& name)
            : raw_builder_(name)
        {}

        template<typename V>
        TypeDescriptorBuilder& AddMemberVar(const std::string& name, V T::*var)
        {
            raw_builder_.AddMemberVar(name, var);
            return *this;
        }

        template<typename FUNC>
        TypeDescriptorBuilder& AddMemberFunc(const std::string& name, FUNC func)
        {
            raw_builder_.AddMemberFunc(name, func);
            return *this;
        }

    private:
        RawTypeDescriptorBuilder raw_builder_;
    };

    class Registry
    {
    public:
        static Registry& instance()
        {
            static Registry inst;
            return inst;
        }

        TypeDescriptor* Find(const std::string& name);

        void Register(std::unique_ptr<TypeDescriptor> desc);

        void Clear();

    private:
        std::unordered_map<std::string, std::unique_ptr<TypeDescriptor>> type_descs_;
    };

    template<typename T>
    TypeDescriptorBuilder<T> AddClass(const std::string& name)
    {
        return TypeDescriptorBuilder<T> {name};
    }

    TypeDescriptor& GetByName(const std::string& name);

    void ClearRegistry();

} // namespace reflect
