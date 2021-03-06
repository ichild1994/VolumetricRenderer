#include "TextureSliceObject.hpp"


std::string TextureSliceObject::vertSrc= R"(
#version 330
//attribs
layout(location = 0) in vec4 pointPosition;
//transforms
uniform mat4 modelViewProjectionMatrix;
uniform mat4 modelMatrix;
uniform mat4 viewMatrix;
uniform mat4 projectionMatrix;
uniform mat4 invMVMatrix;
//outputs
out vec4 fragmentPosition;
//main
void main()
{
	//compute outputs
	fragmentPosition = vec4(pointPosition.x, pointPosition.y, pointPosition.z, 1.0f);
	gl_Position = modelViewProjectionMatrix * vec4(pointPosition.x, pointPosition.y, pointPosition.z, 1.0f);
}
)";
 
std::string TextureSliceObject::fragSrc = R"(
#version 330

//inputs
in vec4 fragmentPosition;

//uniforms

uniform sampler3D volumeTexture;
uniform vec3 texDim;
uniform float brightness;
uniform float contrast;
uniform float threshold; 
uniform sampler1D lutTexture;

//output
layout(location = 0) out vec4 outputColor; 

//main
void main()
{
	float hasp = texDim.x / texDim.y;
	float dasp = texDim.z / texDim.y;
	
	vec4 col = texture(volumeTexture, (fragmentPosition.xyz * vec3(1, 1, 1/dasp) + vec3(0.5f, 0.5f, 0.5f)));
	
	if(col.w <= 0.0001f)
		discard; 
	
	float c = clamp(contrast * col.r + brightness, 0.0f, 1.0f); 
		
	vec4 finalColor = c > threshold ? vec4(c, c, c, 1.0f) : vec4(0, 0, 0, 0);
	
  	outputColor = finalColor;
}
)";
 
int TextureSliceObject::programShaderObject;
int TextureSliceObject::vertexShaderObject;
int TextureSliceObject::fragmentShaderObject;
 
void TextureSliceObject::InitSystem()
{
	OPENGL_FUNC_MACRO
	
	//make array to pointer for source code (needed for opengl )
	const char* vsrc[1];
	const char* fsrc[1];
	vsrc[0] = vertSrc.c_str();
	fsrc[0] = fragSrc.c_str();
	
	//compile vertex and fragment shaders from source
	vertexShaderObject = ogl->glCreateShader(GL_VERTEX_SHADER);
	ogl->glShaderSource(vertexShaderObject, 1, vsrc, NULL);
	ogl->glCompileShader(vertexShaderObject);
	fragmentShaderObject = ogl->glCreateShader(GL_FRAGMENT_SHADER);
	ogl->glShaderSource(fragmentShaderObject, 1, fsrc, NULL);
	ogl->glCompileShader(fragmentShaderObject);
	
	//link vertex and fragment shader to create shader program object
	programShaderObject = ogl->glCreateProgram();
	ogl->glAttachShader(programShaderObject, vertexShaderObject);
	ogl->glAttachShader(programShaderObject, fragmentShaderObject);
	ogl->glLinkProgram(programShaderObject);
	
	//Check status of shader and log any compile time errors
	int linkStatus;
	ogl->glGetProgramiv(programShaderObject, GL_LINK_STATUS, &linkStatus);
	if (linkStatus != GL_TRUE) 
	{
		char log[5000];
		int logLen; 
		ogl->glGetProgramInfoLog(programShaderObject, 5000, &logLen, log);
		std::cerr << "TextureSliceObject:Could not link program: " << std::endl;
		std::cerr << log << std::endl;
		ogl->glGetShaderInfoLog(vertexShaderObject, 5000, &logLen, log);
		std::cerr << "vertex shader log:\n" << log << std::endl;
		ogl->glGetShaderInfoLog(fragmentShaderObject, 5000, &logLen, log);
		std::cerr << "fragment shader log:\n" << log << std::endl;
		ogl->glDeleteProgram(programShaderObject);
		programShaderObject = 0;
	}
	else
	{
		std::cout << "TextureSliceObject::CompileShader:compile success " << std::endl;
	}
}


TextureSliceObject::TextureSliceObject()
{
	brightness = 0;
	contrast = 1;
}


