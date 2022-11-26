#pragma once
#include "renderer.h"
#include "defines.h"
#include "resource_manager.h"
#include <vector>
#include <string>
#include <iterator>
#include <cstddef>
#include <tuple>
#include <iostream>


#define ECS_COMPONENTS \
	X(Transform_Component, transform_component) \
	X(Static_Mesh_Component, static_mesh_component) \
	X(Renderable_Component, renderable_component) \
	X(Camera_Component, camera_component) \
	X(Velocity_Component, velocity_component)



struct Renderable_Component
{
	Renderer* renderer;
	bool render_ready;
};

struct Velocity_Component
{
	glm::vec3 velocity;
};

struct Transform_Component
{
	glm::quat rotation = glm::quat_identity<float, glm::packed_highp>();
	glm::vec3 pos = glm::vec3(0.f);
	float scale = 1.0;
};

struct Camera_Component
{
	glm::mat4 view;
	glm::mat4 proj;
	bool dirty = false;
	void set_transform(Transform_Component* xform);
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
	uint32_t num_components = 0; //different from table size

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
		++num_components;
	}

	void remove_component(uint32_t entity_id)
	{
		assert(entity_id >= 0 && entity_id < components.size());
		components[entity_id].reset();
		--num_components;
	}

	T* get_component(uint32_t entity_id)
	{
		if (entity_id > components.size()) return nullptr;
		assert(entity_id < components.size());
		if (components[entity_id].has_value())
		{
			return &components[entity_id].value();
		}
		else
			return nullptr;
	}

	struct Comp_Iterator
	{
		using iterator_category = std::forward_iterator_tag;

		Comp_Iterator(ECS_Component_Entry<T>* entry, uint32_t start_index) : 
			comps(&entry->components),
			current_idx(start_index),
			max_idx((u32)entry->components.size())
		{
		}

		u32& operator*() const
		{
			return (u32)current_idx;
		}

		Comp_Iterator& operator++()
		{ 
			do {
				current_idx++;
			} while (current_idx < max_idx && !(*comps)[current_idx].has_value());

			return *this;
		}
		Comp_Iterator operator++(int) 
		{ 
			Iterator tmp = *this; 
			++(*this); 
			return tmp; 
		}

		friend bool operator== (const Comp_Iterator& a, const Comp_Iterator& b) 
		{ 
			return a.current_idx == b.current_idx; 
		}
		friend bool operator!= (const Comp_Iterator& a, const Comp_Iterator& b) 
		{ 
			return a.current_idx != b.current_idx; 
		}

		std::vector<std::optional<T>>* comps;
		u32 current_idx;
		u32 max_idx;
	};

	Comp_Iterator begin() 
	{
		return Comp_Iterator(this, 0);
	}

	Comp_Iterator end()
	{
		return Comp_Iterator(this, (u32)components.size());
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
		(add_component(new_id, ts), ...);

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

	template<typename ...T>
	std::tuple<T*...> get_comp_pointers(u32 entity_id, ECS_Component_Entry<T>*... c)
	{
		return { c->get_component(entity_id)... };
	}

	template<typename ...T>
	struct Filter_Iterator
	{
		std::tuple<ECS_Component_Entry<T>*...> comps;
		u32 current_idx = 0;
		u32 max_idx = 0;
		using value_type = std::tuple<T*...>;
		Filter_Iterator(int pos, ECS_Component_Entry<T>*... comps) // 0 for start, -1 for end
			: comps({ comps... }),
			current_idx(0),
			max_idx(0)
		{
			max_idx = (u32)std::get<0>(this->comps)->components.size();
			if (pos == -1) current_idx = max_idx;
			else
			{
				u32 i = max_idx;
				for (auto& c : *(std::get<0>(this->comps)))
				{
					bool found = true;
					std::apply([&found, c](auto&&... args) {((found = (found && args->get_component(c) != nullptr)), ...); }, this->comps);
					if (found)
					{
						i = c;
						break;
					}
				}
				current_idx = i;
			}
		}

		Filter_Iterator()
		{
		}

		value_type operator*()
		{
			return std::apply([&](auto&&... args)
				{
					return std::make_tuple(args->get_component(current_idx)...);
				}, this->comps);
		}

		Filter_Iterator operator++()
		{
			bool found = true;
			uint32_t idx = current_idx;
			do
			{
				idx++;
				std::apply([&found, &idx](auto&&... args) {((found = (found && args->get_component(idx) != nullptr)), ...); }, this->comps);
			} while (!found && idx < max_idx);
			current_idx = idx;
			return *this;
		}

		Filter_Iterator operator++(int)
		{
			FilterIterator tmp = *this;
			++(*this);
			return tmp;
		}

		bool operator==(const Filter_Iterator& rhs)
		{
			return current_idx == rhs.current_idx;
		}

		bool operator!=(const Filter_Iterator& rhs)
		{
			return current_idx != rhs.current_idx;
		}
	};

	template <typename ...T>
	struct Filter_Helper
	{
		Filter_Iterator<T...> start_;
		Filter_Iterator<T...> end_;
		Filter_Helper<T...>(ECS_Component_Entry<T>* ...ts)
		{
			start_ = Filter_Iterator(0, ts...);
			end_ = Filter_Iterator(-1, ts...);
		}

		Filter_Iterator<T...> begin()
		{
			return start_;
		}

		Filter_Iterator<T...> end()
		{
			return end_;
		}
	};

	template <typename ...T>
	Filter_Helper<T...> filter()
	{	
		return Filter_Helper<T...>(get_ptr<T>()...);
	}

	template<typename T>
	ECS_Component_Entry<T>* get_ptr()
	{
#define X(type, name) \
		if constexpr(std::is_same<T, type>::value) return &name;
		ECS_COMPONENTS
#undef X
	}

	uint32_t add_entity() { return next_free_id++; }
	uint32_t next_free_id = 0;
};
