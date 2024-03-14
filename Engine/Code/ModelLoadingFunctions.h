#ifndef BUFFER_MANAGER_FUNC
#define BUFFER_MANAGER_FUNC

#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "Globals.h"

namespace ModelLoader {
	struct VertexBufferAttribute {
		u8 location;
		u8 componentCount;
		u8 offset;
	};

	struct VertexBufferLayout
	{
		std::vector<VertexBufferAttribute> attributes;
		u8 stride;
	};

	struct VertexShaderAttribute
	{
		u8 location;
		u8 componentCount;
	};
	struct VertexShaderLayout {

		std::vector<VertexShaderAttribute> attributes;
	};

	struct VAO
	{
		GLuint handle;
		GLuint programHandle;
	};
}

#endif