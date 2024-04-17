#define LOGGER_VERBOSE_LVL 0

#include "vulkan_utils.h"
#include "debug.h"
#include "misc_utils.h"
#include "time_utils.h"

#include <queue>

static auto create_vbuff(auto dev, auto cp, const std::vector<vku_vertex2d_t>& vertices) {
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

int main(int argc, char const *argv[])
{
    DBG_SCOPE();

    std::vector<vku_vertex2d_t> vertices;

    glm::vec3 colors[] = {
        glm::vec3(1, 1, 1), /* white */
        glm::vec3(1, 0, 0), /* red */
        glm::vec3(0, 1, 0), /* green */
        glm::vec3(0, 0, 1), /* blue */
        glm::vec3(1, 0, 1), /* magenta */
        glm::vec3(1, 1, 0), /* yellow */
        glm::vec3(0, 1, 1), /* cyan */
    };

    auto add_line = [&](glm::vec2 a, glm::vec2 b, int color) {
        vertices.push_back(vku_vertex2d_t{ .pos = a, .color = colors[color] });
        vertices.push_back(vku_vertex2d_t{ .pos = b, .color = colors[color] });
    };

    float ang_deg = 20;
    float ang_rad = ang_deg / 180. * 3.141592653589;
    float side = 0.25;
    int iter_cnt = 360 / ang_deg;
    int rec_cnt = 4;

    std::vector<glm::vec2> visited;
    std::queue<std::pair<glm::vec2, int>> points;
    points.push({glm::vec2(0, 0), rec_cnt});
    while (points.size()) {
        auto [origin, level] = points.front();
        visited.push_back(origin);
        points.pop();

        for (int i = 0; i < iter_cnt; i++) {
            glm::vec2 new_point = origin + glm::vec2(cos(i * ang_rad), sin(i * ang_rad)) * side;
    
            add_line(origin, new_point, rec_cnt - level);

            bool was_visited = false;
            for (auto &p : visited)
                if (glm::length(p - new_point) < 0.00001) {
                    was_visited = true;
                    break;
                }
            if (level - 1 >= 0 && !was_visited)
                points.push({new_point, level - 1});
        }
    }

    vku_opts_t opts;
    auto inst = new vku_instance_t(opts);

    auto vert = vku_spirv_compile(inst, VKU_SPIRV_VERTEX, R"___(
        #version 450

        layout(location = 0) in vec2 in_pos;    // those are referenced by
        layout(location = 1) in vec3 in_color;
        layout(location = 2) in vec2 in_tex;

        layout(location = 0) out vec3 out_color;
        layout(location = 1) out vec2 out_tex_coord;

        void main() {
            gl_Position = vec4(in_pos, 0.0, 1.0);
            gl_PointSize = 3.0;
            out_color = in_color;
            out_tex_coord = in_tex;
        }

    )___");

    auto frag = vku_spirv_compile(inst, VKU_SPIRV_FRAGMENT, R"___(
        #version 450

        layout(location = 0) in vec3 in_color;      // this is referenced by the vert shader
        layout(location = 1) in vec2 in_tex_coord;  // this is referenced by the vert shader

        layout(location = 0) out vec4 out_color;

        void main() {
            out_color = vec4(in_color, 1.0);
        }
    )___");

    auto surf =     new vku_surface_t(inst);
    auto dev =      new vku_device_t(surf);
    auto cp =       new vku_cmdpool_t(dev);

    vku_binding_desc_t bindings = {
        .binds = {},
    };

    auto sh_vert =  new vku_shader_t(dev, vert);
    auto sh_frag =  new vku_shader_t(dev, frag);
    auto swc =      new vku_swapchain_t(dev);
    auto rp =       new vku_renderpass_t(swc);
    auto pl =       new vku_pipeline_t(
        opts,
        rp,
        {sh_vert, sh_frag},
        VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
        vku_vertex2d_t::get_input_desc(),
        bindings
    );
    auto fbs =      new vku_framebuffs_t(rp);

    auto img_sem =  new vku_sem_t(dev);
    auto draw_sem = new vku_sem_t(dev);
    auto fence =    new vku_fence_t(dev);

    auto cbuff =    new vku_cmdbuff_t(cp);

    auto vbuff = create_vbuff(dev, cp, vertices);

    double start_time = get_time_ms();
   
    DBG("Starting main loop"); 
    while (!glfwWindowShouldClose(inst->window)) {
        if (glfwGetKey(inst->window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            break;
        glfwPollEvents();

        try {
            uint32_t img_idx;
            vku_aquire_next_img(swc, img_sem, &img_idx);

            cbuff->begin(0);
            cbuff->begin_rpass(fbs, img_idx);
            cbuff->bind_vert_buffs(0, {{vbuff, 0}});
            cbuff->draw(pl, vertices.size());
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
                    VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
                    vku_vertex2d_t::get_input_desc(),
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