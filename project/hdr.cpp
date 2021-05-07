#include "hdr.h"
#include <iostream>
#include <stb_image.h>

namespace labhelper
{
struct HDRImage
{
	int width, height, components;
	float* data = nullptr;
	// Constructor
	HDRImage(const std::string& filename)
	{
		stbi_set_flip_vertically_on_load(false);
		data = stbi_loadf(filename.c_str(), &width, &height, &components, 3);
		stbi_set_flip_vertically_on_load(true);
		if(data == nullptr)
		{
			std::cout << "Failed to load image: " << filename << ".\n";
			exit(1);
		}
	};
	// Destructor
	~HDRImage()
	{
		stbi_image_free(data);
	};
};

GLuint loadHdrTexture(const std::string& filename)
{
	GLuint texId;
	glGenTextures(1, &texId);
	glBindTexture(GL_TEXTURE_2D, texId);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	HDRImage image(filename);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, image.width, image.height, 0, GL_RGB, GL_FLOAT, image.data);

	return texId;
}

GLuint loadHdrMipmapTexture(const std::vector<std::string>& filenames)
{
	GLuint texId;
	glGenTextures(1, &texId);
	glBindTexture(GL_TEXTURE_2D, texId);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

	for(int i = 0; i < filenames.size(); i++)
	{
		HDRImage image(filenames[i]);
		glTexImage2D(GL_TEXTURE_2D, i, GL_RGB32F, image.width, image.height, 0, GL_RGB, GL_FLOAT, image.data);
		if(i == 0)
		{
			glGenerateMipmap(GL_TEXTURE_2D);
		}
	}

	return texId;
}
} // namespace labhelper
