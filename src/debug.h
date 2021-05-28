#pragma once

#include <imgui.h>
#include <imgui_internal.h>

#include <ImGuizmo.h>
#include <glad/glad.h>

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>

#include "gpu.h"
#include "camera.h"

using namespace glm;

class DebugDrawer {
public:
  static DebugDrawer* instance() {
    static DebugDrawer* drawer;
    if (drawer == nullptr) {
      drawer = new DebugDrawer;
    }

    return drawer;
  }

  void loadShaders(bool is_reload) {
    GLuint shader = gpu::loadShaderProgram("resources/shaders/debug.vert",
                                           "resources/shaders/debug.frag", is_reload);
    if (shader != 0) debug_program = shader;
  }

  void setCamera(mat4 view_matrix, mat4 proj_matrix) {
    glUseProgram(debug_program);

    gpu::setUniformSlow(debug_program, "projection", proj_matrix);
    gpu::setUniformSlow(debug_program, "view", view_matrix);
  }

  void drawLine(const vec3& from, const vec3& to, const vec3& color) {
    glUseProgram(debug_program);

    // Vertex data
    GLfloat points[12];

    points[0] = from.x;
    points[1] = from.y;
    points[2] = from.z;
    points[3] = color.x;
    points[4] = color.y;
    points[5] = color.z;

    points[6] = to.x;
    points[7] = to.y;
    points[8] = to.z;
    points[9] = color.x;
    points[10] = color.y;
    points[11] = color.z;

    glDeleteBuffers(1, &VBO);
    glDeleteVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(points), &points, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), 0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat),
                          (GLvoid*)(3 * sizeof(GLfloat)));
    glBindVertexArray(0);

    glBindVertexArray(VAO);
    glDrawArrays(GL_LINES, 0, 2);
    glBindVertexArray(0);
  }

  void calcPerspectiveFrustumCorners(mat4 view_matrix, mat4 proj_matrix, vec4* frustum_corners) {
    mat4 view_inverse = inverse(view_matrix);

    float near = proj_matrix[3][2] / (proj_matrix[2][2] - 1);
    float far = proj_matrix[3][2] / (proj_matrix[2][2] + 1);

    float fovy = 2.0 * atan(1.0 / proj_matrix[1][1]);
    float ar = proj_matrix[0][0] / proj_matrix[1][1];

    GLfloat tanHalfHFov = tan(fovy / 2.0) / ar;
    GLfloat tanHalfVFov = tan(fovy / 2.0);

    float xn = near * tanHalfHFov;
    float xf = far * tanHalfHFov;
    float yn = near * tanHalfVFov;
    float yf = far * tanHalfVFov;

    // near face
    frustum_corners[0] = view_inverse * vec4(xn, yn, -near, 1.0);
    frustum_corners[1] = view_inverse * vec4(-xn, yn, -near, 1.0);
    frustum_corners[2] = view_inverse * vec4(xn, -yn, -near, 1.0);
    frustum_corners[3] = view_inverse * vec4(-xn, -yn, -near, 1.0);

    // far face
    frustum_corners[4] = view_inverse * vec4(xf, yf, -far, 1.0);
    frustum_corners[5] = view_inverse * vec4(-xf, yf, -far, 1.0);
    frustum_corners[6] = view_inverse * vec4(xf, -yf, -far, 1.0);
    frustum_corners[7] = view_inverse * vec4(-xf, -yf, -far, 1.0);
  }

  void calcOrthographicFrustumCorners(mat4 view_matrix, OrthoProjInfo ortho_info, vec4* frustum_corners) {
    mat4 view_inverse = inverse(view_matrix);

    // near face
    frustum_corners[0] = view_inverse * vec4(ortho_info.r, ortho_info.t, -ortho_info.n, 1.0);
    frustum_corners[1] = view_inverse * vec4(ortho_info.l, ortho_info.t, -ortho_info.n, 1.0);
    frustum_corners[2] = view_inverse * vec4(ortho_info.r, ortho_info.b, -ortho_info.n, 1.0);
    frustum_corners[3] = view_inverse * vec4(ortho_info.l, ortho_info.b, -ortho_info.n, 1.0);

    // far face
    frustum_corners[4] = view_inverse * vec4(ortho_info.r, ortho_info.t, -ortho_info.f, 1.0);
    frustum_corners[5] = view_inverse * vec4(ortho_info.l, ortho_info.t, -ortho_info.f, 1.0);
    frustum_corners[6] = view_inverse * vec4(ortho_info.r, ortho_info.b, -ortho_info.f, 1.0);
    frustum_corners[7] = view_inverse * vec4(ortho_info.l, ortho_info.b, -ortho_info.f, 1.0);
  }

  void drawPerspectiveFrustum(const mat4& view_matrix, const mat4& proj_matrix, const vec3& color) {
    glUseProgram(debug_program);

    vec4 fcorners[8];
    calcPerspectiveFrustumCorners(view_matrix, proj_matrix, fcorners);

    DebugDrawer::instance()->drawLine(vec3(fcorners[0]), vec3(fcorners[4]), color);
    DebugDrawer::instance()->drawLine(vec3(fcorners[1]), vec3(fcorners[5]), color);
    DebugDrawer::instance()->drawLine(vec3(fcorners[2]), vec3(fcorners[6]), color);
    DebugDrawer::instance()->drawLine(vec3(fcorners[3]), vec3(fcorners[7]), color);

    DebugDrawer::instance()->drawLine(vec3(fcorners[0]), vec3(fcorners[1]), color);
    DebugDrawer::instance()->drawLine(vec3(fcorners[1]), vec3(fcorners[3]), color);
    DebugDrawer::instance()->drawLine(vec3(fcorners[2]), vec3(fcorners[3]), color);
    DebugDrawer::instance()->drawLine(vec3(fcorners[2]), vec3(fcorners[0]), color);

    DebugDrawer::instance()->drawLine(vec3(fcorners[4]), vec3(fcorners[5]), color);
    DebugDrawer::instance()->drawLine(vec3(fcorners[5]), vec3(fcorners[7]), color);
    DebugDrawer::instance()->drawLine(vec3(fcorners[6]), vec3(fcorners[7]), color);
    DebugDrawer::instance()->drawLine(vec3(fcorners[6]), vec3(fcorners[4]), color);
  }

  void drawOrthographicFrustum(const mat4& view_matrix, const OrthoProjInfo& ortho_info,
                               const vec3& color) {
    glUseProgram(debug_program);

    vec4 fcorners[8];
    calcOrthographicFrustumCorners(view_matrix, ortho_info, fcorners);

    DebugDrawer::instance()->drawLine(vec3(fcorners[0]), vec3(fcorners[4]), color);
    DebugDrawer::instance()->drawLine(vec3(fcorners[1]), vec3(fcorners[5]), color);
    DebugDrawer::instance()->drawLine(vec3(fcorners[2]), vec3(fcorners[6]), color);
    DebugDrawer::instance()->drawLine(vec3(fcorners[3]), vec3(fcorners[7]), color);

    DebugDrawer::instance()->drawLine(vec3(fcorners[0]), vec3(fcorners[1]), color);
    DebugDrawer::instance()->drawLine(vec3(fcorners[1]), vec3(fcorners[3]), color);
    DebugDrawer::instance()->drawLine(vec3(fcorners[2]), vec3(fcorners[3]), color);
    DebugDrawer::instance()->drawLine(vec3(fcorners[2]), vec3(fcorners[0]), color);

    DebugDrawer::instance()->drawLine(vec3(fcorners[4]), vec3(fcorners[5]), color);
    DebugDrawer::instance()->drawLine(vec3(fcorners[5]), vec3(fcorners[7]), color);
    DebugDrawer::instance()->drawLine(vec3(fcorners[6]), vec3(fcorners[7]), color);
    DebugDrawer::instance()->drawLine(vec3(fcorners[6]), vec3(fcorners[4]), color);
  }

  void beginGizmo(mat4 view_matrix, vec2 size, mat4& out_view_matrix, mat4& out_proj_matrix) {
    ImVec2 cursor_pos = ImGui::GetCursorScreenPos();

    ImVec2 box_max = ImVec2(cursor_pos.x + size.x, cursor_pos.y + size.y);

    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    ImRect bb(cursor_pos, ImVec2(cursor_pos.x + size.x, cursor_pos.y + size.y));
    ImGui::ItemSize(bb);
    if (!ImGui::ItemAdd(bb, 0)) return;

    draw_list->AddRect(cursor_pos, box_max, ImColor(100, 100, 100, 255));

    mat4 view_inverse = inverse(view_matrix);
    vec3 pos = vec3(view_inverse[3][0], view_inverse[3][1], view_inverse[3][2]);
    vec3 dir = vec3(view_inverse[2][0], view_inverse[2][1], view_inverse[2][2]);
    vec3 up = vec3(view_inverse[1][0], view_inverse[1][1], view_inverse[1][2]);

    float length = 20;
    const vec3 cam_target = pos - dir * length;

    const float distance = 3.f;
    float fov = acosf(distance / (sqrtf(distance * distance + 3.f)));

    vec3 eye = dir * distance;

    out_view_matrix = lookAt(eye, vec3(0), up);
    out_proj_matrix = perspective(fov / sqrtf(2.f), size.x / size.y, 0.1f, 10.f);

    ImGuizmo::SetRect(cursor_pos.x, cursor_pos.y, size.x, size.y);
    ImGuizmo::SetGizmoSizeClipSpace(0.7);
    // ImGui::PushClipRect(cursor_pos, box_max, true);
  }

  void endGizmo() {
    // ImGui::PopClipRect();
    ImGuizmo::SetGizmoSizeClipSpace(0.1);
  }

  GLuint VBO, VAO;
  GLuint debug_program;
};
