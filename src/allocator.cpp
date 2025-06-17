#include "vm.hpp"

using namespace vm;

runtime::Object *memory::Allocator::create(runtime::Type type, const byte *data, std::size_t data_size)
{
    if (allocated_objects.size() == allocated_objects.capacity())
    {
        collect_garbage();
    }

    runtime::Object *obj = new runtime::Object(type, data, data_size);
    allocated_objects.push_back(obj);
    return obj;
}

std::size_t vm::memory::Allocator::size() const
{
    return allocated_objects.size();
}

vm::memory::Allocator::~Allocator()
{
    for (runtime::Object *obj : allocated_objects)
    {
        delete obj;
    }
}

void vm::memory::Allocator::collect_garbage()
{
    std::size_t current = 0, first_free = 0;
    for (; current < allocated_objects.size(); ++current)
    {
        runtime::Object *obj = allocated_objects[current];
        if (obj->links == 0)
        {
            delete obj;
        }
        else
        {
            allocated_objects[first_free++] = obj;
        }
    }
    allocated_objects.resize(first_free);
}
