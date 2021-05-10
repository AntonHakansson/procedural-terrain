

#ifdef _WIN32
extern "C" _declspec(dllexport) unsigned int NvOptimusEnablement = 0x00000001;
#endif

#include <GL/glew.h>
#include <cmath>
#include <cstdlib>
#include <algorithm>
#include <chrono>

#include <labhelper.h>
#include <imgui.h>
#include <imgui_impl_sdl_gl3.h>

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
using namespace glm;

#include <Model.h>
#include "hdr.h"
#include "fbo.h"

#define FNL_IMPL
#include "FastNoiseLite.h"
#undef FNL_IMPL

#include "terrain.h"
#include "ParticleSystem.h"

using std::min;
using std::max;

///////////////////////////////////////////////////////////////////////////////
// Various globals
///////////////////////////////////////////////////////////////////////////////
SDL_Window* g_window = nullptr;
float currentTime = 0.0f;
float previousTime = 0.0f;
float deltaTime = 0.0f;
bool showUI = false;
int windowWidth, windowHeight;

// Mouse input
ivec2 g_prevMouseCoords = { -1, -1 };
bool g_isMouseDragging = false;

ParticleSystem *particle_system;

///////////////////////////////////////////////////////////////////////////////
// Shader programs
///////////////////////////////////////////////////////////////////////////////
GLuint shaderProgram;       // Shader for rendering the final image
GLuint simpleShaderProgram; // Shader used to draw the shadow map
GLuint backgroundProgram;

///////////////////////////////////////////////////////////////////////////////
// Environment
///////////////////////////////////////////////////////////////////////////////
float environment_multiplier = 1.5f;
GLuint environmentMap, irradianceMap, reflectionMap;
const std::string envmap_base_name = "001";

///////////////////////////////////////////////////////////////////////////////
// Light source
///////////////////////////////////////////////////////////////////////////////
vec3 lightPosition;
vec3 point_light_color = vec3(1.f, 1.f, 1.f);
float point_light_intensity_multiplier = 10000.0f;





///////////////////////////////////////////////////////////////////////////////
// Camera parameters.
///////////////////////////////////////////////////////////////////////////////
vec3 cameraPosition(-70.0f, 50.0f, 70.0f);
vec3 cameraDirection = normalize(vec3(0.0f) - cameraPosition);
float cameraSpeed = 60.f;

vec3 worldUp(0.0f, 1.0f, 0.0f);

///////////////////////////////////////////////////////////////////////////////
// Models
///////////////////////////////////////////////////////////////////////////////
labhelper::Model* fighterModel = nullptr;
labhelper::Model* landingpadModel = nullptr;
labhelper::Model* sphereModel = nullptr;

mat4 roomModelMatrix;
mat4 landingPadModelMatrix;
mat4 fighterModelMatrix;

struct FighterState {
	vec3 pos{vec3(0, 15.0, 0)};
	float rotation = 0; // rotation in radians around worldUp axis
	float rotationVel = 0;

	float movingAcc = 30.0f;
	float rotateAcc = M_PI * 0.9f;

	int exhaustAmount = 10;
	float exhaustSpeed = 10.f;
	vec3 exhaustOffset = vec3(18.f, 3.f, 0.f); // offset in modelspace to exhaust

	vec3 vel;

	mat4 R, T;
};

FighterState fighterState;

Terrain terrain;


void loadShaders(bool is_reload)
{
	GLuint shader = labhelper::loadShaderProgram("../project/simple.vert", "../project/simple.frag",
	                                             is_reload);
	if(shader != 0)
		simpleShaderProgram = shader;
	shader = labhelper::loadShaderProgram("../project/background.vert", "../project/background.frag",
	                                      is_reload);
	if(shader != 0)
		backgroundProgram = shader;
	shader = labhelper::loadShaderProgram("../project/shading.vert", "../project/shading.frag", is_reload);
	if(shader != 0)
		shaderProgram = shader;
}

