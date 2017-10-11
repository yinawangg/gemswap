
#define _USE_MATH_DEFINES
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#if defined(__APPLE__)
#include <GLUT/GLUT.h>
#include <OpenGL/gl3.h>
#include <OpenGL/glu.h>
#else
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__)
#include <windows.h>
#endif
#include <GL/glew.h>		// must be downloaded 
#include <GL/freeglut.h>	// must be downloaded unless you have an Apple
#endif

const unsigned int windowWidth = 512, windowHeight = 512;

bool keyboardState[256];
double t;

// OpenGL major and minor versions
int majorVersion = 4, minorVersion = 1;

void getErrorInfo(unsigned int handle)
{
	int logLen;
	glGetShaderiv(handle, GL_INFO_LOG_LENGTH, &logLen);
	if (logLen > 0)
	{
		char * log = new char[logLen];
		int written;
		glGetShaderInfoLog(handle, logLen, &written, log);
		printf("Shader log:\n%s", log);
		delete log;
	}
}

// check if shader could be compiled
void checkShader(unsigned int shader, char * message)
{
	int OK;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &OK);
	if (!OK)
	{
		printf("%s!\n", message);
		getErrorInfo(shader);
	}
}

// check if shader could be linked
void checkLinking(unsigned int program)
{
	int OK;
	glGetProgramiv(program, GL_LINK_STATUS, &OK);
	if (!OK)
	{
		printf("Failed to link shader program!\n");
		getErrorInfo(program);
	}
}

// row-major matrix 4x4
struct mat4
{
	float m[4][4];
public:
	mat4() {}
	mat4(float m00, float m01, float m02, float m03,
		float m10, float m11, float m12, float m13,
		float m20, float m21, float m22, float m23,
		float m30, float m31, float m32, float m33)
	{
		m[0][0] = m00; m[0][1] = m01; m[0][2] = m02; m[0][3] = m03;
		m[1][0] = m10; m[1][1] = m11; m[1][2] = m12; m[1][3] = m13;
		m[2][0] = m20; m[2][1] = m21; m[2][2] = m22; m[2][3] = m23;
		m[3][0] = m30; m[3][1] = m31; m[3][2] = m32; m[3][3] = m33;
	}

	mat4 operator*(const mat4& right)
	{
		mat4 result;
		for (int i = 0; i < 4; i++)
		{
			for (int j = 0; j < 4; j++)
			{
				result.m[i][j] = 0;
				for (int k = 0; k < 4; k++) result.m[i][j] += m[i][k] * right.m[k][j];
			}
		}
		return result;
	}
	operator float*() { return &m[0][0]; }
};


// 3D point in homogeneous coordinates
struct vec4
{
	float v[4];

	vec4(float x = 0, float y = 0, float z = 0, float w = 1)
	{
		v[0] = x; v[1] = y; v[2] = z; v[3] = w;
	}

	vec4 operator*(const mat4& mat)
	{
		vec4 result;
		for (int j = 0; j < 4; j++)
		{
			result.v[j] = 0;
			for (int i = 0; i < 4; i++) result.v[j] += v[i] * mat.m[i][j];
		}
		return result;
	}

	vec4 operator+(const vec4& vec)
	{
		vec4 result(v[0] + vec.v[0], v[1] + vec.v[1], v[2] + vec.v[2], v[3] + vec.v[3]);
		return result;
	}
};

// 2D point in Cartesian coordinates
struct vec2
{
	float x, y;

	vec2(float x = 0.0, float y = 0.0) : x(x), y(y) {}

	vec2 operator+(const vec2& v)
	{
		return vec2(x + v.x, y + v.y);
	}

	vec2 operator*(float s)
	{
		return vec2(x * s, y * s);
	}
};

class Shader
{

	//protected:
	//    unsigned int shaderProgram;

public:

	Shader() {}


	~Shader() {
		//glDeleteProgram(shaderProgram);
	}

	virtual void CompileShader() = 0;

	virtual void UploadColor(vec4& color) = 0;

	virtual void UploadM(mat4& M) = 0;

	virtual void UploadTime(double t) = 0;

	virtual void Run() = 0;


};




class heartShader : public Shader {

	unsigned int shaderProgram;

public:
	heartShader() {
		CompileShader();
	}

	~heartShader() {
		glDeleteProgram(shaderProgram);
	}

	void CompileShader() {

		const char *vertexSource = R"(
#version 410
        precision highp float;
        
        in vec2 vertexPosition;		// variable input from Attrib Array selected by glBindAttribLocation
        uniform vec3 vertexColor;
        uniform mat4 M;
        out vec3 color;
        void main()
        {
            color = vertexColor;
            gl_Position = vec4(vertexPosition.x, vertexPosition.y, 0, 1) * M;
            // copy position from input to output
        }
        )";

		// fragment shader in GLSL
		const char *fragmentSource = R"(
#version 410
        precision highp float;
        uniform double time;
        in vec3 color;			// variable input: interpolated from the vertex colors
        out vec4 fragmentColor;		// output that goes to the raster memory as told by glBindFragDataLocation
        
