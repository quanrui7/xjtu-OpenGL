#pragma once
#include <OpenGL/gl.h>

#define WINDOW_POSITION_X 100
#define WINDOW_POSITION_Y 100
#define WINDOW_SIZE_WIDTH 800
#define WINDOW_SIZE_HEIGHT 800

struct Color { GLfloat r,g,b; };

#define MAP_BLOCK_EMPTY 0
#define MAP_BLOCK_CUBE  1
#define MAP_BLOCK_START 2
#define MAP_BLOCK_END   3

#define MAP_MAX 100

struct Map {
    GLint width;
    GLint height;
    GLint blocks[MAP_MAX][MAP_MAX];
};

const GLint MAP2_WIDTH = 10;
const GLint MAP2_HEIGHT = 10;
const GLint MAP2_BLOCKS[MAP2_WIDTH][MAP2_HEIGHT] = {
    {1,1,1,3,1,1,1,1,1,1},
    {1,1,1,0,1,1,1,1,1,1},
    {1,1,1,0,1,1,1,1,1,1},
    {1,1,1,0,0,0,0,1,1,1},
    {1,1,1,1,1,1,0,1,1,1},
    {1,0,0,0,0,1,0,0,0,1},
    {1,1,0,1,0,1,0,1,1,1},
    {1,0,0,1,0,1,0,0,0,1},
    {1,0,1,1,0,0,0,1,1,1},
    {1,2,1,1,1,1,1,1,1,1}
};

struct Player {
    GLint x;
    GLint y;
    GLint face;
};

#define PLAYER_FACE_UP 0
#define PLAYER_FACE_DOWN 1
#define PLAYER_FACE_LEFT 2
#define PLAYER_FACE_RIGHT 3

#define MAP_BLOCK_LENGTH 40
#define PLAYER_CUBE_SIZE (MAP_BLOCK_LENGTH / 2.4f)

typedef GLint ViewMode;
#define VIEW_MODE_FRIST_PERSON 1
#define VIEW_MODE_THIRD_PERSON 2
#define VIEW_MODE_GLOBAL 3

struct Camare {
    GLfloat position[3];
    GLfloat lookAt[3];
    GLfloat direction[3];
};

struct Texture {
    GLint width;
    GLint height;
    GLuint id;
};