void initGL()
{
	///////////////////////////////////////////////////////////////////////
	//		Load Shaders
	///////////////////////////////////////////////////////////////////////
	backgroundProgram = labhelper::loadShaderProgram("../project/background.vert",
	                                                 "../project/background.frag");
	shaderProgram = labhelper::loadShaderProgram("../project/shading.vert", "../project/shading.frag");
	simpleShaderProgram = labhelper::loadShaderProgram("../project/simple.vert", "../project/simple.frag");

	///////////////////////////////////////////////////////////////////////
	// Load models and set up model matrices
	///////////////////////////////////////////////////////////////////////
	fighterModel = labhelper::loadModelFromOBJ("../scenes/NewShip.obj");
	landingpadModel = labhelper::loadModelFromOBJ("../scenes/landingpad.obj");
	sphereModel = labhelper::loadModelFromOBJ("../scenes/sphere.obj");

	roomModelMatrix = mat4(1.0f);
	landingPadModelMatrix = mat4(1.0f);


	///////////////////////////////////////////////////////////////////////
	// Load environment map
	///////////////////////////////////////////////////////////////////////
	const int roughnesses = 8;
	std::vector<std::string> filenames;
	for(int i = 0; i < roughnesses; i++)
		filenames.push_back("../scenes/envmaps/" + envmap_base_name + "_dl_" + std::to_string(i) + ".hdr");

	reflectionMap = labhelper::loadHdrMipmapTexture(filenames);
	environmentMap = labhelper::loadHdrTexture("../scenes/envmaps/" + envmap_base_name + ".hdr");
	irradianceMap = labhelper::loadHdrTexture("../scenes/envmaps/" + envmap_base_name + "_irradiance.hdr");


	glEnable(GL_DEPTH_TEST); // enable Z-buffering
	glEnable(GL_CULL_FACE);  // enables backface culling

	// init terrain
	terrain.init();

	// init particle system
	particle_system = new ParticleSystem(100000);
}

void debugDrawLight(const glm::mat4& viewMatrix,
                    const glm::mat4& projectionMatrix,
                    const glm::vec3& worldSpaceLightPos)
{
	// mat4 modelMatrix = glm::translate(worldSpaceLightPos);
	mat4 modelMatrix = glm::translate(glm::vec3(0, 0, 0)) * glm::scale(glm::vec3(10));
	glUseProgram(shaderProgram);
	labhelper::setUniformSlow(shaderProgram, "modelViewProjectionMatrix",
	                          projectionMatrix * viewMatrix * modelMatrix);
	// labhelper::render(sphereModel);

	labhelper::setUniformSlow(shaderProgram, "modelViewProjectionMatrix", projectionMatrix * viewMatrix * modelMatrix);
	labhelper::setUniformSlow(shaderProgram, "modelViewMatrix", viewMatrix * modelMatrix);
	labhelper::setUniformSlow(shaderProgram, "normalMatrix", inverse(transpose(viewMatrix * modelMatrix)));

	labhelper::render(fighterModel);
}


void drawBackground(const mat4& viewMatrix, const mat4& projectionMatrix)
{
	glUseProgram(backgroundProgram);
	labhelper::setUniformSlow(backgroundProgram, "environment_multiplier", environment_multiplier);
	labhelper::setUniformSlow(backgroundProgram, "inv_PV", inverse(projectionMatrix * viewMatrix));
	labhelper::setUniformSlow(backgroundProgram, "camera_pos", cameraPosition);
	labhelper::drawFullScreenQuad();
}