        void main()
        {
            fragmentColor = vec4(color[0]*time,0,0,1); // extend RGB to RGBA
        }
        )";


		// create vertex shader from string
		unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
		if (!vertexShader) { printf("Error in vertex shader creation\n"); exit(1); }

		glShaderSource(vertexShader, 1, &vertexSource, NULL);
		glCompileShader(vertexShader);
		checkShader(vertexShader, "Vertex shader error");

		// create fragment shader from string
		unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
		if (!fragmentShader) { printf("Error in fragment shader creation\n"); exit(1); }

		glShaderSource(fragmentShader, 1, &fragmentSource, NULL);
		glCompileShader(fragmentShader);
		checkShader(fragmentShader, "Fragment shader error");

		// attach shaders to a single program
		shaderProgram = glCreateProgram();
		if (!shaderProgram) { printf("Error in shader program creation\n"); exit(1); }

		glAttachShader(shaderProgram, vertexShader);
		glAttachShader(shaderProgram, fragmentShader);

		// connect Attrib Array to input variables of the vertex shader
		glBindAttribLocation(shaderProgram, 0, "vertexPosition"); // vertexPosition gets values from Attrib Array 0

																  // connect the fragmentColor to the frame buffer memory
		glBindFragDataLocation(shaderProgram, 0, "fragmentColor"); // fragmentColor goes to the frame buffer memory

																   // program packaging
		glLinkProgram(shaderProgram);
		checkLinking(shaderProgram);

	}



	void UploadColor(vec4& color) {
		int location = glGetUniformLocation(shaderProgram, "vertexColor");
		if (location >= 0) glUniform3fv(location, 1, &color.v[0]);
		else printf("uniform vertexColor (heart) cannot be set\n");
	}

	void UploadM(mat4& M) {
		int location = glGetUniformLocation(shaderProgram, "M");
		if (location >= 0) glUniformMatrix4fv(location, 1, GL_TRUE, M);
		else printf("uniform M (heart) cannot be set\n");
	}

	void UploadTime(double t) {
		int location = glGetUniformLocation(shaderProgram, "time");
		if (location >= 0) glUniform1d(location, t);
		else printf("uniform time (heart) cannot be set\n");
	}


	void Run() {
		glUseProgram(shaderProgram);
	}


};


class normalShader : public Shader {

	unsigned int shaderProgram;

public:
	normalShader() {
		CompileShader();
	}

	~normalShader() {
		glDeleteProgram(shaderProgram);
	}

	void CompileShader() {

		const char *vertexSource = R"(
        #version 410
        precision highp float;
        
        in vec2 vertexPosition;		// variable input from Attrib Array selected by glBindAttribLocation
        uniform vec3 vertexColor;
        uniform mat4 M;
        out vec3 color;
        void main()
        {
            color = vertexColor;				 		// set vertex color
            gl_Position = vec4(vertexPosition.x, vertexPosition.y, 0, 1) * M;
            // copy position from input to output
        }
        )";

		// fragment shader in GLSL
		const char *fragmentSource = R"(
        #version 410
        precision highp float;
        
        in vec3 color;			// variable input: interpolated from the vertex colors
        out vec4 fragmentColor;		// output that goes to the raster memory as told by glBindFragDataLocation
        
        void main()
        {
            fragmentColor = vec4(color, 1); // extend RGB to RGBA
        }
        )";


		// create vertex shader from string
		unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
		if (!vertexShader) { printf("Error in vertex shader creation\n"); exit(1); }

		glShaderSource(vertexShader, 1, &vertexSource, NULL);
		glCompileShader(vertexShader);
		checkShader(vertexShader, "Vertex shader error");

		// create fragment shader from string
		unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
		if (!fragmentShader) { printf("Error in fragment shader creation\n"); exit(1); }

		glShaderSource(fragmentShader, 1, &fragmentSource, NULL);
		glCompileShader(fragmentShader);
		checkShader(fragmentShader, "Fragment shader error");

		// attach shaders to a single program
		shaderProgram = glCreateProgram();
		if (!shaderProgram) { printf("Error in shader program creation\n"); exit(1); }

		glAttachShader(shaderProgram, vertexShader);
		glAttachShader(shaderProgram, fragmentShader);

		// connect Attrib Array to input variables of the vertex shader
		glBindAttribLocation(shaderProgram, 0, "vertexPosition"); // vertexPosition gets values from Attrib Array 0

																  // connect the fragmentColor to the frame buffer memory
		glBindFragDataLocation(shaderProgram, 0, "fragmentColor"); // fragmentColor goes to the frame buffer memory

																   // program packaging
		glLinkProgram(shaderProgram);
		checkLinking(shaderProgram);
	}


	void UploadColor(vec4& color) {
		int location = glGetUniformLocation(shaderProgram, "vertexColor");
		if (location >= 0) glUniform3fv(location, 1, &color.v[0]);
		else printf("uniform vertexColor cannot be set\n");
	}

	void UploadM(mat4& M) {
		int location = glGetUniformLocation(shaderProgram, "M");
		if (location >= 0) glUniformMatrix4fv(location, 1, GL_TRUE, M);
		else printf("uniform M cannot be set\n");
	}

	void UploadTime(double t) {}



	void Run() {
		glUseProgram(shaderProgram);
	}


};


class Camera
{
    vec2 center;
    vec2 halfSize;
	float orientation;
	bool b;

public:
    Camera()
    {
        center = vec2(0.0, 0.0);
        halfSize =  vec2(1.0, 1.0);
		orientation = 0.0;
		b = false;
    }

    mat4 GetViewTransformationMatrix()
    {
		float orientation_rad = orientation / 180 * M_PI;
        mat4 T = mat4(
                      1.0, 0.0, 0.0, 0.0,
                      0.0, 1.0, 0.0, 0.0,
                      0.0, 0.0, 1.0, 0.0,
                      -center.x, -center.y, 0.0, 1.0);

        mat4 S = mat4(
                      1.0 / halfSize.x, 0.0, 0.0, 0.0,
                      0.0, 1.0 / halfSize.y, 0.0, 0.0,
                      0.0, 0.0, 1.0, 0.0,
                      0.0, 0.0, 0.0, 1.0);
			mat4 R = mat4(cos(orientation_rad), sin(orientation_rad), 0, 0,
				-sin(orientation_rad), cos(orientation_rad), 0, 0,
				0, 0, 1, 0,
				0, 0, 0, 1);
		

        return T * R * S;
    }

