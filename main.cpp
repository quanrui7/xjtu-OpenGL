// main.cpp — REDESIGNED WITH CONSISTENT FORWARD DIRECTION
#include <GLUT/glut.h>
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "define.h"

#include <cstdio>
#include <cmath>
#include <chrono>
#include <cstring>

// ---------------- globals ----------------
Color white, gray, green;
Map mapData;

Player player;
Texture wallTex = {0,0,0}, completeTex = {0,0,0};

Camare cam1P, cam3P, camGlobal;
ViewMode viewMode = VIEW_MODE_FRIST_PERSON;

// 玩家角度（以度为单位，0=向上，90=向右，180=向下，270=向左）
float playerAngle = 0.0f;

// 移动插值
bool moving = false;
float t_move = 0.0f; // 0..1
float px_src = 0, py_src = 0, px_dst = 0, py_dst = 0;

float lastTime = 0;

// 游戏完成状态
bool gameCompleted = false;
float completeAlpha = 0.0f;
const float COMPLETE_FADE_SPEED = 1.5f;

// 键盘状态（用于平滑控制）
bool keyUp = false, keyLeft = false, keyRight = false;

// ---------------- time ----------------
double now() {
    using namespace std::chrono;
    return duration_cast<duration<double>>(steady_clock::now().time_since_epoch()).count();
}

// ---------------- load texture ----------------
bool loadTexture(Texture& tex, const char* file) {
    int ch;
    unsigned char* data = stbi_load(file, &tex.width, &tex.height, &ch, 3);
    if (!data) {
        printf("Texture not found: %s\n", file);
        return false;
    }

    glGenTextures(1, &tex.id);
    glBindTexture(GL_TEXTURE_2D, tex.id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, tex.width, tex.height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);

    stbi_image_free(data);
    printf("Loaded Texture %s (%d x %d)\n", file, tex.width, tex.height);
    return true;
}

// ---------------- map utilities ----------------
bool canMove(int tx, int ty) {
    if (tx < 0 || ty < 0 || tx >= mapData.height || ty >= mapData.width)
        return false;
    return mapData.blocks[tx][ty] != MAP_BLOCK_CUBE;
}

// ---------------- 根据角度更新玩家朝向 ----------------
void updatePlayerFaceFromAngle() {
    // 将角度标准化到0-360度
    float normalizedAngle = fmod(playerAngle, 360.0f);
    if (normalizedAngle < 0) normalizedAngle += 360.0f;
    
    // 根据角度确定朝向
    if ((normalizedAngle >= 315.0f && normalizedAngle <= 360.0f) || 
        (normalizedAngle >= 0.0f && normalizedAngle < 45.0f)) {
        player.face = PLAYER_FACE_UP;
    } else if (normalizedAngle >= 45.0f && normalizedAngle < 135.0f) {
        player.face = PLAYER_FACE_RIGHT;
    } else if (normalizedAngle >= 135.0f && normalizedAngle < 225.0f) {
        player.face = PLAYER_FACE_DOWN;
    } else {
        player.face = PLAYER_FACE_LEFT;
    }
}

// ---------------- 根据玩家朝向计算角度 ----------------
float getAngleFromPlayerFace() {
    switch (player.face) {
        case PLAYER_FACE_UP: return 0.0f;
        case PLAYER_FACE_RIGHT: return 90.0f;
        case PLAYER_FACE_DOWN: return 180.0f;
        case PLAYER_FACE_LEFT: return 270.0f;
        default: return 0.0f;
    }
}

