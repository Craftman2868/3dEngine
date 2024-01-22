#include <stdio.h>  // printf
#include <sys/ioctl.h>  // ioctl TIOCGWINSZ, winsize (get terminal width and height)
#include <stdint.h>  // uint8_t

#include "debug.hpp"
#include "render.hpp"

int w = 20, h = 20;
char *pixels;
camera cam;


vec2 vecToScreen(vec2 v)
{
    return {(CHAR_RATIO * ((double) h) / ((double) w) * v.x + 1) * w / 2, (-v.y + 1) * h / 2};
}

triangle2 triangleToScreen(triangle2 tri)
{
    return {vecToScreen(tri.p1), vecToScreen(tri.p2), vecToScreen(tri.p3)};
}

void draw()
{
    printf("\033[H%s\n", pixels);
    printf("%7.3f  %7.3f  %7.3f  /  %6.3f  %6.3f", cam.pos.x, cam.pos.y, cam.pos.z, cam.pitch, cam.yaw);
    fflush(stdout);
}

void fill(char px)
{
    for (int i = 0; i < w*h; i++)
        pixels[i] = px;
}

void clear()
{
    fill(' ');
}

void putPixel(unsigned int x, unsigned int y, char px)
{
    pixels[y*w+x] = px;
}

void putPixel(vec2 pos, char px)
{
    putPixel(toInt(pos.x), toInt(pos.y), px);
}

double eq(vec2 p, vec2 a, vec2 b)
{
    return (a.x-p.x)*(b.y-p.y)-(a.y-p.y)*(b.x-p.x);
}

void putScreenTriangle(triangle2 tri, char px)
{
    int xmin = toInt(std::min(std::min(tri.p1.x, tri.p2.x), tri.p3.x));
    int xmax = toInt(std::max(std::max(tri.p1.x, tri.p2.x), tri.p3.x));
    int ymin = toInt(std::min(std::min(tri.p1.y, tri.p2.y), tri.p3.y));
    int ymax = toInt(std::max(std::max(tri.p1.y, tri.p2.y), tri.p3.y));

    for (int y = ymin; y < ymax; y++)
        if (0 <= y && y < h)
            for (int x = xmin; x < xmax; x++)
                if (0 <= x && x < w)
                {
                    vec2 pos = {(double) x, (double) y};
                    double w1 = eq(pos, tri.p3, tri.p1);
                    double w2 = eq(pos, tri.p1, tri.p2);
                    double w3 = eq(pos, tri.p2, tri.p3);

                    if ((w1 >= 0 && w2 >= 0 && w3 >= 0) || (-w1 >= 0 && -w2 >= 0 && -w3 >= 0))
                        putPixel(pos, px);
                }
}

void putTriangle2(triangle2 tri, char px)
{
    return putScreenTriangle(triangleToScreen(tri), px);
}

void putTriangle3(triangle3 tri, char px)
{
    return putTriangle2(projTriangle(cam, tri), px);
}