	mat4 getInverseViewTransformationMatrix() {

		float orientation_rad = orientation / 180 * M_PI;
		mat4 T = mat4(
			1.0, 0.0, 0.0, 0.0,
			0.0, 1.0, 0.0, 0.0,
			0.0, 0.0, 1.0, 0.0,
			center.x, center.y, 0.0, 1.0);

		mat4 S = mat4(
			1.0 * halfSize.x, 0.0, 0.0, 0.0,
			0.0, 1.0 * halfSize.y, 0.0, 0.0,
			0.0, 0.0, 1.0, 0.0,
			0.0, 0.0, 0.0, 1.0);
		mat4 R = mat4(cos(-orientation_rad), sin(-orientation_rad), 0, 0,
			sin(orientation_rad), cos(-orientation_rad), 0, 0,
			0, 0, 1, 0,
			0, 0, 0, 1);

		return S * R * T;
	}

    void SetAspectRatio(int width, int height)
    {
        halfSize = vec2((float)width / height,1.0);
    }

    void Move(float dt)
    {
        if(keyboardState['j']) center = center + vec2(1.0 , 0.0) * dt;
        if(keyboardState['l']) center = center + vec2(-1.0, 0.0) * dt;
        if(keyboardState['k']) center = center + vec2(0.0, 1.0) * dt;
        if(keyboardState['i']) center = center + vec2(0.0, -1.0) * dt;
	}

	void Quake(float dt) {
		center = center + vec2(0.001, 0.0) * dt; 
	}

	void rotateCamClock() {

		if (!b) {
			orientation += M_PI/2;
			b = true;
		}
	}

	void rotateCamCount() {

		if (!b) {
			orientation -= M_PI/2;
			b = true;
		}
	}


	void reset() {
		b = false;
	}

	
};

Camera camera;


class Geometry
{
protected:
	unsigned int vao;

public:
	virtual void Draw() = 0;
	Geometry()
	{
		glGenVertexArrays(1, &vao);
	}
};


class Material
{
	Shader* shader;
	vec4 color;

public:
	Material(Shader* shader, vec4 color)
	{
		this->shader = shader;
		this->color = color;
	}

	void UploadAttributes()
	{
		shader->UploadColor(color);
	}
};



class Mesh
{
	Geometry* geometry;
	Material* material;

public:
	Mesh(Geometry* geometry, Material* material)
	{
		this->material = material;
		this->geometry = geometry;
	}

	void Draw()
	{
		material->UploadAttributes();
		geometry->Draw();
	}
};

class Object
{
	Shader* shader;
	Mesh* mesh;
	vec2 position;
	vec2 scaling;
	float orientation;
	int ID;
	bool d;

public:
	Object(Shader* shader, Mesh* mesh, vec2 position, vec2 scaling, float orientation, int ID)
	{
		this->shader = shader;
		this->mesh = mesh;
		this->position = position;
		this->scaling = scaling;
		this->orientation = orientation;
		this->ID = ID;
		d = false;
	}

	void UploadAttributes()
	{
		mat4 S = mat4(scaling.x, 0, 0, 0,
			0, scaling.y, 0, 0,
			0, 0, 1, 0,
			0, 0, 0, 1);
		float orientation_rad = (orientation * M_PI) / 180;
		mat4 R = mat4(cos(orientation_rad), sin(orientation_rad), 0, 0,
			-sin(orientation_rad), cos(orientation_rad), 0, 0,
			0, 0, 1, 0,
			0, 0, 0, 1);
		mat4 T = mat4(1, 0, 0, 0,
			0, 1, 0, 0,
			0, 0, 1, 0,
			position.x, position.y, 0, 1);
		mat4 V = camera.GetViewTransformationMatrix();

		mat4 M = S * R * T * V; // scaling, rotation, and translation

		shader->UploadM(M);
	}


	int getID() {
		return ID;
	}

	vec2 getScale() {
		return scaling;
	}

	void Scale(float dt) {
		scaling.x = scaling.x *dt;
		scaling.y = scaling.y * dt;
	}

	vec2 getPosition() {
		return position;
	}

	void setPosition(vec2 position) {
		this->position = position;
	}

	float getOrientation() {
		return orientation;
	}

	void setOrientation(float orientation) {
		this->orientation = orientation;
	}

	void Rotate(float dt) {
		orientation += 0.1 * dt;
	}

	void DeleteBlock() {
		d = true;
	}

	void CheckD() {
		if (d) {
			Scale(0.999);
			Rotate(-4);
		}
	}

	void Draw()
	{
		UploadAttributes();
		mesh->Draw();
	}
};


class Triangle : public Geometry
{
	unsigned int vbo;	// vertex array object id

public:
	Triangle()
	{
		glBindVertexArray(vao);		// make it active

		glGenBuffers(1, &vbo);		// generate a vertex buffer object

									// vertex coordinates: vbo -> Attrib Array 0 -> vertexPosition of the vertex shader
		glBindBuffer(GL_ARRAY_BUFFER, vbo); // make it active, it is an array
		static float vertexCoords[] = { -0.5, -0.5, 0.5, -0.5, -0.5, 0.5 };	// vertex data on the CPU

		glBufferData(GL_ARRAY_BUFFER,	// copy to the GPU
			sizeof(vertexCoords),	// size of the vbo in bytes
			vertexCoords,		// address of the data array on the CPU
			GL_STATIC_DRAW);	// copy to that part of the memory which is not modified

								// map Attribute Array 0 to the currently bound vertex buffer (vbo)
		glEnableVertexAttribArray(0);

		// data organization of Attribute Array 0
		glVertexAttribPointer(0,	// Attribute Array 0
			2, GL_FLOAT,		// components/attribute, component type
			GL_FALSE,		// not in fixed point format, do not normalized
			0, NULL);		// stride and offset: it is tightly packed
	}