// ---------------- camera update ----------------
void updateCameras(float vx, float vy) {
    // 将角度转换为弧度
    float rad = playerAngle * M_PI / 180.0f;
    
    // 计算方向向量（注意：在OpenGL中，Y轴可能向上，但在2D地图中我们需要调整）
    // 由于地图坐标系与OpenGL坐标系可能不同，这里需要调整
    // 假设玩家角度0度是看向+Y方向（向上）
    float dx = sin(rad);  // X方向的变化
    float dy = cos(rad);  // Y方向的变化
    
    // 第一人称摄像机
    cam1P.position[0] = vx;
    cam1P.position[1] = vy;
    cam1P.position[2] = PLAYER_CUBE_SIZE * 1.5f;
    
    // 看向玩家面对的方向
    cam1P.lookAt[0] = vx + dx * 20.0f;
    cam1P.lookAt[1] = vy + dy * 20.0f;
    cam1P.lookAt[2] = PLAYER_CUBE_SIZE * 1.0f;
    
    // 第三人称摄像机 - 从玩家后方
    cam3P.position[0] = vx - dx * 60.0f;
    cam3P.position[1] = vy - dy * 60.0f;
    cam3P.position[2] = PLAYER_CUBE_SIZE * 5.0f;
    
    cam3P.lookAt[0] = vx + dx * 10.0f;
    cam3P.lookAt[1] = vy + dy * 10.0f;
    cam3P.lookAt[2] = PLAYER_CUBE_SIZE * 1.0f;
    
    // 全局摄像机 - 俯视视角
    camGlobal.position[0] = vx;
    camGlobal.position[1] = vy - MAP_BLOCK_LENGTH * 10;
    camGlobal.position[2] = MAP_BLOCK_LENGTH * 15;
    
    camGlobal.lookAt[0] = vx;
    camGlobal.lookAt[1] = vy;
    camGlobal.lookAt[2] = 0;
}

// ---------------- 获取前进方向的目标位置 ----------------
void getForwardTarget(int& tx, int& ty) {
    tx = player.x;
    ty = player.y;
    
    // 根据当前朝向计算前进方向的目标位置
    switch (player.face) {
        case PLAYER_FACE_UP:    tx--; break;    // 向上（地图中行索引减小）
        case PLAYER_FACE_DOWN:  tx++; break;    // 向下（地图中行索引增加）
        case PLAYER_FACE_LEFT:  ty--; break;    // 向左（地图中列索引减小）
        case PLAYER_FACE_RIGHT: ty++; break;    // 向右（地图中列索引增加）
    }
}