void drawScene(GLuint currentShaderProgram,
               const mat4& viewMatrix,
               const mat4& projectionMatrix,
               const mat4& lightViewMatrix,
               const mat4& lightProjectionMatrix)
{
	glUseProgram(currentShaderProgram);
	// Light source
	vec4 viewSpaceLightPosition = viewMatrix * vec4(lightPosition, 1.0f);
	labhelper::setUniformSlow(currentShaderProgram, "point_light_color", point_light_color);
	labhelper::setUniformSlow(currentShaderProgram, "point_light_intensity_multiplier",
	                          point_light_intensity_multiplier);
	labhelper::setUniformSlow(currentShaderProgram, "viewSpaceLightPosition", vec3(viewSpaceLightPosition));
	labhelper::setUniformSlow(currentShaderProgram, "viewSpaceLightDir",
	                          normalize(vec3(viewMatrix * vec4(-lightPosition, 0.0f))));


	// Environment
	labhelper::setUniformSlow(currentShaderProgram, "environment_multiplier", environment_multiplier);

	// camera
	labhelper::setUniformSlow(currentShaderProgram, "viewInverse", inverse(viewMatrix));

	// Terrain
	terrain.render(projectionMatrix, viewMatrix, cameraPosition);

	// landing pad
	// labhelper::setUniformSlow(currentShaderProgram, "modelViewProjectionMatrix",
	//                           projectionMatrix * viewMatrix * landingPadModelMatrix);
	// labhelper::setUniformSlow(currentShaderProgram, "modelViewMatrix", viewMatrix * landingPadModelMatrix);
	// labhelper::setUniformSlow(currentShaderProgram, "normalMatrix",
	//                           inverse(transpose(viewMatrix * landingPadModelMatrix)));

	// labhelper::render(landingpadModel);

	// Fighter
	// labhelper::setUniformSlow(currentShaderProgram, "modelViewProjectionMatrix",
	//                           projectionMatrix * viewMatrix * fighterModelMatrix);
	// labhelper::setUniformSlow(currentShaderProgram, "modelViewMatrix", viewMatrix * fighterModelMatrix);
	// labhelper::setUniformSlow(currentShaderProgram, "normalMatrix",
	//                           inverse(transpose(viewMatrix * fighterModelMatrix)));

	// labhelper::render(fighterModel);
}


void display(void)
{
	///////////////////////////////////////////////////////////////////////////
	// Check if window size has changed and resize buffers as needed
	///////////////////////////////////////////////////////////////////////////
	{
		int w, h;
		SDL_GetWindowSize(g_window, &w, &h);
		if(w != windowWidth || h != windowHeight)
		{
			windowWidth = w;
			windowHeight = h;
		}
	}

	///////////////////////////////////////////////////////////////////////////
	// setup matrices
	///////////////////////////////////////////////////////////////////////////
	mat4 projMatrix = perspective(radians(45.0f), float(windowWidth) / float(windowHeight), 5.0f, 2000.0f);
	mat4 viewMatrix = lookAt(cameraPosition, cameraPosition + cameraDirection, worldUp);

	vec4 lightStartPosition = vec4(40.0f, 40.0f, 0.0f, 1.0f);
	lightPosition = vec3(rotate(currentTime, worldUp) * lightStartPosition);
	mat4 lightViewMatrix = lookAt(lightPosition, vec3(0.0f), worldUp);
	mat4 lightProjMatrix = perspective(radians(45.0f), 1.0f, 25.0f, 100.0f);

	///////////////////////////////////////////////////////////////////////////
	// Bind the environment map(s) to unused texture units
	///////////////////////////////////////////////////////////////////////////
	glActiveTexture(GL_TEXTURE6);
	glBindTexture(GL_TEXTURE_2D, environmentMap);
	glActiveTexture(GL_TEXTURE7);
	glBindTexture(GL_TEXTURE_2D, irradianceMap);
	glActiveTexture(GL_TEXTURE8);
	glBindTexture(GL_TEXTURE_2D, reflectionMap);
	glActiveTexture(GL_TEXTURE0);



	///////////////////////////////////////////////////////////////////////////
	// Draw from camera
	///////////////////////////////////////////////////////////////////////////
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, windowWidth, windowHeight);
	glClearColor(0.2f, 0.2f, 0.8f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	drawBackground(viewMatrix, projMatrix);
	drawScene(shaderProgram, viewMatrix, projMatrix, lightViewMatrix, lightProjMatrix);
	particle_system->draw_particles(windowWidth, windowHeight, viewMatrix, projMatrix);
	debugDrawLight(viewMatrix, projMatrix, vec3(lightPosition));
}