	void Draw()
	{
		glBindVertexArray(vao);	// make the vao and its vbos active playing the role of the data source
		glDrawArrays(GL_TRIANGLES, 0, 3); // draw a single triangle with vertices defined in vao
	}
};

class Quad : public Geometry
{
	unsigned int vbo;

public:
	Quad()
	{
		glBindVertexArray(vao);

		glGenBuffers(1, &vbo);

		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		static float vertexCoords[12];
		vertexCoords[0] = 0;
		vertexCoords[1] = 0;
		float A = M_PI / 2;
		for (int i = 1; i < 6; i++)
		{
			vertexCoords[i * 2] = cos(A) * .75;
			vertexCoords[i * 2 + 1] = sin(A) * .75;
			A += M_PI / 2;
		}


		//static float vertexCoords[] = { 0, 0, 1, 0, 0, 1, 1, 1 };
		glBufferData(GL_ARRAY_BUFFER,	// copy to the GPU
			sizeof(vertexCoords),	// size of the vbo in bytes
			vertexCoords,		// address of the data array on the CPU
			GL_STATIC_DRAW);	// copy to that part of the memory which is not modified

								// map Attribute Array 0 to the currently bound vertex buffer (vbo)
		glEnableVertexAttribArray(0);

		// data organization of Attribute Array 0
		glVertexAttribPointer(0,	// Attribute Array 0
			2, GL_FLOAT,		// components/attribute, component type
			GL_FALSE,		// not in fixed point format, do not normalized
			0, NULL);		// stride and offset: it is tightly packed
	}



	void Draw()
	{
		glBindVertexArray(vao);	// make the vao and its vbos active playing the role of the data source
		glDrawArrays(GL_TRIANGLE_FAN, 0, 6); // draw a single triangle with vertices defined in vao
	}
};


class Stellar : public Geometry
{
	unsigned int vbo;

public:
	Stellar()
	{
		glBindVertexArray(vao);

		glGenBuffers(1, &vbo);

		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		static float vertexCoords[24];
		float r = .75; 
		vertexCoords[0] = 0;
		vertexCoords[1] = 0;
		float A = M_PI / 2;
		for (int i = 1; i < 13; i++)
		{
			if (i % 2 == 0)
			{
				r = (sin(M_PI / 10) / sin(3 * M_PI / 10)) * .75;
			}
			vertexCoords[i* 2] = cos(A) * r;
			vertexCoords[i*2 + 1] = sin(A) * r;
			A += M_PI / 5;
			r = .75;
		}



		glBufferData(GL_ARRAY_BUFFER,	// copy to the GPU
			sizeof(vertexCoords),	// size of the vbo in bytes
			vertexCoords,		// address of the data array on the CPU
			GL_STATIC_DRAW);	// copy to that part of the memory which is not modified

								// map Attribute Array 0 to the currently bound vertex buffer (vbo)
		glEnableVertexAttribArray(0);

		// data organization of Attribute Array 0
		glVertexAttribPointer(0,	// Attribute Array 0
			2, GL_FLOAT,		// components/attribute, component type
			GL_FALSE,		// not in fixed point format, do not normalized
			0, NULL);		// stride and offset: it is tightly packed
	}



	void Draw()
	{
		glBindVertexArray(vao);	// make the vao and its vbos active playing the role of the data source
		glDrawArrays(GL_TRIANGLE_FAN, 0, 12); // draw a single triangle with vertices defined in vao
	
	}
};

class Pentagon : public Geometry
{
	unsigned int vbo;

public:
	Pentagon()
	{
		glBindVertexArray(vao);

		glGenBuffers(1, &vbo);

		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		static float vertexCoords[14];
		vertexCoords[0] = 0;
		vertexCoords[1] = 0;
		float A = M_PI / 2;
		for (int i = 1; i < 7; i++)
		{
			vertexCoords[i * 2] = cos(A) * 0.75;
			vertexCoords[i * 2 + 1] = sin(A) * 0.75;
			A += 2 * M_PI / 5;
		}
		// vertices in a pentagon
		glBufferData(GL_ARRAY_BUFFER,	// copy to the GPU
			sizeof(vertexCoords),	// size of the vbo in bytes
			vertexCoords,		// address of the data array on the CPU
			GL_STATIC_DRAW);	// copy to that part of the memory which is not modified

								// map Attribute Array 0 to the currently bound vertex buffer (vbo)
		glEnableVertexAttribArray(0);

		// data organization of Attribute Array 0
		glVertexAttribPointer(0,	// Attribute Array 0
			2, GL_FLOAT,		// components/attribute, component type
			GL_FALSE,		// not in fixed point format, do not normalized
			0, NULL);		// stride and offset: it is tightly packed
	}

	void Draw()
	{
		glBindVertexArray(vao);	// make the vao and its vbos active playing the role of the data source
		glDrawArrays(GL_TRIANGLE_FAN, 0, 7); // 
	}
};

