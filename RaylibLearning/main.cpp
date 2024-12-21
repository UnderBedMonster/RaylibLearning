#include <iostream>
#include "raylib.h"
#include "array"
#include "raymath.h"

#include "Tree.cpp"

#include <vector>
#include <list>

#include <chrono>
#include <thread>

#define MAX_BUILDINGS   100
#define MOVE_SPEED 0.1f
#define ROTATION_SENSITIVITY 0.005f
#define ZOOM_SENSITIVITY 0.1f
#define MAX_ZOOM 0.1f
#define MIN_ZOOM 10.0f
#define PAN_SPEED 0.05f

typedef enum GameScreen { LOGO = 0, TITLE, GAMEPLAY, ENDING } GameScreen;
const int screenWidth = 1000;
const int screenHeight = 800; 
const int blockWidth = screenWidth/5;
const int blockHeight = screenHeight/5;
const Vector2 playerStartpoint = {0,0};

const int ROW_DIRECTIONS[] = { -1, 1, 0, 0 };
const int COL_DIRECTIONS[] = { 0, 0, -1, 1 };

std::vector<std::vector<char>> labyrinth = {
    {'#', '#', '#', '#', '#', '#', '#', '#', '#'},
    {'#', ' ', ' ', '#', ' ', ' ', ' ', ' ', '#'},
    {'#', ' ', ' ', '#', ' ', '#', '#', ' ', '#'},
    {'#', ' ', '#', '#', ' ', '#', ' ', ' ', '#'},
    {'#', ' ', '#', ' ', ' ', '#', ' ', '#', '#'},
    {'#', ' ', ' ', ' ', '#', ' ', ' ', ' ', '#'},
    {'#', '#', '#', ' ', '#', '#', '#', ' ', '#'},
    {'#', ' ', ' ', ' ', ' ', ' ', ' ', ' ', '#'},
    {'#', '#', '#', '#', '#', '#', '#', '#', '#'}
};

// Helper function for vector scaling
Vector3 Vector3ScaleCustom(Vector3 v, float scalar) {
    v.x = scalar;
    v.y = scalar;
    v.z *= scalar;
    return v;
}

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

        Vector2MoveTowards(Vector2{ arr[0].x, arr[0].y }, Vector2{ -(cubeWidth / 2), (cubeHeight / 2) }, UINT32_MAX);
        Vector2MoveTowards(Vector2{ arr[1].x, arr[1].y }, Vector2{ 0, (cubeHeight / 2) }, UINT32_MAX);
        Vector2MoveTowards(Vector2{ arr[2].x, arr[2].y }, Vector2{ -(cubeWidth / 2), -(cubeHeight / 2) }, UINT32_MAX);
        Vector2MoveTowards(Vector2{ arr[3].x, arr[3].y }, Vector2{ 0, -(cubeHeight / 2) }, UINT32_MAX);

        
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

std::shared_ptr<TreeNode> constructTree(std::vector<std::vector<char>>& labyrinth, Vector2 start, Vector2 end) {
    
    if (start.x < 0 ||
        start.y < 0 ||
        start.x >= labyrinth.size() ||
        start.y >= labyrinth[0].size() ||
        labyrinth[start.x][start.y] == '#' ||
        labyrinth[start.x][start.y] == '*') {

        return nullptr;
    }
    
        auto node = std::make_shared<TreeNode>(Vector2{start.x, start.y});

    labyrinth[start.x][start.y] = '*';

    if (start.x == end.x && start.y == end.y) {
        return node;
    }

    for (int i = 0; i < 4; ++i) {
        float newX = start.x + ROW_DIRECTIONS[i];
        float newY = start.y + COL_DIRECTIONS[i];

        // Recursively build the path tree
        auto childNode = constructTree(labyrinth, Vector2{newX, newY}, end);
        if (childNode) {
            node->addChild(childNode);
        }
    }

    //labyrinth[start.getX()][start.getY()] = ' ';

    return !node->getChildren().empty() || (start.x == end.x && start.y == end.y) ? node : nullptr;
}

void printPath(std::shared_ptr<TreeNode> node) {
    if (!node) return;
    std::cout << "(" << node->getData().y << ", " << node->getData().x << ")";
    if (!node->getChildren().empty()) {
        std::cout << " -> ";
        printPath(node->getChildren().front()); // For simplicity, take the first path (DFS order)
    }
    std::cout << std::endl;
}

void printMap(std::vector<std::vector<char>> labyrinth) {
    for (size_t i = 0; i < labyrinth.size(); i++)
    {
        for (size_t j = 0; j < labyrinth[0].size(); j++)
        {
            DrawRectangleLines(j * blockWidth, i * blockHeight, blockWidth, blockHeight, Color(RED));
            if (labyrinth[i][j] == '#')
            {
                DrawRectangle(j*blockWidth,i*blockHeight,blockWidth,blockHeight,Color(BLACK));
            }
        }
    }
}

void printPathOnScreen(std::shared_ptr<TreeNode> node, Camera3D camera) {
    if (!node) return;
    DrawRectangle(node->getData().y * blockWidth, node->getData().x * blockHeight,blockWidth, blockHeight, Color(GREEN));
    if (!node->getChildren().empty()) {
        printPathOnScreen(node->getChildren().front(),camera); // For simplicity, take the first path (DFS order)
    }
}