void TextureSliceObject::Init()
{
	//Init Buffers and VAO for rendering	
	OPENGL_FUNC_MACRO

	ogl->glGenBuffers(1, &vertexBuffer);
	ogl->glGenBuffers(1, &elementBuffer);
	ogl->glGenVertexArrays(1, &vertexArrayObject);
	
	//Bind VAO
	ogl->glBindVertexArray(vertexArrayObject);
	
	//build buffers
	std::vector<Vertex> vertexData(4); 
	
	Vertex v;
	v.w = 1.0;
	
	
	double extent = 0.5; 
	
	v.z = 0;
	v.x = -extent;
	v.y = -extent;
	vertexData[0] = v;
	v.x = extent;
	v.y = -extent;
	vertexData[1] = v;
	v.x = extent;
	v.y = extent;
	vertexData[2] = v;
	v.x = -extent;
	v.y = extent;
	vertexData[3] = v;

	
	std::vector<unsigned int> elementData(6); 
	
	elementData[0] = 0;
	elementData[1] = 1;
	elementData[2] = 2;
	elementData[3] = 2;
	elementData[4] = 3;
	elementData[5] = 0;

	
	ogl->glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
	ogl->glBufferData(GL_ARRAY_BUFFER, vertexData.size() * sizeof(Vertex), (char*)&vertexData[0], GL_STATIC_DRAW);

	ogl->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementBuffer);
	ogl->glBufferData(GL_ELEMENT_ARRAY_BUFFER, elementData.size() * sizeof(unsigned int), (char*)&elementData[0], GL_STATIC_DRAW);

	//set vertex attributes
	ogl->glEnableVertexAttribArray(0);
	ogl->glVertexAttribPointer(0, 4, GL_FLOAT, false, sizeof(Vertex), (void*)((uintptr_t)0));
	
	//Unbind VAO
	ogl->glBindVertexArray(0);
	
	//Unbind array and element buffers
	ogl->glBindBuffer(GL_ARRAY_BUFFER, 0);
	ogl->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	
	volumeTexture = NULL; 
	
	lutTexture = NULL; 
}


