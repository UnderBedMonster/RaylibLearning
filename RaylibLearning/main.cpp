#include "raylib.h"
#include "array"
#include "raymath.h"
#define MAX_BUILDINGS   100

typedef enum GameScreen { LOGO = 0, TITLE, GAMEPLAY, ENDING } GameScreen;
const int screenWidth = 800;
const int screenHeight = 1000;
const Vector2 playerStartpoint = {0,0};

Vector2 mesh = { 8,10 };

int spacing = 0;

class shape {
protected:
    const float cubeWidth = 100;
    const float cubeHeight = 100;

public:


    std::array<Rectangle,   4> arr{};

    virtual void moveshape(Vector2 direction) {
        for (size_t i = 0; i < arr.size(); i++)
        {

            arr[i].x += direction.x;
            arr[i].y += direction.y;
        }
    }
    virtual void rotateShape(Vector2 centerPoint, float angle) = 0;
        
    virtual void drawShape(Color color) {

        
        for (size_t i = 0; i < arr.size(); i++)
        {
          DrawRectangle(arr[i].x, arr[i].y, arr[i].width, arr[i].height, color);
          DrawRectangleLines(arr[i].x, arr[i].y, arr[i].width, arr[i].height, Color(RED));
        }
    }
    virtual float getWidth() = 0;

    virtual float getHeight() = 0;
};

class cubeShape : public shape{
public:
    cubeShape(Vector2 placePoint) {
        arr[0] = Rectangle{ placePoint.x, placePoint.y, placePoint.x + cubeWidth, placePoint.y + cubeHeight };
        arr[1] = Rectangle{ placePoint.x + cubeWidth, placePoint.y, cubeWidth, cubeHeight };
        arr[2] = Rectangle{ placePoint.x, placePoint.y + cubeHeight, cubeWidth, cubeHeight };
        arr[3] = Rectangle{ placePoint.x + cubeWidth, placePoint.y + cubeHeight, cubeWidth, cubeHeight };
    }

    virtual void rotateShape(Vector2 centerPoint, float angle) override {

        Vector2MoveTowards(Vector2{ arr[0].x, arr[0].y }, Vector2{}, UINT32_MAX);
        Vector2MoveTowards(Vector2{ arr[0].x, arr[0].y }, Vector2{}, UINT32_MAX);
        Vector2MoveTowards(Vector2{ arr[0].x, arr[0].y }, Vector2{}, UINT32_MAX);
        Vector2MoveTowards(Vector2{ arr[0].x, arr[0].y }, Vector2{}, UINT32_MAX);

     
        Vector2Rotate(centerPoint, angle);

    }

    virtual float getWidth() override{
        return arr[0].width + arr[1].width;
    }

    virtual float getHeight() override {
        return arr[0].height + arr[1].height;
    }

    ~cubeShape() = default;
};

int main(void)
{
    Camera2D camera = { 0 };

    camera.offset = Vector2{ screenWidth / 2.0f, screenHeight / 2.0f };
    camera.rotation = 0.0f;
    camera.zoom = 1.0f;

    cubeShape shape1 = { Vector2{ 0,0 } };
    
    InitWindow(screenWidth, screenHeight, "raylib [core] example - basic window");

    int framesCounter = 0;
    SetTargetFPS(60);

    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
        // Player movement
        if (IsKeyPressed(KEY_RIGHT)) shape1.moveshape(Vector2{ shape1.getWidth(),0});
        else if (IsKeyPressed(KEY_LEFT)) shape1.moveshape(Vector2{ -shape1.getWidth(),0 });

        if (IsKeyPressed(KEY_UP)) shape1.moveshape(Vector2{ 0,-shape1.getHeight() });
        else if (IsKeyPressed(KEY_DOWN)) shape1.moveshape(Vector2{ 0,shape1.getHeight() });

        // Camera target follows player
        camera.target = Vector2{ 400, 500 };

        // Camera rotation controls
        if (IsKeyDown(KEY_A)) camera.rotation--;
        else if (IsKeyDown(KEY_S)) camera.rotation++;

        // Limit camera rotation to 80 degrees (-40 to 40)
        if (camera.rotation > 40) camera.rotation = 40;
        else if (camera.rotation < -40) camera.rotation = -40;

        // Camera zoom controls
        camera.zoom += ((float)GetMouseWheelMove() * 0.05f);

        if (camera.zoom > 3.0f) camera.zoom = 3.0f;
        else if (camera.zoom < 0.1f) camera.zoom = 0.1f;

        // Camera reset (zoom and rotation)
        if (IsKeyPressed(KEY_R))
        {
            camera.zoom = 1.0f;
            camera.rotation = 0.0f;
        }

        BeginDrawing();

        ClearBackground(RAYWHITE);
      
        BeginMode2D(camera);

        shape1.drawShape(Color(BLACK));
        //drawMesh(mesh);

        EndMode2D();

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