bool handleEvents(void)
{
	// check events (keyboard among other)
	SDL_Event event;
	bool quitEvent = false;

	while(SDL_PollEvent(&event))
	{
		if(event.type == SDL_QUIT || (event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_ESCAPE))
		{
			quitEvent = true;
		}
		if(event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_g)
		{
			showUI = !showUI;
		}
		if(event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT
		   && (!showUI || !ImGui::GetIO().WantCaptureMouse))
		{
			g_isMouseDragging = true;
			int x;
			int y;
			SDL_GetMouseState(&x, &y);
			g_prevMouseCoords.x = x;
			g_prevMouseCoords.y = y;
		}

		if(!(SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(SDL_BUTTON_LEFT)))
		{
			g_isMouseDragging = false;
		}

		if(event.type == SDL_MOUSEMOTION && g_isMouseDragging)
		{
			// More info at https://wiki.libsdl.org/SDL_MouseMotionEvent
			int delta_x = event.motion.x - g_prevMouseCoords.x;
			int delta_y = event.motion.y - g_prevMouseCoords.y;
			float rotationSpeed = 0.1f;
			mat4 yaw = rotate(rotationSpeed * deltaTime * -delta_x, worldUp);
			mat4 pitch = rotate(rotationSpeed * deltaTime * -delta_y,
			                    normalize(cross(cameraDirection, worldUp)));
			cameraDirection = vec3(pitch * yaw * vec4(cameraDirection, 0.0f));
			g_prevMouseCoords.x = event.motion.x;
			g_prevMouseCoords.y = event.motion.y;
		}
	}

	// check keyboard state (which keys are still pressed)
	const uint8_t* state = SDL_GetKeyboardState(nullptr);
	vec3 cameraRight = cross(cameraDirection, worldUp);

	if(state[SDL_SCANCODE_W])
	{
		cameraPosition += cameraSpeed * deltaTime * cameraDirection;
	}
	if(state[SDL_SCANCODE_S])
	{
		cameraPosition -= cameraSpeed * deltaTime * cameraDirection;
	}
	if(state[SDL_SCANCODE_A])
	{
		cameraPosition -= cameraSpeed * deltaTime * cameraRight;
	}
	if(state[SDL_SCANCODE_D])
	{
		cameraPosition += cameraSpeed * deltaTime * cameraRight;
	}
	if(state[SDL_SCANCODE_Q])
	{
		cameraPosition -= cameraSpeed * deltaTime * worldUp;
	}
	if(state[SDL_SCANCODE_E])
	{
		cameraPosition += cameraSpeed * deltaTime * worldUp;
	}

	//
	// Aircraft controls
	//
	const vec3 fighterForward = -normalize(vec3(fighterState.R[0]));
	if(state[SDL_SCANCODE_I])
	{
		fighterState.vel += fighterState.movingAcc * fighterForward * deltaTime;
	}
	else {
		// attenuate velocity
		fighterState.vel *= 0.94f;
	}
	if(state[SDL_SCANCODE_J])
	{
		fighterState.rotationVel += fighterState.rotateAcc * deltaTime;
	}
	else if(state[SDL_SCANCODE_L])
	{
		fighterState.rotationVel -= fighterState.rotateAcc * deltaTime;
	}
	else {
		// attenuate rotation vel
		fighterState.rotationVel *= 0.94;
	}

	//
	// Spawn particles
	//
	if (state[SDL_SCANCODE_I]) {
		for (int i = 0; i < fighterState.exhaustAmount; i++) {
			float theta			= labhelper::uniform_randf(0.f, 2.f * M_PI);
			const float u		= labhelper::uniform_randf(0.95f, 1.f);
			vec3 cone_sample	= glm::vec3(u, sqrt(1.f - u * u) * cosf(theta), sqrt(1.f - u * u) * sinf(theta));

			vec3 pos = fighterState.pos + vec3(fighterState.R * vec4(fighterState.exhaustOffset, 1));
			vec3 vel = cone_sample * fighterState.exhaustSpeed;

			auto p = Particle{
				.life_length = 5.f,
				.velocity = vec3(fighterState.R * vec4(vel, 0)),
				.pos = pos,
			};
			particle_system->spawn(p);
		}
	}

	fighterState.pos += fighterState.vel * deltaTime;
	fighterState.rotation += fighterState.rotationVel * deltaTime;

	fighterState.R = rotate(fighterState.rotation, worldUp);
	fighterState.T = translate(fighterState.pos);
	fighterModelMatrix = fighterState.T * fighterState.R;

	return quitEvent;
}

