#pragma once
#include "renderer.h"
#include "defines.h"
#include "resource_manager.h"
#include <vector>
#include <string>

#define ECS_COMPONENTS \
	X(Transform_Component, transform_component) \
	X(Static_Mesh_Component, static_mesh_component) \
	X(Renderable_Component, renderable_component)

struct Renderable_Component
{
	Renderer* renderer;
	bool render_ready;
};

struct Transform_Component
{
	glm::quat rotation = glm::quat_identity<float, glm::packed_highp>();
	glm::vec3 pos = glm::vec3(0.f);
	float scale = 1.0;
};

struct Static_Mesh_Component
{
	Resource_Manager<Mesh>* manager;
	int32_t mesh_id;
};

template <typename T>
struct ECS_Component_Entry
{
	ECS_Component_Entry(std::string name)
	{
		component_name = name;
	}

	ECS_Component_Entry(const char* str)
	{
		component_name = std::string(str);
	}

	std::vector<std::optional<T>> components;
	std::string component_name;
	template<typename F>
	void iterate(F&& func)	//void (T& component, uint32_t entity_id)
	{
		const size_t size = components.size();
		for (size_t i = 0; i < size; ++i)
		{
			if (components[i].has_value())
				func(components[i].value(), (uint32_t)i);
		}
	}

	void add_component(T component, uint32_t entity_id)
	{
		while (entity_id >= components.size())
		{
			if (components.size() == 0)
				components.resize(32);
			else
				components.resize(components.size() * 2);
		}
		assert(!components[entity_id].has_value());
		components[entity_id] = component;
	}

	void remove_component(uint32_t entity_id)
	{
		assert(entity_id >= 0 && entity_id < components.size());
		components[entity_id].reset();
	}

	T* get_component(uint32_t entity_id)
	{
		assert(entity_id < components.size());
		if (components[entity_id].has_value())
		{
			return &components[entity_id].value();
		}
		else
			return nullptr;
	}
};

struct ECS
{
#define X(type, name) \
	ECS_Component_Entry<type> name = ECS_Component_Entry<type>(#name); 
	ECS_COMPONENTS
#undef X

	template <typename T>
	void add_component(uint32_t entity_id, T comp)
	{
#define X(type, name) \
		if constexpr(std::is_same<T, type>::value) name.add_component(comp, entity_id);
		ECS_COMPONENTS
#undef X
	}

	template<typename ...T>
	uint32_t add_entity(T... ts)
	{
		uint32_t new_id = add_entity();
		([&]
		{
			add_component(new_id, ts);
		} (), ...);

		return new_id;
	}

	template<typename T, typename F>
	void iterate(F&& func)
	{
#define X(type, name) \
		if constexpr(std::is_same<T, type>::value) name.iterate(func);
		ECS_COMPONENTS
#undef X
	}

	template<typename T>
	T* get_component(uint32_t entity_id)
	{
#define X(type, name) \
		if constexpr(std::is_same<T, type>::value) return name.get_component(entity_id);
		ECS_COMPONENTS
#undef X
	}

	uint32_t add_entity() { return next_free_id++; }
	uint32_t next_free_id = 0;
};