void TextureSliceObject::Render(glm::mat4 viewMatrix, glm::mat4 projectionMatrix)
{
	if(!visible) return; 
	
	OPENGL_FUNC_MACRO
	
	//compute mvp matrix
	glm::mat4 modelMatrix = GetModelMatrix(); 
	glm::mat4 mvpMatrix = projectionMatrix * viewMatrix * GetModelMatrix();
	glm::mat4 invMVMatrix = glm::inverse(viewMatrix * GetModelMatrix());
		
	//disable writting to depth buffer
	ogl->glEnable(GL_DEPTH_TEST);
	ogl->glDepthMask(GL_FALSE);
	
	//enable backface culling 
	ogl->glDisable(GL_CULL_FACE); 
	
	//enable blending
	ogl->glEnable(GL_BLEND);
	//ogl->glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	ogl->glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA); 
	//glBlendFunc(GL_ZERO, GL_SRC_COLOR);
	//glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	//glBlendFunc(GL_SRC_ALPHA, GL_ONE);

	//bind shader
	ogl->glUseProgram(programShaderObject);
	
	//update mvp transform uniform in shader
	int modelViewProjectionMatrixLocation = ogl->glGetUniformLocation(programShaderObject, "modelViewProjectionMatrix"); 
	ogl->glUniformMatrix4fv(modelViewProjectionMatrixLocation, 1, false, glm::value_ptr(mvpMatrix));
	
	int modelMatrixLocation = ogl->glGetUniformLocation(programShaderObject, "modelMatrix"); 
	ogl->glUniformMatrix4fv(modelMatrixLocation, 1, false, glm::value_ptr(modelMatrix));
	
	int viewMatrixLocation = ogl->glGetUniformLocation(programShaderObject, "viewMatrix"); 
	ogl->glUniformMatrix4fv(viewMatrixLocation, 1, false, glm::value_ptr(viewMatrix));
	
	int projectionMatrixLocation = ogl->glGetUniformLocation(programShaderObject, "projectionMatrix"); 
	ogl->glUniformMatrix4fv(projectionMatrixLocation, 1, false, glm::value_ptr(projectionMatrix));
	
	int invMVMatrixLocation = ogl->glGetUniformLocation(programShaderObject, "invMVMatrix"); 
	ogl->glUniformMatrix4fv(invMVMatrixLocation, 1, false, glm::value_ptr(invMVMatrix));
	
	//update 3d texture
	int texDimLocation = ogl->glGetUniformLocation(programShaderObject, "texDim"); 
	ogl->glUniform3f(texDimLocation, (float)volumeTexture->Width(), (float)volumeTexture->Height(), (float)volumeTexture->Depth());
	int volumeTextureLocation = ogl->glGetUniformLocation(programShaderObject, "volumeTexture"); 
	ogl->glUniform1i(volumeTextureLocation, 0);
	ogl->glActiveTexture(GL_TEXTURE0 + 0);
	
	if(volumeTexture != NULL)
	{
		ogl->glBindTexture(GL_TEXTURE_3D, volumeTexture->GetTextureId());
		
		ogl->glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		ogl->glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		ogl->glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
		ogl->glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
		ogl->glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_BORDER);
		float bcolor[] = { 0.0f, 0.0f, 0.0f, 0.0f };
		ogl->glTexParameterfv(GL_TEXTURE_3D, GL_TEXTURE_BORDER_COLOR, bcolor);
	}
	else
	{
		ogl->glBindTexture(GL_TEXTURE_3D, 0);
	}
	
	//update LUT texture
	int lutTextureLocation = ogl->glGetUniformLocation(programShaderObject, "lutTexture"); 
	ogl->glUniform1i(lutTextureLocation, 1);
	ogl->glActiveTexture(GL_TEXTURE0 + 1);
	
	if(lutTexture != NULL)
	{
		ogl->glBindTexture(GL_TEXTURE_1D, lutTexture->GetTextureId());
		
		ogl->glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		ogl->glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		ogl->glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		ogl->glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}
	else
	{
		ogl->glBindTexture(GL_TEXTURE_1D, 0);
	}
	
	//update material uniforms
	int materialBrightnessLocation = ogl->glGetUniformLocation(programShaderObject, "brightness"); 
	ogl->glUniform1f(materialBrightnessLocation, brightness);
	int materialContrastLocation = ogl->glGetUniformLocation(programShaderObject, "contrast"); 
	ogl->glUniform1f(materialContrastLocation, contrast);
	int materialThresholdLocation = ogl->glGetUniformLocation(programShaderObject, "threshold");
	ogl->glUniform1f(materialThresholdLocation, threshold);
	
	//bind VAO
	ogl->glBindVertexArray(vertexArrayObject);
	
	//draw elements
	ogl->glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
	
	//unbind VAO
	ogl->glBindVertexArray(0);
	
	//unbind shader program
	ogl->glUseProgram(0);
	
	//reset opengl state
	//enable writting to depth buffer
	ogl->glDepthMask(GL_TRUE);
}


void TextureSliceObject::Destroy()
{
	OPENGL_FUNC_MACRO
	ogl->glDeleteBuffers(1, &vertexBuffer);
	ogl->glDeleteBuffers(1, &elementBuffer);
	ogl->glDeleteVertexArrays(1, &vertexArrayObject);
}


void TextureSliceObject::SetVolumeTexture(Texture3D* vt)
{
	volumeTexture = vt; 
}


void TextureSliceObject::SetGradientTexture(Texture3D* gt)
{
	gradientTexture = gt; 
}


void TextureSliceObject::SetLUTTexture(Texture1D* lt)
{
	lutTexture = lt;
}


void TextureSliceObject::SetBrightness(double b)
{
	double ammount = 10.0;
	brightness = ammount * b / 100;
}


void TextureSliceObject::SetContrast(double c)
{
	double ammount = 10.0f;
	
	if(c < 0)
	{
		contrast = 1.0 / ( ammount * (-c / 100.0) + 1.0 );
	}
	else
	{
		contrast = ammount * (c / 100.0) + 1.0;
	}
}


void TextureSliceObject::SetThreshold(double t)
{
	threshold = t;
}