uint8_t clipTriangle3(triangle3 tri, vec3 planeNormal, triangle3 *clippedTriangles)
{
    bool p1in, p2in, p3in;
    // bool inverted;
    vec3 collision;
    vec3 zNear = addVec3(cam.pos, mulVec3(planeNormal, PLANE_DIST));

    debug_log("src/render.cpp:clipTriangle3\n");

    p1in = dotVec3(subVec3(zNear, tri.p1), planeNormal) < 0;
    p2in = dotVec3(subVec3(zNear, tri.p2), planeNormal) < 0;
    p3in = dotVec3(subVec3(zNear, tri.p3), planeNormal) < 0;

    // inverted = p1in && p3in;

    // inverted = false;

    debug_obj(tri);
    debug_obj(planeNormal);
    debug_log("dot points: %f, %f, %f\n", dotVec3(subVec3(zNear, tri.p1), planeNormal), dotVec3(subVec3(zNear, tri.p2), planeNormal), dotVec3(subVec3(zNear, tri.p3), planeNormal));

    if (!p1in && !p2in && !p3in)  // No point in
    {
        return 0;
    }
    else if (p1in && p2in && p3in)  // Every point in
    {
        clippedTriangles[0] = tri;
        return 1;
    }
    else if (p1in ^ p2in ^ p3in)  // 1 point in
    {
        // if (inverted)
        //     clippedTriangles[0] = {
        //         linePlaneCollision(planeNormal, zNear, (!p1in ? tri.p1 : tri.p2), (p1in ? tri.p1 : (p2in ? tri.p2 : tri.p3))),
        //         (p1in ? tri.p1 : (p2in ? tri.p2 : tri.p3)),
        //         linePlaneCollision(planeNormal, zNear, (!p3in ? tri.p3 : tri.p2), (p1in ? tri.p1 : (p2in ? tri.p2 : tri.p3))),
        //     };
        // else
        clippedTriangles[0] = {
            linePlaneCollision(planeNormal, zNear, (!p1in ? tri.p1 : tri.p2), (p1in ? tri.p1 : (p2in ? tri.p2 : tri.p3))),
            linePlaneCollision(planeNormal, zNear, (!p3in ? tri.p3 : tri.p2), (p1in ? tri.p1 : (p2in ? tri.p2 : tri.p3))),
            (p1in ? tri.p1 : (p2in ? tri.p2 : tri.p3)),
        };

        return 1;
    }
    else  // 2 points in
    {
        // if (inverted)
        // {
        //     collision = linePlaneCollision(planeNormal, zNear, (!p1in ? tri.p1 : (!p2in ? tri.p2 : tri.p3)), (p1in ? tri.p1 : tri.p2));
        //     clippedTriangles[0] = {
        //         linePlaneCollision(planeNormal, zNear, (!p1in ? tri.p1 : (!p2in ? tri.p2 : tri.p3)), (p3in ? tri.p3 : tri.p2)),
        //         (p3in ? tri.p3 : tri.p2),
        //         collision,
        //     };
        //     clippedTriangles[1] = {
        //         collision,
        //         (p3in ? tri.p3 : tri.p2),
        //         (p1in ? tri.p1 : tri.p2),
        //     };
        // }
        // else
        // {
        collision = linePlaneCollision(planeNormal, zNear, (!p1in ? tri.p1 : (!p2in ? tri.p2 : tri.p3)), (p3in ? tri.p3 : tri.p2));
        clippedTriangles[0] = {
            linePlaneCollision(planeNormal, zNear, (!p1in ? tri.p1 : (!p2in ? tri.p2 : tri.p3)), (p1in ? tri.p1 : tri.p2)),
            (p1in ? tri.p1 : tri.p2),
            collision,
        };
        clippedTriangles[1] = {
            collision,
            (p1in ? tri.p1 : tri.p2),
            (p3in ? tri.p3 : tri.p2),
        };
        // }

        return 2;
    }
}

bool isVisible(triangle3 tri)
{
    vec3 line1 = subVec3(tri.p2, tri.p1);
    vec3 line2 = subVec3(tri.p3, tri.p1);

    vec3 normal = crossProdVec3(line1, line2);

    return dotVec3(normal, subVec3(tri.p1, cam.pos)) > 0;
}

// char *meshChars = ".:;/ox*'@+-~";

void putMesh(mesh m)
{
    triangle3 clippedTriangles[2];
    uint8_t triN = 0;
    char ch = MESH_CHAR;

    debug_log("src/render.cpp:putMesh\n");
    debug_obj(cam);
    // debug_obj(-std::sin((cam).yaw) * std::cos((cam).pitch));
    // debug_obj( std::sin((cam).pitch));
    // debug_obj( std::cos((cam).yaw) * std::cos((cam).pitch));

    for (triangle3 tri : m)
    {
        // ch = meshChars[(uint8_t) (dotVec3(tri.p1, tri.p3) + dotVec3(tri.p2, tri.p2)) % sizeof (meshChars)];

        triN = clipTriangle3(tri, CAM_LOOK_AT_DIRECTION(cam), clippedTriangles);

        for (uint8_t i = 0; i < triN; i++)
            if (isVisible(clippedTriangles[i]))
                putTriangle3(triangle3Rotate(triangle3Translate(clippedTriangles[i], mulVec3(cam.pos, -1)), cam.pitch, cam.yaw), ch);
    }
}

camera *init()
{
    struct winsize ws;
    ioctl(0, TIOCGWINSZ, &ws);

    w = ws.ws_col;
    h = ws.ws_row - 1;

    cam = {{0, 0, 0}, 0, 0, 1};

    pixels = new char[w * h + 1];
    pixels[w * h] = 0;
    clear();

    printf("\033[?1049h\033[?25l\033[2J");
    draw();

    return &cam;
}

void quit()
{
    printf("\033[H\033[?1049l\033[?25h");
    fflush(stdout);

    delete[] pixels;
}