// ---------------- init ----------------
void initGame() {
    white = {1,1,1};
    gray = {0.15f,0.18f,0.2f};
    green = {0.2f, 1.0f, 0.3f};

    mapData.width = MAP2_WIDTH;
    mapData.height = MAP2_HEIGHT;
    for (int i = 0; i < MAP2_WIDTH; i++)
        for (int j = 0; j < MAP2_HEIGHT; j++)
            mapData.blocks[i][j] = MAP2_BLOCKS[i][j];

    // 找到起始位置
    bool foundStart = false;
    for (int i = 0; i < mapData.height; i++) {
        for (int j = 0; j < mapData.width; j++) {
            if (mapData.blocks[i][j] == MAP_BLOCK_START) {
                player.x = i;
                player.y = j;
                foundStart = true;
                break;
            }
        }
        if (foundStart) break;
    }
    
    if (!foundStart) {
        printf("Warning: No start position found! Using default (0,0)\n");
        player.x = player.y = 0;
    }

    // 初始方向为向上
    player.face = PLAYER_FACE_UP;
    playerAngle = 0.0f;

    // 加载纹理
    loadTexture(wallTex, "wall.jpg");
    loadTexture(completeTex, "complete.jpg");

    // 初始世界坐标
    px_src = px_dst = player.y * MAP_BLOCK_LENGTH + MAP_BLOCK_LENGTH / 2.0f;
    py_src = py_dst = mapData.height * MAP_BLOCK_LENGTH - player.x * MAP_BLOCK_LENGTH - MAP_BLOCK_LENGTH / 2.0f;

    lastTime = now();
    updateCameras(px_src, py_src);

    // OpenGL 设置
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_NORMALIZE);
    glEnable(GL_COLOR_MATERIAL);
    
    // 光照设置
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    
    GLfloat light_ambient[] = { 0.6f, 0.6f, 0.6f, 1.0f };
    GLfloat light_diffuse[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    GLfloat light_specular[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    GLfloat light_position[] = { 500.0f, 500.0f, 1000.0f, 1.0f };
    
    glLightfv(GL_LIGHT0, GL_AMBIENT, light_ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, light_specular);
    glLightfv(GL_LIGHT0, GL_POSITION, light_position);
    
    // 材质属性
    GLfloat mat_specular[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    GLfloat mat_shininess[] = { 50.0f };
    glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
    glMaterialfv(GL_FRONT, GL_SHININESS, mat_shininess);
    
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    
    // 启用混合用于完成画面
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    printf("Maze Game Loaded Successfully.\n");
    printf("Controls: UP=Move Forward | LEFT/RIGHT=Turn | 1:F1 | 2:F2 | 3:F3 | ESC: quit\n");
    printf("Find the red exit block (block type 3) to complete the maze!\n");
    printf("NOTE: You can only move forward, not backward.\n");
}

// ---------------- draw cube ----------------
void drawCube(float x, float y, float z, float s, bool tex) {
    if (tex && wallTex.id) {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, wallTex.id);
    } else {
        glDisable(GL_TEXTURE_2D);
    }

    float x1 = x + s, y1 = y + s, z1 = z + s;

    // 顶部
    glBegin(GL_QUADS);
    glNormal3f(0, 0, 1);
    glTexCoord2f(0,0); glVertex3f(x, y, z1);
    glTexCoord2f(1,0); glVertex3f(x1, y, z1);
    glTexCoord2f(1,1); glVertex3f(x1, y1, z1);
    glTexCoord2f(0,1); glVertex3f(x, y1, z1);
    glEnd();

    // 底部
    glBegin(GL_QUADS);
    glNormal3f(0, 0, -1);
    glTexCoord2f(0,0); glVertex3f(x, y, z);
    glTexCoord2f(1,0); glVertex3f(x1, y, z);
    glTexCoord2f(1,1); glVertex3f(x1, y1, z);
    glTexCoord2f(0,1); glVertex3f(x, y1, z);
    glEnd();

    // 前面
    glBegin(GL_QUADS);
    glNormal3f(0, -1, 0);
    glTexCoord2f(0,0); glVertex3f(x, y, z);
    glTexCoord2f(1,0); glVertex3f(x1, y, z);
    glTexCoord2f(1,1); glVertex3f(x1, y, z1);
    glTexCoord2f(0,1); glVertex3f(x, y, z1);
    glEnd();

    // 后面
    glBegin(GL_QUADS);
    glNormal3f(0, 1, 0);
    glTexCoord2f(0,0); glVertex3f(x, y1, z);
    glTexCoord2f(1,0); glVertex3f(x1, y1, z);
    glTexCoord2f(1,1); glVertex3f(x1, y1, z1);
    glTexCoord2f(0,1); glVertex3f(x, y1, z1);
    glEnd();

    // 左面
    glBegin(GL_QUADS);
    glNormal3f(-1, 0, 0);
    glTexCoord2f(0,0); glVertex3f(x, y, z);
    glTexCoord2f(1,0); glVertex3f(x, y1, z);
    glTexCoord2f(1,1); glVertex3f(x, y1, z1);
    glTexCoord2f(0,1); glVertex3f(x, y, z1);
    glEnd();

    // 右面
    glBegin(GL_QUADS);
    glNormal3f(1, 0, 0);
    glTexCoord2f(0,0); glVertex3f(x1, y, z);
    glTexCoord2f(1,0); glVertex3f(x1, y1, z);
    glTexCoord2f(1,1); glVertex3f(x1, y1, z1);
    glTexCoord2f(0,1); glVertex3f(x1, y, z1);
    glEnd();

    glDisable(GL_TEXTURE_2D);
}

// ---------------- draw maze ----------------
void drawMaze() {
    for (int i = 0; i < mapData.height; i++) {
        for (int j = 0; j < mapData.width; j++) {
            float x = j * MAP_BLOCK_LENGTH;
            float y = mapData.height * MAP_BLOCK_LENGTH - (i+1) * MAP_BLOCK_LENGTH;
            
            if (mapData.blocks[i][j] == MAP_BLOCK_CUBE) {
                glColor3f(0.9, 0.9, 0.9);
                drawCube(x, y, 0, MAP_BLOCK_LENGTH, true);
            } else if (mapData.blocks[i][j] == MAP_BLOCK_END && !gameCompleted) {
                // 绘制终点方块（红色）
                glColor3f(1.0, 0.3, 0.3);
                drawCube(x, y, 0, MAP_BLOCK_LENGTH, false);
            }
        }
    }
}

// ---------------- HUD ----------------
void drawText(float x, float y, const char* s) {
    glRasterPos2f(x, y);
    while (*s) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *s++);
}