class Hexagon : public Geometry
{
	unsigned int vbo;

public:
	Hexagon()
	{
		glBindVertexArray(vao);

		glGenBuffers(1, &vbo);

		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		static float vertexCoords[16];
		vertexCoords[0] = 0;
		vertexCoords[1] = 0;
		float A = M_PI / 2;
		for (int i = 1; i < 8; i++)
		{
			vertexCoords[i * 2] = cos(A) * 0.75;
			vertexCoords[i * 2 + 1] = sin(A) * 0.75;
			A += M_PI / 3;
		}
		glBufferData(GL_ARRAY_BUFFER,	// copy to the GPU
			sizeof(vertexCoords),	// size of the vbo in bytes
			vertexCoords,		// address of the data array on the CPU
			GL_STATIC_DRAW);	// copy to that part of the memory which is not modified

								// map Attribute Array 0 to the currently bound vertex buffer (vbo)
		glEnableVertexAttribArray(0);

		// data organization of Attribute Array 0
		glVertexAttribPointer(0,	// Attribute Array 0
			2, GL_FLOAT,		// components/attribute, component type
			GL_FALSE,		// not in fixed point format, do not normalized
			0, NULL);		// stride and offset: it is tightly packed
	}



	void Draw()
	{
		glBindVertexArray(vao);	// make the vao and its vbos active playing the role of the data source
		glDrawArrays(GL_TRIANGLE_FAN, 0, 8); // draw a single triangle with vertices defined in vao
	}
};

class Heart : public Geometry
{
	unsigned int vbo;

public:
	Heart()
	{
		glBindVertexArray(vao);

		glGenBuffers(1, &vbo);

		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		static float vertexCoords[104];
		float triangle_num = 50;
		int i;
		int index = 0;
		vertexCoords[0] = 0;
		vertexCoords[1] = 0;
		int r = 0.75;
		index += 2;
		for (i = 2; i < 53; i++) {

			vertexCoords[index] = (16 * pow(sin((2 * i * M_PI) / triangle_num), 3)) / 24;
			vertexCoords[index + 1] = (13 * cos((2 * i * M_PI) / triangle_num) - 5 * cos(2 * (2 * i * M_PI) / triangle_num)
				- 2 * cos(3 * (2 * i * M_PI) / triangle_num) - cos(4 * (2 * i * M_PI) / triangle_num)) / 24;
			index += 2;
		}
		glBufferData(GL_ARRAY_BUFFER,	// copy to the GPU
			sizeof(vertexCoords),	// size of the vbo in bytes
			vertexCoords,		// address of the data array on the CPU
			GL_STATIC_DRAW);	// copy to that part of the memory which is not modified

								// map Attribute Array 0 to the currently bound vertex buffer (vbo)
		glEnableVertexAttribArray(0);

		// data organization of Attribute Array 0
		glVertexAttribPointer(0,	// Attribute Array 0
			2, GL_FLOAT,		// components/attribute, component type
			GL_FALSE,		// not in fixed point format, do not normalized
			0, NULL);		// stride and offset: it is tightly packed
	}



	void Draw()
	{
		glBindVertexArray(vao);	// make the vao and its vbos active playing the role of the data source
		glDrawArrays(GL_TRIANGLE_FAN, 0, 52); // draw a single triangle with vertices defined in vao
	}
};

# include <vector>
Object* objectgrid[10][10];

class Scene {
	Shader* shader;
	Shader* hShader;
	std::vector<Material*> materials;
	std::vector<Geometry*> geometries;
	std::vector<Mesh*> meshes;

	int currentI;
	int currentJ;

