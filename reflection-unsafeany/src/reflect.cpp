#include "reflect.hpp"

#include <iostream>

namespace reflect
{
    RawTypeDescriptorBuilder::RawTypeDescriptorBuilder(const std::string& name)
        : desc_(std::make_unique<TypeDescriptor>())
    {
        desc_->m_name = name;
    }

    RawTypeDescriptorBuilder::~RawTypeDescriptorBuilder() { Registry::instance().Register(std::move(desc_)); }

    TypeDescriptor* Registry::Find(const std::string& name) { return type_descs_.find(name)->second.get(); }

    void Registry::Register(std::unique_ptr<TypeDescriptor> desc)
    {
        if (desc == nullptr)
            return;

        auto name         = desc->name();
        type_descs_[name] = std::move(desc);
    }

    void Registry::Clear()
    {
        decltype(type_descs_) tmp;
        tmp.swap(type_descs_);
    }

    TypeDescriptor& GetByName(const std::string& name) { return *Registry::instance().Find(name); }

    void ClearRegistry() { Registry::instance().Clear(); }

} // namespace reflect