void HUD() {
    int W = WINDOW_SIZE_WIDTH, H = WINDOW_SIZE_HEIGHT;
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, W, 0, H);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);

    // 文本半透明背景
    glEnable(GL_BLEND);
    glColor4f(0.0f, 0.0f, 0.0f, 0.5f);
    glBegin(GL_QUADS);
    glVertex2f(0, H-25);
    glVertex2f(W, H-25);
    glVertex2f(W, H);
    glVertex2f(0, H);
    glEnd();
    glDisable(GL_BLEND);

    glColor3f(1,1,1);
    const char* vname =
        (viewMode==VIEW_MODE_FRIST_PERSON) ? "第一人称" :
        (viewMode==VIEW_MODE_THIRD_PERSON) ? "第三人称" : "全局视角";

    // 朝向文字
    const char* faceName = "未知";
    switch (player.face) {
        case PLAYER_FACE_UP: faceName = "上"; break;
        case PLAYER_FACE_DOWN: faceName = "下"; break;
        case PLAYER_FACE_LEFT: faceName = "左"; break;
        case PLAYER_FACE_RIGHT: faceName = "右"; break;
    }
    
    char buf[128];
    sprintf(buf, "视角:%s 位置:(%d,%d) 朝向:%s(%.0f°)", vname, player.x, player.y, faceName, playerAngle);
    drawText(10, H-20, buf);
    
    if (gameCompleted) {
        glColor3f(0.2f, 1.0f, 0.3f);
        drawText(10, H-40, "迷宫完成！按ESC退出游戏。");
    } else {
        glColor3f(0.8f, 0.8f, 0.8f);
        drawText(10, H-40, "前进：上箭头 | 转向：左/右箭头 | 切换视角：1/2/3");
    }

    glEnable(GL_LIGHTING);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

// ---------------- draw completion screen ----------------
void drawCompletionScreen() {
    if (completeAlpha <= 0.0f) return;
    
    int W = WINDOW_SIZE_WIDTH, H = WINDOW_SIZE_HEIGHT;
    
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, W, 0, H);
    
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    
    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    
    // 绘制半透明覆盖层
    glColor4f(0.0f, 0.0f, 0.0f, completeAlpha * 0.8f);
    glBegin(GL_QUADS);
    glVertex2f(0, 0);
    glVertex2f(W, 0);
    glVertex2f(W, H);
    glVertex2f(0, H);
    glEnd();
    
    // 绘制完成纹理（如果已加载）
    if (completeTex.id && completeAlpha >= 0.3f) {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, completeTex.id);
        
        float texWidth = 400.0f;
        float texHeight = 300.0f;
        float x = (W - texWidth) / 2.0f;
        float y = (H - texHeight) / 2.0f;
        
        glColor4f(1.0f, 1.0f, 1.0f, completeAlpha);
        glBegin(GL_QUADS);
        glTexCoord2f(0, 1); glVertex2f(x, y);
        glTexCoord2f(1, 1); glVertex2f(x + texWidth, y);
        glTexCoord2f(1, 0); glVertex2f(x + texWidth, y + texHeight);
        glTexCoord2f(0, 0); glVertex2f(x, y + texHeight);
        glEnd();
        
        glDisable(GL_TEXTURE_2D);
    }
    
    // 绘制祝贺文本
    if (completeAlpha >= 0.5f) {
        glColor4f(0.2f, 1.0f, 0.3f, completeAlpha);
        const char* congrats = "恭喜！";
        glRasterPos2f(W/2 - 30, H/2 + 180);
        for (int i = 0; congrats[i] != '\0'; i++) {
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, congrats[i]);
        }
        
        glColor4f(1.0f, 1.0f, 1.0f, completeAlpha);
        const char* message = "你找到了出口！";
        glRasterPos2f(W/2 - 50, H/2 + 150);
        for (int i = 0; message[i] != '\0'; i++) {
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, message[i]);
        }
        
        const char* exitMsg = "按ESC键退出游戏";
        glRasterPos2f(W/2 - 60, H/2 - 180);
        for (int i = 0; exitMsg[i] != '\0'; i++) {
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, exitMsg[i]);
        }
    }
    
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