	int futureI;
	int futureJ;

public:
	Scene() { 
		
		shader = 0; 
		hShader = 0; 
	}
	void Initialize() {
		shader = new normalShader();
		hShader = new heartShader();
	
		// build the scene here
		//materials.push_back(new Material(shader, vec4(1, 0, 0)));
		//materials.push_back(new Material(shader, vec4(0, 1, 0)));
		//materials.push_back(new Material(shader, vec4(0, 0, 1)));
		//materials.push_back(new Material(shader, vec4(0, 1, 1)));
		//materials.push_back(new Material(shader, vec4(1, 1, 1)));
		//materials.push_back(new Material(hShader, vec4(0.5, 0, 1)));


		//geometries.push_back(new Triangle());
		//geometries.push_back(new Quad());
		//geometries.push_back(new Stellar()); 
		//geometries.push_back(new Pentagon());
		//geometries.push_back(new Hexagon());
		//geometries.push_back(new Heart());

		//meshes.push_back(new Mesh(geometries[0], materials[0]));
		//meshes.push_back(new Mesh(geometries[1], materials[1]));
		//meshes.push_back(new Mesh(geometries[2], materials[2]));
		//meshes.push_back(new Mesh(geometries[3], materials[3]));
		//meshes.push_back(new Mesh(geometries[4], materials[4]));
		//meshes.push_back(new Mesh(geometries[5], materials[5]));

		/*objects.push_back(new Object(shader, meshes[0], vec2(-0.5, -0.5), vec2(0.5, 1.0), 10.0));
		objects.push_back(new Object(shader, meshes[1], vec2(0.25, 0.5), vec2(0.5, 0.5), -30.0));*/

		for (int i = 0; i < 10; i++)
			for (int j = 0; j < 10; j++)
			{
				int r = (rand() % 6) + 1;
				switch (r)
				{
				case 1: 
					materials.push_back(new Material(shader, vec4(1, 0.5, 0)));
					geometries.push_back(new Triangle());
					meshes.push_back(new Mesh(geometries[(i*10) + j], materials[(i*10) + j]));
					objectgrid[i][j] = new Object(shader, meshes[(i * 10) + j],
						vec2((((float)(i - 5)) / 5) + 0.08, (((float)(j - 5)) / 5) + 0.08), vec2(0.1, 0.1), 0.0, 1);
					break;
				case 2: 
					materials.push_back(new Material(shader, vec4(0.3, 1, 0)));
					geometries.push_back(new Quad());
					meshes.push_back(new Mesh(geometries[(i * 10) + j], materials[(i * 10) + j]));
					objectgrid[i][j] = new Object(shader, meshes[(i * 10) + j],
						vec2((((float)(i - 5)) / 5) + 0.08, (((float)(j - 5)) / 5) + 0.08), vec2(0.1, 0.1), 0.0, 2);
					break;
				case 3: 
					materials.push_back(new Material(shader, vec4(0.6, 0, 1)));
					geometries.push_back(new Stellar());
					meshes.push_back(new Mesh(geometries[(i * 10) + j], materials[(i * 10) + j]));
					objectgrid[i][j] = new Object(shader, meshes[(i * 10) + j],
						vec2((((float)(i - 5)) / 5) + 0.08, (((float)(j - 5)) / 5) + 0.08), vec2(0.1, 0.1), 0.0, 3);
					break;
				case 4:
					materials.push_back(new Material(shader, vec4(0.54, 1, 1)));
					geometries.push_back(new Pentagon());
					meshes.push_back(new Mesh(geometries[(i * 10) + j], materials[(i * 10) + j]));
					objectgrid[i][j] = new Object(shader, meshes[(i * 10) + j],
						vec2((((float)(i - 5)) / 5) + 0.08, (((float)(j - 5)) / 5) + 0.08), vec2(0.1, 0.1), 0.0, 4);
					break;
				case 5:
					materials.push_back(new Material(shader, vec4(1, 1, 1)));
					geometries.push_back(new Hexagon());
					meshes.push_back(new Mesh(geometries[(i * 10) + j], materials[(i * 10) + j]));
					objectgrid[i][j] = new Object(shader, meshes[(i * 10) + j],
						vec2((((float)(i - 5)) / 5) + 0.08, (((float)(j - 5)) / 5) + 0.08), vec2(0.1, 0.1), 0.0, 5);
					break;
				case 6:
					materials.push_back(new Material(hShader, vec4(0.5, 0, 1)));
					geometries.push_back(new Heart());
					meshes.push_back(new Mesh(geometries[(i * 10) + j], materials[(i * 10) + j]));
					objectgrid[i][j] = new Object(hShader, meshes[(i * 10) + j],
						vec2((((float)(i - 5)) / 5) + 0.08, (((float)(j - 5)) / 5) + 0.08), vec2(0.1, 0.1), 0.0, 6);
				}
				/*objectgrid[i][j] = new Object(shader, meshes[(i * 10) + j], 
					vec2((((float)(i - 5)) / 5) + 0.08, (((float)(j - 5)) / 5) + 0.08), vec2(0.1, 0.1), 0.0);*/
			}

		shader->Run();
	}
	~Scene() {
		for (int i = 0; i < materials.size(); i++) delete materials[i];
		for (int i = 0; i < geometries.size(); i++) delete geometries[i];
		for (int i = 0; i < meshes.size(); i++) delete meshes[i];
		//for (int i = 0; i < objects.size(); i++) delete objects[i];
		for (int i = 0; i < 10; i++)
			for (int j = 0; j < 10; j++)
			{
				delete objectgrid[i][j];
			}
		if (shader) delete shader;
	}

	void HeartBeat(double t) {

		//        for(int i = 0; i < 10; i++){
		//            for(int j = 0; j < 10; j++){
		//                if(shapes[i][j]==2){
		//                    materials[i][j]->ChangeColor(t);
		//                }
		//            }
		//        }
		hShader->Run();
		hShader->UploadTime(sin(3 * t));
	}