void gui()
{
	// Inform imgui of new frame
	ImGui_ImplSdlGL3_NewFrame(g_window);

	// ----------------- Set variables --------------------------
	ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate,
	            ImGui::GetIO().Framerate);
	// ----------------------------------------------------------

	if (ImGui::CollapsingHeader("Fighter State")) {
        ImGui::Text("Movement");
		ImGui::SliderFloat("Move Acceleration", &fighterState.movingAcc, 1.0f, 50.f);
		ImGui::SliderFloat("Rotate Acceleration", &fighterState.rotateAcc, 0.1f, M_PI * 2.f);
		ImGui::Separator();

        ImGui::Text("Exhaustion");
		ImGui::SliderInt("Exhaust Amount", &fighterState.exhaustAmount, 0, 30);
		ImGui::SliderFloat("Exhaust Speed", &fighterState.exhaustSpeed, 0, 30.0f);
		ImGui::SliderFloat3("Exhaust Offset", (float *)&fighterState.exhaustOffset[0], -20.f, 20.f);
		ImGui::Separator();
	}

	///////////////////////////////////////////////////////////////////////////
	// Light and environment map
	///////////////////////////////////////////////////////////////////////////
	if(ImGui::CollapsingHeader("Light sources", "lights_ch", true, true))
	{
		ImGui::SliderFloat("Environment multiplier", &environment_multiplier, 0.0f, 10.0f);
		ImGui::ColorEdit3("Point light color", &point_light_color.x);
		ImGui::SliderFloat("Point light intensity multiplier", &point_light_intensity_multiplier, 0.0f,
		                   10000.0f, "%.3f", 2.f);
	}

	///////////////////////////////////////////////////////////////////////////
	// A button for reloading the shaders
	///////////////////////////////////////////////////////////////////////////
	if(ImGui::Button("Reload Shaders"))
	{
		loadShaders(true);
		terrain.loadShader(true);
	}

	terrain.draw_imgui(g_window);

	particle_system->draw_imgui(g_window);


	// Render the GUI.
	ImGui::Render();
}

int main(int argc, char* argv[])
{
	g_window = labhelper::init_window_SDL("OpenGL Project");

	initGL();

	bool stopRendering = false;
	auto startTime = std::chrono::system_clock::now();

	while(!stopRendering)
	{
		//update currentTime
		std::chrono::duration<float> timeSinceStart = std::chrono::system_clock::now() - startTime;
		previousTime = currentTime;
		currentTime = timeSinceStart.count();
		deltaTime = currentTime - previousTime;

		// update state
		particle_system->process_particles(deltaTime);

		// render to window
		display();

		// Render overlay GUI.
		if(showUI)
		{
			gui();
		}

		// Swap front and back buffer. This frame will now been displayed.
		SDL_GL_SwapWindow(g_window);

		// check events (keyboard among other)
		stopRendering = handleEvents();
	}
	// Free Models
	labhelper::freeModel(fighterModel);
	labhelper::freeModel(landingpadModel);
	labhelper::freeModel(sphereModel);

	// Free particle system
	delete particle_system;

	// Shut down everything. This includes the window and all other subsystems.
	labhelper::shutDown(g_window);
	return 0;
}
