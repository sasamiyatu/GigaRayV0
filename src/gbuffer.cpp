#include "gbuffer.h"


Gbuffer_Renderer::Gbuffer_Renderer(Vk_Context* ctx)
	: ctx(ctx),
	albedo(nullptr),
	normal(nullptr),
	world_pos(nullptr),
	depth(nullptr)
{
}

void Gbuffer_Renderer::initialize(Render_Target* albedo_rt, Render_Target* normal_rt, Render_Target* world_pos_rt, Render_Target* depth_rt)
{
	albedo = albedo_rt;
	normal = normal_rt;
	world_pos = world_pos_rt;
	depth = depth_rt;


}

void Gbuffer_Renderer::run()
{

}