	void Update() {
		for (int i = 0; i < 10; i++)
			for (int j = 0; j < 10; j++)
			{
				if (j - 1 >= 0 && j + 1 < 10)
				{
					if (objectgrid [i][j]->getID() == objectgrid[i][j + 1]->getID() && objectgrid[i][j]->getID() == objectgrid[i][j - 1]->getID())
					{
						objectgrid[i][j - 1]->DeleteBlock();
						objectgrid[i][j]->DeleteBlock();
						objectgrid[i][j + 1]->DeleteBlock();

					}

					if (objectgrid[j][i]->getID() == objectgrid[j + 1][i]->getID() && objectgrid[j][i]->getID() == objectgrid[j - 1][i]->getID())
					{
						objectgrid[j - 1][i]->DeleteBlock();
						objectgrid[j][i]->DeleteBlock();
						objectgrid[j + 1][i]->DeleteBlock();
					}

				}

				if (objectgrid[i][j]->getID() == 4)
				{
					objectgrid[i][j]->Rotate(0.5);
				}

				objectgrid[i][j]->CheckD();
				if (keyboardState['b']) {
					objectgrid[currentI][currentJ]->DeleteBlock();
				}
				if (objectgrid[i][j]->getScale().x < 0.02) {
					int r = (rand() % 6) + 1;
					switch (r)
					{
					case 1:
						materials[(i * 10) + j] = new Material(shader, vec4(1, 0.5, 0));
						geometries[(i * 10) + j] = new Triangle();
						meshes[(i * 10) + j] = new Mesh(geometries[(i * 10) + j], materials[(i * 10) + j]);
						objectgrid[i][j] = new Object(shader, meshes[(i * 10) + j],
							vec2((((float)(i - 5)) / 5) + 0.08, (((float)(j - 5)) / 5) + 0.08), vec2(0.1, 0.1), 0.0, 1);
						break;
					case 2:
						materials[(i * 10) + j] = new Material(shader, vec4(0.3, 1, 0));
						geometries[(i * 10) + j] = new Quad();
						meshes[(i * 10) + j] = new Mesh(geometries[(i * 10) + j], materials[(i * 10) + j]);
						objectgrid[i][j] = new Object(shader, meshes[(i * 10) + j],
							vec2((((float)(i - 5)) / 5) + 0.08, (((float)(j - 5)) / 5) + 0.08), vec2(0.1, 0.1), 0.0, 2);
						break;
					case 3:
						materials[(i * 10) + j] = new Material(shader, vec4(0.6, 0, 1));
						geometries[(i * 10) + j] = new Stellar();
						meshes[(i * 10) + j] = new Mesh(geometries[(i * 10) + j], materials[(i * 10) + j]);
						objectgrid[i][j] = new Object(shader, meshes[(i * 10) + j],
							vec2((((float)(i - 5)) / 5) + 0.08, (((float)(j - 5)) / 5) + 0.08), vec2(0.1, 0.1), 0.0, 3);
						break;
					case 4:
						materials[(i * 10) + j] = new Material(shader, vec4(0.54, 1, 1));
						geometries[(i * 10) + j] = new Pentagon();
						meshes[(i * 10) + j] = new Mesh(geometries[(i * 10) + j], materials[(i * 10) + j]);
						objectgrid[i][j] = new Object(shader, meshes[(i * 10) + j],
							vec2((((float)(i - 5)) / 5) + 0.08, (((float)(j - 5)) / 5) + 0.08), vec2(0.1, 0.1), 0.0, 4);
						break;
					case 5:
						materials[(i * 10) + j] = new Material(shader, vec4(1, 1, 1));
						geometries[(i * 10) + j] = new Hexagon();
						meshes[(i * 10) + j] = new Mesh(geometries[(i * 10) + j], materials[(i * 10) + j]);
						objectgrid[i][j] = new Object(shader, meshes[(i * 10) + j],
							vec2((((float)(i - 5)) / 5) + 0.08, (((float)(j - 5)) / 5) + 0.08), vec2(0.1, 0.1), 0.0, 5);
						break;
					case 6:
						materials[(i * 10) + j] = new Material(hShader, vec4(0.5, 0, 1));
						geometries[(i * 10) + j] = new Heart();
						meshes[(i * 10) + j] = new Mesh(geometries[(i * 10) + j], materials[(i * 10) + j]);
						objectgrid[i][j] = new Object(hShader, meshes[(i * 10) + j],
							vec2((((float)(i - 5)) / 5) + 0.08, (((float)(j - 5)) / 5) + 0.08), vec2(0.1, 0.1), 0.0, 6);
						break;
					}
				}
			}
		if (keyboardState['q']) {
			camera.Quake(sin(t * 100));
			for (int i = 0; i < 10; i++) {
				for (int j = 0; j < 10; j++) {
					int ran = rand() % 5000;
					if (ran == 1) {
						objectgrid[i][j]->DeleteBlock();
					}
				}
			 }
		}


	}

	//void SetOrientation(double t) {
	//	for (int i = 0; i < 10; i++) {
	//		for (int j = 0; j < 10; j++) {
	//			if (shapes[i][j] == 3) {
	//				objects[i][j]->SetOrientation(t);
	//			}
	//		}
	//	}
	//}

	bool hasThree(int x1, int y1)
	{
		int shape1 = objectgrid[x1][y1]->getID();

		int count = 0;
		for (int i = x1 - 2; i <= x1 + 2; i++)
		{
			if ((i != x1) && (i <= 9) && (i >= 0)) {
				if (shape1 == objectgrid[i][y1]->getID())
				{
					count++;
					if (count == 2) return true;
				}
				else count = 0;
			}
		}

		for (int i = y1 - 2; i <= y1 + 2; i++)
		{
			if ((i <= 9) && (i >= 0) && (i != y1)) {
				if (shape1 == objectgrid[x1][i]->getID())
				{
					count++;
					if (count == 2) return true;
				}
				else count = 0;
			}
		}

		return false;
	}

	bool isLegal(int i, int j, int u, int v) {
		return hasThree(i, j) || hasThree(u, v);
	}

	void Select(int u, int v) {
		currentI = u;
		currentJ = v;
	}

	void Swap(int u, int v) {
		int diffX = abs(u - currentI);
		int diffY = abs(v - currentJ);

		if (((diffX == 1) && !diffY) || ((diffY == 1) && !diffX))
			// check to make sure shapes are neighboring
		{
			Object* obj = objectgrid[u][v];
			vec2 pos = obj->getPosition();

			objectgrid[u][v]->setPosition(objectgrid[currentI][currentJ]->getPosition());
			objectgrid[currentI][currentJ]->setPosition(pos);

			objectgrid[u][v] = objectgrid[currentI][currentJ];
			objectgrid[currentI][currentJ] = obj;

			if (!isLegal(currentI, currentJ, u, v)) // make sure swap is Legal, if not swap em back
			{
				Object* obj = objectgrid[u][v];
				objectgrid[u][v] = objectgrid[currentI][currentJ];
				objectgrid[currentI][currentJ] = obj;

				vec2 position = objectgrid[u][v]->getPosition();
				vec2 position2 = objectgrid[currentI][currentJ]->getPosition();
				objectgrid[u][v]->setPosition(position2);
				objectgrid[currentI][currentJ]->setPosition(position);
			}
		}


	}



	void Draw()
	{
		for (int i = 0; i < 10; i++)
			for (int j = 0; j < 10; j++)
			{
				if (objectgrid[i][j]->getID() == 6) {
					hShader->Run();
				}
				else {
					shader->Run();
				}
				objectgrid[i][j]->Draw();
			}
	}
};


