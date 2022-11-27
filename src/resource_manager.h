#pragma once
#include "defines.h"
#include "obj.h"

struct Mesh;

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
			Resource<T> new_resource = { std::move(data), filepath };
			resources.emplace_back(new_resource);
			return next_id++;
		}
	}

	int register_resource(T res, std::string path)
	{
		// Check if already exists
		for (const auto& res : resources)
		{
			assert(res.filepath != path);
			// Already exists
		}
		Resource<T> new_res = { std::move(res), path };
		resources.emplace_back(new_res);
		return next_id++;
	}

	T* get_resource_with_id(int id)
	{
		if (id >= (int)next_id) return nullptr;
		return &resources[id].resource;
	}
};

