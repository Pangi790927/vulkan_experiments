#define LOGGER_VERBOSE_LVL 0

#include "vulkan_utils.h"
#include "debug.h"
#include "misc_utils.h"
#include "time_utils.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

struct part_t {
    glm::vec2 pos;
    glm::vec2 vel;
    glm::vec4 color;
};

static auto create_vbuff(auto dev, auto cp, const std::vector<vku_vertex3d_t>& vertices) {
    size_t verts_sz = vertices.size() * sizeof(vertices[0]);
    auto staging_vbuff = new vku_buffer_t(
        dev,
        verts_sz,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_SHARING_MODE_EXCLUSIVE,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    memcpy(staging_vbuff->map_data(0, verts_sz), vertices.data(), verts_sz);
    staging_vbuff->unmap_data();

    auto vbuff = new vku_buffer_t(
        dev,
        verts_sz,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_SHARING_MODE_EXCLUSIVE,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );
    vku_copy_buff(cp, vbuff, staging_vbuff, verts_sz);
    delete staging_vbuff;
    return vbuff;
}

static auto create_ibuff(auto dev, auto cp, const std::vector<uint16_t>& indices) {
    size_t idxs_sz = indices.size() * sizeof(indices[0]);
    auto staging_ibuff = new vku_buffer_t(
        dev,
        idxs_sz,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_SHARING_MODE_EXCLUSIVE,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    memcpy(staging_ibuff->map_data(0, idxs_sz), indices.data(), idxs_sz);
    staging_ibuff->unmap_data();

    auto ibuff = new vku_buffer_t(
        dev,
        idxs_sz,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_SHARING_MODE_EXCLUSIVE,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );
    vku_copy_buff(cp, ibuff, staging_ibuff, idxs_sz);
    delete staging_ibuff;
    return ibuff;
}

static auto load_image(vku_cmdpool_t *cp, std::string path) {
    int w, h, chans;
    stbi_uc* pixels = stbi_load(path.c_str(), &w, &h, &chans, STBI_rgb_alpha);

    /* TODO: some more logs around here */
    vk_device_size_t imag_sz = w*h*4;
    if (!pixels) {
        throw vku_err_t("Failed to load image");
    }

    auto img = new vku_image_t(cp->dev, w, h, VK_FORMAT_R8G8B8A8_SRGB);
    img->set_data(cp, pixels, imag_sz);

    stbi_image_free(pixels);

    return img;
}

int main(int argc, char const *argv[])
{
    DBG_SCOPE();

    // const std::vector<vku_vertex2d_t> vertices = {
    //     {{0.0f, -0.5f}, {1.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
    //     {{0.5f,  0.5f}, {0.0f, 1.0f, 1.0f}, {0.0f, 0.0f}},
    //     {{-0.5f, 0.5f}, {1.0f, 1.0f, 0.0f}, {0.0f, 0.0f}}
    // };

    const std::vector<vku_vertex3d_t> vertices = {
        {{-0.5f, -0.5f,  0.0f}, {0, 0, 0}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
        {{ 0.5f, -0.5f,  0.0f}, {0, 0, 0}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
        {{ 0.5f,  0.5f,  0.0f}, {0, 0, 0}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
        {{-0.5f,  0.5f,  0.0f}, {0, 0, 0}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},

        {{-0.5f, -0.5f, -0.5f}, {0, 0, 0}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
        {{ 0.5f, -0.5f, -0.5f}, {0, 0, 0}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
        {{ 0.5f,  0.5f, -0.5f}, {0, 0, 0}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
        {{-0.5f,  0.5f, -0.5f}, {0, 0, 0}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},
    };

    const std::vector<uint16_t> indices = {
        0, 1, 2, 2, 3, 0,
        4, 5, 6, 6, 7, 4
    };

    vku_mvp_t mvp;

    vku_opts_t opts;
    auto inst = new vku_instance_t(opts);

    auto vert = vku_spirv_compile(inst, VKU_SPIRV_VERTEX, R"___(
        #version 450

        layout(binding = 0) uniform ubo_t {
            mat4 model;
            mat4 view;
            mat4 proj;
        } ubo;

        layout(location = 0) in vec3 in_pos;    // those are referenced by
        layout(location = 1) in vec3 in_normal; // vku_vertex3d_t::get_input_desc()
        layout(location = 2) in vec3 in_color;
        layout(location = 3) in vec2 in_tex;

        layout(location = 0) out vec3 out_color;
        layout(location = 1) out vec2 out_tex_coord;

        void main() {
            gl_Position = ubo.proj * ubo.view * ubo.model * vec4(in_pos, 1.0);
            out_color = in_color;
            out_tex_coord = in_tex;
        }

    )___");

    auto frag = vku_spirv_compile(inst, VKU_SPIRV_FRAGMENT, R"___(
        #version 450

        layout(location = 0) in vec3 in_color;      // this is referenced by the vert shader
        layout(location = 1) in vec2 in_tex_coord;  // this is referenced by the vert shader

        layout(location = 0) out vec4 out_color;

        layout(binding = 1) uniform sampler2D tex_sampler;

        void main() {
            out_color = vec4(in_color, 1.0);
            out_color = texture(tex_sampler, in_tex_coord);
        }
    )___");

    auto surf =     new vku_surface_t(inst);
    auto dev =      new vku_device_t(surf);
    auto cp =       new vku_cmdpool_t(dev);

    auto img = load_image(cp, "test_image.png");
    auto view = new vku_img_view_t(img, VK_IMAGE_ASPECT_COLOR_BIT);
    auto sampl = new vku_img_sampl_t(dev);

    auto mvp_buff = new vku_buffer_t(
        dev,
        sizeof(mvp),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_SHARING_MODE_EXCLUSIVE,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    auto mvp_pbuff = mvp_buff->map_data(0, sizeof(vku_mvp_t));

    vku_binding_desc_t bindings = {
        .binds = {
            vku_binding_desc_t::buff_binding_t::make_bind(
                vku_ubo_t::get_desc_set(0, VK_SHADER_STAGE_VERTEX_BIT),
                mvp_buff
            ),
            vku_binding_desc_t::sampl_binding_t::make_bind(
                vku_img_sampl_t::get_desc_set(1, VK_SHADER_STAGE_FRAGMENT_BIT),
                view,
                sampl
            ),
        },
    };

    auto sh_vert =  new vku_shader_t(dev, vert);
    auto sh_frag =  new vku_shader_t(dev, frag);
    auto swc =      new vku_swapchain_t(dev);
    auto rp =       new vku_renderpass_t(swc);
    auto pl =       new vku_pipeline_t(
        opts,
        rp,
        {sh_vert, sh_frag},
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        vku_vertex3d_t::get_input_desc(),
        bindings
    );
    auto fbs =      new vku_framebuffs_t(rp);

    auto img_sem =  new vku_sem_t(dev);
    auto draw_sem = new vku_sem_t(dev);
    auto fence =    new vku_fence_t(dev);

    auto cbuff =      new vku_cmdbuff_t(cp);

    auto vbuff = create_vbuff(dev, cp, vertices);
    auto ibuff = create_ibuff(dev, cp, indices);

    auto desc_pool = new vku_desc_pool_t(dev, bindings, 1);
    auto desc_set = new vku_desc_set_t(desc_pool, pl->vk_desc_set_layout, bindings);

    /* TODO: print a lot more info on vulkan, available extensions, size of memory, etc. */

    /* TODO: the program ever only draws on one image and waits on the fence, we need to use
    at least two images to speed up the draw process */
    // std::map<uint32_t, vku_sem_t *> img_sems;
    // std::map<uint32_t, vku_sem_t *> draw_sems;
    // std::map<uint32_t, vku_fence_t *> fences;
    double start_time = get_time_ms();
   
    DBG("Starting main loop"); 
    while (!glfwWindowShouldClose(inst->window)) {
        if (glfwGetKey(inst->window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            break;
        glfwPollEvents();

        try {
            uint32_t img_idx;
            vku_aquire_next_img(swc, img_sem, &img_idx);

            float curr_time = ((double)get_time_ms() - start_time)/100000.;
            curr_time *= 100;
            mvp.model = glm::rotate(glm::mat4(1.0f), curr_time * glm::radians(90.0f),
                    glm::vec3(0.0f, 0.0f, 1.0f));
            mvp.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f),
                    glm::vec3(0.0f, 0.0f, 1.0f));
            mvp.proj = glm::perspective(glm::radians(45.0f),
                    swc->vk_extent.width / (float)swc->vk_extent.height, 0.1f, 10.0f);
            mvp.proj[1][1] *= -1;
            memcpy(mvp_pbuff, &mvp, sizeof(mvp));

            cbuff->begin(0);
            cbuff->begin_rpass(fbs, img_idx);
            cbuff->bind_vert_buffs(0, {{vbuff, 0}});
            cbuff->bind_idx_buff(ibuff, 0, VK_INDEX_TYPE_UINT16);
            cbuff->bind_desc_set(VK_PIPELINE_BIND_POINT_GRAPHICS, pl->vk_layout, desc_set);
            cbuff->draw_idx(pl, indices.size());
            cbuff->end_rpass();
            cbuff->end();

            vku_submit_cmdbuff({{img_sem, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT}},
                    cbuff, fence, {draw_sem});
            vku_present(swc, {draw_sem}, img_idx);

            vku_wait_fences({fence});
            vku_reset_fences({fence});
        }
        catch (vku_err_t &e) {
            /* TODO: fix this (next time write what's wrong with it) */
            if (e.vk_err == VK_SUBOPTIMAL_KHR) {
                vk_device_wait_idle(dev->vk_dev);

                delete swc;
                swc = new vku_swapchain_t(dev);
                rp = new vku_renderpass_t(swc);
                pl = new vku_pipeline_t(
                    opts,
                    rp,
                    {sh_vert, sh_frag},
                    VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
                    vku_vertex3d_t::get_input_desc(),
                    bindings
                );
                fbs = new vku_framebuffs_t(rp);
            }
            else
                throw e;
        }
    }

    delete inst;
    return 0;
}