int main(void)
{

    auto path = constructTree(labyrinth, Vector2{1, 1}, Vector2{ 1, 7 });
    
    


    // Define the camera
       

    cubeShape shape1 = { Vector2{ 0,0 } };
    
    InitWindow(screenWidth, screenHeight, "raylib [core] example - basic window");
    

    Camera3D camera = { 0 };
    camera.position = Vector3{ 0.0f, 2.0f, 4.0f };
    camera.target = Vector3{ 0.0f, 0.0f, 0.0f };
    camera.up = Vector3{ 0.0f, 1.0f, 0.0f };
    camera.fovy = 60.0f; // Wider initial FOV
    camera.projection = CAMERA_PERSPECTIVE;

    Vector3 cameraDirection = Vector3Subtract(camera.target, camera.position);
    cameraDirection = Vector3Normalize(cameraDirection);
    Vector3 cameraRight = Vector3CrossProduct(Vector3{ 0.0f, 1.0f, 0.0f }, cameraDirection);
    Vector3 cameraUp = Vector3CrossProduct(cameraDirection, cameraRight);

    // Movement velocity (for smooth acceleration/deceleration)
    Vector3 moveVelocity = { 0.0f, 0.0f, 0.0f };

    int framesCounter = 0;
    SetTargetFPS(60);

    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
        Vector3 moveDirection = { 0.0f, 0.0f, 0.0f };

        if (IsKeyDown(KEY_W)) moveDirection = Vector3Add(moveDirection, cameraDirection);
        if (IsKeyDown(KEY_S)) moveDirection = Vector3Subtract(moveDirection, cameraDirection);
        if (IsKeyDown(KEY_A)) moveDirection = Vector3Subtract(moveDirection, cameraRight);
        if (IsKeyDown(KEY_D)) moveDirection = Vector3Add(moveDirection, cameraRight);
        if (IsKeyDown(KEY_SPACE)) moveDirection.y += 1.0f;
        if (IsKeyDown(KEY_LEFT_SHIFT)) moveDirection.y -= 1.0f;

        // Normalize movement direction if necessary
        if (Vector3LengthSqr(moveDirection) > 0) {
            moveDirection = Vector3Normalize(moveDirection);
        }

        // Apply acceleration and deceleration
        if (Vector3LengthSqr(moveDirection) > 0) {
            moveVelocity = Vector3Add(moveVelocity, Vector3ScaleCustom(moveDirection, MOVE_SPEED));
        }
        else {
            // Deceleration (simple drag)
            moveVelocity = Vector3ScaleCustom(moveVelocity, 0.9f);
            if (Vector3LengthSqr(moveVelocity) < 0.0001f) {
                moveVelocity = Vector3{ 0.0f, 0.0f, 0.0f };
            }
        }

        // Apply velocity to camera position
        //camera.position = Vector3Add(camera.position, moveVelocity);

        // Rotation with mouse
        if (IsMouseButtonDown(MOUSE_RIGHT_BUTTON)) {
            Vector2 mouseDelta = GetMouseDelta();

            // Calculate rotation around Y-axis (yaw)
            float yawAngle = -mouseDelta.x * ROTATION_SENSITIVITY;
            Matrix rotationY = MatrixRotate(Vector3 { 0.0f, 1.0f, 0.0f }, yawAngle);
            cameraDirection = Vector3Transform(cameraDirection, rotationY);
            cameraRight = Vector3Transform(cameraRight, rotationY);
            cameraUp = Vector3Transform(cameraUp, rotationY);

            // Calculate rotation around the right vector (pitch)
            float pitchAngle = -mouseDelta.y * ROTATION_SENSITIVITY;
            Matrix rotationX = MatrixRotate(cameraRight, pitchAngle);
            Vector3 newUp = Vector3Transform(cameraUp, rotationX);

            // Check for up vector singularity (to avoid flipping)
            if (Vector3DotProduct(newUp, Vector3 { 0.0f, 1.0f, 0.0f }) > 0.001f) {
                cameraUp = newUp;
                cameraDirection = Vector3Transform(cameraDirection, rotationX);
            }
        }

        // Zoom with mouse wheel
        float wheelMove = GetMouseWheelMove();
        if (wheelMove != 0) {
            camera.fovy -= wheelMove * ZOOM_SENSITIVITY;
            if (camera.fovy < MAX_ZOOM) camera.fovy = MAX_ZOOM;
            if (camera.fovy > MIN_ZOOM) camera.fovy = MIN_ZOOM;
        }

        // Panning with middle mouse button
        if (IsMouseButtonDown(MOUSE_MIDDLE_BUTTON)) {
            Vector2 mouseDelta = GetMouseDelta();
            Vector3 pan = Vector3ScaleCustom(cameraRight, -mouseDelta.x * PAN_SPEED);
            pan = Vector3Add(pan, Vector3ScaleCustom(cameraUp, mouseDelta.y * PAN_SPEED));
            camera.position = Vector3Add(camera.position, pan);
        }

        // Update camera target (look-at point)
        camera.target = Vector3Add(camera.position, cameraDirection);
        // Camera target follows player
        //camera.target = Vector2{ 400, 500 };

        // Camera rotation controls
    

        

        // Camera zoom controls
        /*camera.zoom += ((float)GetMouseWheelMove() * 0.05f);

        if (camera.zoom > 3.0f) camera.zoom = 3.0f;
        else if (camera.zoom < 0.1f) camera.zoom = 0.1f;*/

        
        
        UpdateCamera(&camera, CAMERA_FREE);

        BeginDrawing();

        ClearBackground(RAYWHITE);
      
        BeginMode3D(camera);
       
        printMap(labyrinth);

        printPathOnScreen(path,camera);

        EndMode3D();

        EndDrawing();

    }

    CloseWindow();
    return 0;
}
