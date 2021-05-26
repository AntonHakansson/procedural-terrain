#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include "gpu.h"

using namespace glm;

class DebugDrawer {
public:
	static DebugDrawer* instance()
	{
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
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (GLvoid*)(3 * sizeof(GLfloat)));
		glBindVertexArray(0);

		glBindVertexArray(VAO);
		glDrawArrays(GL_LINES, 0, 2);
		glBindVertexArray(0);
	}

	void calcPerspectiveFrustumCorners(mat4 view_matrix, mat4 proj_matrix, vec4* frustum_corners) {
		mat4 view_inverse = inverse(view_matrix);

		float near = proj_matrix[3][2] / (proj_matrix[2][2] - 1);
		float far  = proj_matrix[3][2] / (proj_matrix[2][2] + 1);

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

	void calcOrthographicFrustumCorners(mat4 view_matrix, mat4 proj_matrix, vec4* frustum_corners) {
		mat4 view_inverse = inverse(view_matrix);

		float near   =  (1 + proj_matrix[2][3]) / proj_matrix[2][2];
		float far    = -(1 - proj_matrix[2][3]) / proj_matrix[2][2];
		float bottom =  (1 - proj_matrix[1][3]) / proj_matrix[1][1];
		float top    = -(1 + proj_matrix[1][3]) / proj_matrix[1][1];
		float left   = -(1 + proj_matrix[0][3]) / proj_matrix[0][0];
		float right  =  (1 - proj_matrix[0][3]) / proj_matrix[0][0];

		// near face
		frustum_corners[0] = view_inverse * vec4(right, top,    -near, 1.0);
		frustum_corners[1] = view_inverse * vec4(left,  top,    -near, 1.0);
		frustum_corners[2] = view_inverse * vec4(right, bottom, -near, 1.0);
		frustum_corners[3] = view_inverse * vec4(left,  bottom, -near, 1.0);

		// far face
		frustum_corners[4] = view_inverse * vec4(right, top,    -far, 1.0);
		frustum_corners[5] = view_inverse * vec4(left,  top,    -far, 1.0);
		frustum_corners[6] = view_inverse * vec4(right, bottom, -far, 1.0);
		frustum_corners[7] = view_inverse * vec4(left,  bottom, -far, 1.0);
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

	void drawOrthographicFrustum(const mat4& view_matrix, const mat4& proj_matrix, const vec3& color) {
		glUseProgram(debug_program);

    vec4 fcorners[8];
    calcOrthographicFrustumCorners(view_matrix, proj_matrix, fcorners);

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

	GLuint VBO, VAO;
    GLuint debug_program;
};