//Shader* gShader = 0;
//Mesh* gMesh = 0;
//Material* gMaterial = 0;
//Geometry* gGeometry = 0;
//Object* gObject = 0;

Scene *scene;

void onKeyboard(unsigned char key, int x, int y)
{
	keyboardState[key] = true;
}

void onKeyboardUp(unsigned char key, int x, int y)
{
	keyboardState[key] = false;
}

void onReshape(int winWidth0, int winHeight0)
{
	camera.SetAspectRatio(winWidth0, winHeight0);
	glViewport(0, 0, winWidth0, winHeight0);
	glutPostRedisplay();
}

void onMouse(int button, int state, int i, int j)
{
	int viewport[4];
	glGetIntegerv(GL_VIEWPORT, viewport);

	float x = ((float)i / viewport[2]) * 2.0 - 1.0;
	float y = 1.0 - ((float)j / viewport[3]) * 2.0;

	mat4 invV = camera.getInverseViewTransformationMatrix();

	vec4 p = vec4(x, y, 0, 1) * invV;

	int u = (int)floor((p.v[0] + 1.0) * 5.0);
	int v = (int)floor((p.v[1] + 1.0) * 5.0);

	printf("%i %i\n", u, v);

	if ((u < 0) || (u > 9) || (v < 0) || (v > 9)) return;

	if (state == GLUT_DOWN)
		scene->Select(u, v);
	if (state == GLUT_UP)
		scene->Swap(u, v);
}

void onIdle() {
	// time elapsed since program started, in seconds
	t = glutGet(GLUT_ELAPSED_TIME) * 0.001;
	// variable to remember last time idle was called
	static double lastTime = 0.0;
	// time difference between calls: time step  
	double dt = t - lastTime;
	// store time
	lastTime = t;

	if (keyboardState['d']) {
		camera.rotateCamClock();
		camera.reset();
	}

	if (keyboardState['a']) {
		camera.rotateCamCount();
		camera.reset();
	}


	//triangle0.Scale(sin(t));
	//triangle1.Rotate(dt);
	//triangle2.Move(sin(t));

	camera.Move(dt);

	scene->HeartBeat(t);
	scene->Update();

	glutPostRedisplay();
}

// initialization, create an OpenGL context
void onInitialization()
{
	//int i, j;
	//Shader* gShader;
	//Mesh* gMesh;
	//Material* gMaterial;
	//Geometry* gGeometry;
	//Object* gObject;

	
			 //   gShader = new Shader();
			
			 //   //gMaterial = new Material(gShader, vec4(1,0,0));
			 //   //gGeometry = new Triangle();
			 //   //gGeometry = new Quad();
			 //   //gMesh = new Mesh(gGeometry, gMaterial);
			 //   gObject = new Object(gShader, gMesh, vec2(-0.5, -0.5),
			 //                       vec2(0.5, 1.0), -30.0);

				//gShader->Run();
		

	glViewport(0, 0, windowWidth, windowHeight);

	//    gShader = new Shader();
	//
	//    gMaterial = new Material(gShader, vec4(1,0,0));
	//    gGeometry = new Triangle();
	//    gGeometry = new Quad();
	//    gMesh = new Mesh(gGeometry, gMaterial);
	//    gObject = new Object(gShader, gMesh, vec2(-0.5, -0.5),
	//                         vec2(0.5, 1.0), -30.0);
	scene = new Scene();
	scene->Initialize();

	//    gShader->Run();

}

void onExit()
{
	//    if (gShader) delete gShader;
	//    if (gMesh) delete gMesh;
	//    if (gMaterial) delete gMaterial;
	//    if (gGeometry) delete gGeometry;
	//    if (gObject) delete gObject;

	printf("exit");
}

// window has become invalid: redraw
void onDisplay()
{
	glClearColor(0, 0, 0, 0); // background color 
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // clear the screen

														//    gObject->Draw();
	scene->Draw();

	glutSwapBuffers(); // exchange the two buffers
}


int main(int argc, char * argv[])
{
	glutInit(&argc, argv);
#if !defined(_APPLE_)
	glutInitContextVersion(majorVersion, minorVersion);
#endif
	glutInitWindowSize(windowWidth, windowHeight); 	// application window is initially of resolution 512x512
	glutInitWindowPosition(50, 50);			// relative location of the application window
#if defined(_APPLE_)
	glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH | GLUT_3_2_CORE_PROFILE);  // 8 bit R,G,B,A + double buffer + depth buffer
#else
	glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
#endif
	glutCreateWindow("Triangle Rendering");

#if !defined(_APPLE_)
	glewExperimental = true;
	glewInit();
#endif
	printf("GL Vendor    : %s\n", glGetString(GL_VENDOR));
	printf("GL Renderer  : %s\n", glGetString(GL_RENDERER));
	printf("GL Version (string)  : %s\n", glGetString(GL_VERSION));
	glGetIntegerv(GL_MAJOR_VERSION, &majorVersion);
	glGetIntegerv(GL_MINOR_VERSION, &minorVersion);
	printf("GL Version (integer) : %d.%d\n", majorVersion, minorVersion);
	printf("GLSL Version : %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));

	onInitialization();

	glutDisplayFunc(onDisplay); // register event handlers
	glutIdleFunc(onIdle);
	glutKeyboardFunc(onKeyboard);
	glutKeyboardUpFunc(onKeyboardUp);
	glutReshapeFunc(onReshape);
	glutMouseFunc(onMouse);
	glutMainLoop();
	onExit();
	return 1;
}