// ---------------- display ----------------
void display() {
    glClearColor(gray.r, gray.g, gray.b, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(60, 1, 1, 5000);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    float vx = px_src + (px_dst - px_src) * t_move;
    float vy = py_src + (py_dst - py_src) * t_move;

    updateCameras(vx, vy);

    // 根据当前视角设置摄像机
    if (viewMode == VIEW_MODE_FRIST_PERSON) {
        gluLookAt(cam1P.position[0], cam1P.position[1], cam1P.position[2],
                  cam1P.lookAt[0], cam1P.lookAt[1], cam1P.lookAt[2], 0,0,1);
    } else if (viewMode == VIEW_MODE_THIRD_PERSON) {
        gluLookAt(cam3P.position[0], cam3P.position[1], cam3P.position[2],
                  cam3P.lookAt[0], cam3P.lookAt[1], cam3P.lookAt[2], 0,0,1);
    } else {
        gluLookAt(camGlobal.position[0], camGlobal.position[1], camGlobal.position[2],
                  camGlobal.lookAt[0], camGlobal.lookAt[1], camGlobal.lookAt[2], 0,0,1);
    }

    drawMaze();

    // 绘制玩家（绿色立方体）
    glPushMatrix();
    glTranslatef(vx, vy, PLAYER_CUBE_SIZE / 2.0f);
    glRotatef(playerAngle, 0, 0, 1);
    glTranslatef(-vx, -vy, -PLAYER_CUBE_SIZE/2.0f);
    glColor3f(0.2, 1.0, 0.3);
    drawCube(vx - PLAYER_CUBE_SIZE/2.0f,
             vy - PLAYER_CUBE_SIZE/2.0f,
             0, PLAYER_CUBE_SIZE, false);
    glPopMatrix();

    HUD();
    
    if (gameCompleted) {
        drawCompletionScreen();
    }

    glutSwapBuffers();
}

// ---------------- idle ----------------
void idle() {
    float t = now();
    float dt = t - lastTime;
    lastTime = t;

    // 检查游戏是否完成
    if (!gameCompleted && mapData.blocks[player.x][player.y] == MAP_BLOCK_END) {
        gameCompleted = true;
        printf("恭喜！你完成了迷宫！\n");
    }
    
    // 淡入完成画面
    if (gameCompleted && completeAlpha < 1.0f) {
        completeAlpha += dt * COMPLETE_FADE_SPEED;
        if (completeAlpha > 1.0f) completeAlpha = 1.0f;
    }

    // 移动插值
    if (moving) {
        t_move += dt * 3.0f; // 移动速度
        if (t_move >= 1.0f) {
            t_move = 1.0f;
            moving = false;
        }
    }

    glutPostRedisplay();
}

// ---------------- 按键处理 ----------------
void special(int key, int, int) {
    // 如果游戏已完成，只允许切换视角
    if (gameCompleted) {
        if (key == GLUT_KEY_F1) { viewMode = VIEW_MODE_FRIST_PERSON; return; }
        if (key == GLUT_KEY_F2) { viewMode = VIEW_MODE_THIRD_PERSON; return; }
        if (key == GLUT_KEY_F3) { viewMode = VIEW_MODE_GLOBAL; return; }
        return;
    }

    // 切换视角
    if (key == GLUT_KEY_F1) { viewMode = VIEW_MODE_FRIST_PERSON; return; }
    if (key == GLUT_KEY_F2) { viewMode = VIEW_MODE_THIRD_PERSON; return; }
    if (key == GLUT_KEY_F3) { viewMode = VIEW_MODE_GLOBAL; return; }

    if (moving) return;

    // 左转 - 直接90度
    if (key == GLUT_KEY_LEFT) {
        // 更新角度
        playerAngle -= 90.0f;
        
        // 确保角度在0-360度范围内
        playerAngle = fmod(playerAngle, 360.0f);
        if (playerAngle < 0) playerAngle += 360.0f;
        
        // 根据角度更新玩家朝向
        updatePlayerFaceFromAngle();
        
        // 立即重绘
        glutPostRedisplay();
        return;
    }
    
    // 右转 - 直接90度
    if (key == GLUT_KEY_RIGHT) {
        // 更新角度
        playerAngle += 90.0f;
        
        // 确保角度在0-360度范围内
        playerAngle = fmod(playerAngle, 360.0f);
        if (playerAngle < 0) playerAngle += 360.0f;
        
        // 根据角度更新玩家朝向
        updatePlayerFaceFromAngle();
        
        // 立即重绘
        glutPostRedisplay();
        return;
    }

    // 前进
    if (key == GLUT_KEY_UP) {
        // 获取前进方向的目标位置
        int tx, ty;
        getForwardTarget(tx, ty);

        if (canMove(tx, ty)) {
            // 设置移动插值
            px_src = player.y * MAP_BLOCK_LENGTH + MAP_BLOCK_LENGTH/2.0f;
            py_src = mapData.height * MAP_BLOCK_LENGTH - player.x * MAP_BLOCK_LENGTH - MAP_BLOCK_LENGTH/2.0f;

            px_dst = ty * MAP_BLOCK_LENGTH + MAP_BLOCK_LENGTH/2.0f;
            py_dst = mapData.height * MAP_BLOCK_LENGTH - tx * MAP_BLOCK_LENGTH - MAP_BLOCK_LENGTH/2.0f;

            t_move = 0.0f;
            moving = true;
            
            // 更新玩家位置
            player.x = tx;
            player.y = ty;
        }
    }
}

// ---------------- 处理按键释放 ----------------
void specialUp(int key, int, int) {
    if (key == GLUT_KEY_UP) keyUp = false;
    if (key == GLUT_KEY_LEFT) keyLeft = false;
    if (key == GLUT_KEY_RIGHT) keyRight = false;
}

void keyboard(unsigned char key, int, int) {
    if (key == 27) { exit(0); } // ESC
    if (key == '1') viewMode = VIEW_MODE_FRIST_PERSON;
    if (key == '2') viewMode = VIEW_MODE_THIRD_PERSON;
    if (key == '3') viewMode = VIEW_MODE_GLOBAL;
}

// ---------------- main ----------------
int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH | GLUT_MULTISAMPLE);
    glutInitWindowSize(WINDOW_SIZE_WIDTH, WINDOW_SIZE_HEIGHT);
    glutInitWindowPosition(WINDOW_POSITION_X, WINDOW_POSITION_Y);
    glutCreateWindow("迷宫游戏 - 仅能前进模式");

    initGame();

    glutDisplayFunc(display);
    glutIdleFunc(idle);
    glutSpecialFunc(special);
    glutSpecialUpFunc(specialUp);
    glutKeyboardFunc(keyboard);

    glutMainLoop();
    return 0;
}