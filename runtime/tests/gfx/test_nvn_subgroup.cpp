// SPDX-License-Identifier: GPL-3.0-or-later
#include "nvn_subgroup.h"
#include "test_framework.h"
#include <cstring>
NXTEST_GROUP(gfx_nvn_subgroup)
using namespace nxrt::nvn::be::gpu;
using namespace nxrt::gfx;
#include "subgroup_spirv.inc"

// Probe GLSL (glslangValidator -V --target-env vulkan1.1):
// #version 450
// #extension GL_KHR_shader_subgroup_ballot : require
// #extension GL_KHR_shader_subgroup_shuffle : require
// layout(local_size_x=64) in;
// layout(set=0,binding=0,std430) buffer Output { uvec4 values[]; };
// void main() {
//   uint i=gl_GlobalInvocationID.x;
//   values[i]=uvec4(gl_SubgroupSize,gl_SubgroupInvocationID,
//       subgroupBallot(i>=32).x,subgroupShuffle(i,0));
// }

NXTEST(required_size_chain_and_limits) {
    CHECK(!subgroup_shader({0x07230203,0x10300,0,2,0, (2u<<16)|17u,64}).used);
    CHECK(subgroup_shader({0x07230203,0x10300,0,2,0, (4u<<16)|71u,1,11,41}).used);
    DeviceFeatures f; f.subgroup_size_control = f.compute_full_subgroups = true;
    DeviceLimitsExt l; l.min_subgroup_size=32; l.max_subgroup_size=64;
    l.required_subgroup_size_stages=VK_SHADER_STAGE_ALL; l.max_compute_workgroup_subgroups=4;
    SubgroupShader shader{true,{64,1,1}};
    for (auto bit : {VK_SHADER_STAGE_VERTEX_BIT,VK_SHADER_STAGE_FRAGMENT_BIT,VK_SHADER_STAGE_GEOMETRY_BIT,
                    VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,VK_SHADER_STAGE_COMPUTE_BIT}) {
        VkPipelineRobustnessCreateInfoEXT previous{VK_STRUCTURE_TYPE_PIPELINE_ROBUSTNESS_CREATE_INFO_EXT};
        VkPipelineShaderStageCreateInfo stage{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,&previous}; stage.stage=bit;
        VkPipelineShaderStageRequiredSubgroupSizeCreateInfo size{};
        CHECK(require_guest_subgroup(stage,size,shader,f,l)); CHECK(stage.pNext==&size);
        CHECK(size.pNext==&previous); CHECK_EQ(size.requiredSubgroupSize,32u);
        CHECK_EQ(size.sType,VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_REQUIRED_SUBGROUP_SIZE_CREATE_INFO);
    }
    VkPipelineShaderStageCreateInfo st{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO}; st.stage=VK_SHADER_STAGE_COMPUTE_BIT;
    VkPipelineShaderStageRequiredSubgroupSizeCreateInfo size{};
    shader.used=false; CHECK(!require_guest_subgroup(st,size,shader,f,l)); CHECK(st.pNext==nullptr); shader.used=true;
    l.min_subgroup_size=64; CHECK(!require_guest_subgroup(st,size,shader,f,l)); l.min_subgroup_size=32;
    l.required_subgroup_size_stages=VK_SHADER_STAGE_FRAGMENT_BIT; CHECK(!require_guest_subgroup(st,size,shader,f,l)); l.required_subgroup_size_stages=VK_SHADER_STAGE_ALL;
    shader.local[0]=129; CHECK(!require_guest_subgroup(st,size,shader,f,l));
    shader.local[0]=48; CHECK(require_guest_subgroup(st,size,shader,f,l)); CHECK_EQ(st.flags,0u);
    st.pNext=nullptr; shader.local[0]=64; CHECK(require_guest_subgroup(st,size,shader,f,l));
    CHECK(st.flags & VK_PIPELINE_SHADER_STAGE_CREATE_REQUIRE_FULL_SUBGROUPS_BIT);
    st.pNext=nullptr; st.flags=0;
    shader.local[0]=16; shader.local[1]=16; l.max_compute_workgroup_subgroups=8;
    CHECK(require_guest_subgroup(st,size,shader,f,l)); CHECK_EQ(st.flags,0u);
    st.pNext=nullptr; shader.local[0]=0;
    CHECK(!require_guest_subgroup(st,size,shader,f,l));
    f.subgroup_size_control=false; st.pNext=nullptr; CHECK(!require_guest_subgroup(st,size,shader,f,l));
    const auto reflected=subgroup_shader({std::begin(kSubgroupProbe),std::end(kSubgroupProbe)});
    CHECK(reflected.used); CHECK_EQ(reflected.local[0],64u);
}

