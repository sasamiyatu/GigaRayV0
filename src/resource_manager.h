#pragma once
#include "defines.h"
#include "renderer.h"
#include "obj.h"

template <typename T>
struct Resource
{
	T resource;
	std::string filepath;
};

template <typename T>
struct Resource_Manager
{
	std::vector<Resource<T>> resources;
	std::vector<uint32_t> ids;
	uint32_t next_id = 0;

	int load_from_disk(const char* filepath)
	{
		if constexpr(std::is_same<T, Mesh>::value)
		{
			T data = load_obj_from_file(filepath);
			Resource<T> new_resource = { data, filepath };
			resources.push_back(new_resource);
			return next_id++;
		}
	}

	T* get_resource_with_id(int id)
	{
		if (id >= (int)next_id) return nullptr;
		return &resources[id].resource;
	}
};