static void probe(const char* gpu) {
    DeviceDesc desc; desc.headless=1; desc.validation=1; desc.gpu=gpu;
    std::string error; auto d=Device::create(desc,&error);
    REQUIRE(d);
    if (!d->gpu_override_matched()) { nxtest::skip("requested GPU unavailable"); return; }
    CHECK(d->validation_enabled());
    auto dev=d->device();
    std::printf("    probe GPU: %s\n",d->properties().deviceName);
    std::vector<uint32_t> words(std::begin(kSubgroupProbe),std::end(kSubgroupProbe));
    auto shader=subgroup_shader(words);
    Buffer buffer; REQUIRE(d->create_buffer(64*16,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,MemoryUse::Readback,&buffer)==VK_SUCCESS);
    VkDescriptorSetLayoutBinding binding{0,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT};
    VkDescriptorSetLayoutCreateInfo li{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO}; li.bindingCount=1; li.pBindings=&binding;
    VkDescriptorSetLayout sl; REQUIRE(vkCreateDescriptorSetLayout(dev,&li,nullptr,&sl)==VK_SUCCESS);
    VkPipelineLayoutCreateInfo pli{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO}; pli.setLayoutCount=1; pli.pSetLayouts=&sl;
    VkPipelineLayout pl; REQUIRE(vkCreatePipelineLayout(dev,&pli,nullptr,&pl)==VK_SUCCESS);
    VkDescriptorPoolSize ps{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1};
    VkDescriptorPoolCreateInfo pi{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO}; pi.maxSets=1; pi.poolSizeCount=1; pi.pPoolSizes=&ps;
    VkDescriptorPool pool; REQUIRE(vkCreateDescriptorPool(dev,&pi,nullptr,&pool)==VK_SUCCESS);
    VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO}; ai.descriptorPool=pool; ai.descriptorSetCount=1; ai.pSetLayouts=&sl;
    VkDescriptorSet set; REQUIRE(vkAllocateDescriptorSets(dev,&ai,&set)==VK_SUCCESS);
    VkDescriptorBufferInfo bi{buffer.buffer,0,64*16};
    VkWriteDescriptorSet wr{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET}; wr.dstSet=set; wr.descriptorCount=1; wr.descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; wr.pBufferInfo=&bi;
    vkUpdateDescriptorSets(dev,1,&wr,0,nullptr);
    VkShaderModuleCreateInfo mi{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO}; mi.codeSize=words.size()*4; mi.pCode=words.data();
    VkShaderModule module; REQUIRE(vkCreateShaderModule(dev,&mi,nullptr,&module)==VK_SUCCESS);
    for (bool fixed : {false,true}) {
        VkComputePipelineCreateInfo ci{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO}; ci.layout=pl;
        ci.stage={VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO}; ci.stage.stage=VK_SHADER_STAGE_COMPUTE_BIT; ci.stage.module=module; ci.stage.pName="main";
        VkPipelineShaderStageRequiredSubgroupSizeCreateInfo size{};
        if (fixed) REQUIRE(require_guest_subgroup(ci.stage,size,shader,d->features(),d->limits_ext()));
        VkPipeline pipeline; REQUIRE(vkCreateComputePipelines(dev,VK_NULL_HANDLE,1,&ci,nullptr,&pipeline)==VK_SUCCESS);
        REQUIRE(d->submit_once([&](VkCommandBuffer cmd) {
            vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_COMPUTE,pipeline);
            vkCmdBindDescriptorSets(cmd,VK_PIPELINE_BIND_POINT_COMPUTE,pl,0,1,&set,0,nullptr);
            vkCmdDispatch(cmd,1,1,1);
            VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER}; barrier.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT; barrier.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
            vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,1,&barrier,0,nullptr,0,nullptr);
        }));
        d->invalidate_buffer(buffer);
        auto* out=static_cast<uint32_t*>(buffer.mapped); uint32_t mismatches=0;
        for (uint32_t i=0;i<64;i++) if(out[4*i]!=32 || out[4*i+1]!=i%32 || out[4*i+2]!=(i>=32?0xffffffffu:0u) || out[4*i+3]!=(i/32)*32) ++mismatches;
        std::printf("    %s: subgroup size %u, guest-warp mismatches %u/64\n",fixed?"required32":"default",out[0],mismatches);
        if(fixed) CHECK_EQ(mismatches,0u);
        vkDestroyPipeline(dev,pipeline,nullptr);
    }
    vkDestroyShaderModule(dev,module,nullptr); vkDestroyDescriptorPool(dev,pool,nullptr);
    vkDestroyPipelineLayout(dev,pl,nullptr); vkDestroyDescriptorSetLayout(dev,sl,nullptr); d->destroy_buffer(buffer);
    CHECK_EQ(d->validation_stats().errors,0u);
}
NXTEST(ballot_shuffle_nvidia) { probe("NVIDIA"); }
NXTEST(ballot_shuffle_amd) { probe("AMD Radeon"); }
