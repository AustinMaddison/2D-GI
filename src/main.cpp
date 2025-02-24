#include "raylib.h"

#define RAYGUI_IMPLEMENTATION
#include "raygui.h"

#include "resource_dir.h"	

int main ()
{
	SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_HIGHDPI);
    
	InitWindow(1280, 720, "raygui - controls test suite");
    SetTargetFPS(60);

	SearchAndSetResourceDir("resources");


	Texture wabbit = LoadTexture("wabbit_alpha.png");
	
	bool showMessageBox = false;
	
	while (!WindowShouldClose())
	{
		
        BeginDrawing();
            ClearBackground(GetColor(GuiGetStyle(DEFAULT, BACKGROUND_COLOR)));

            if (GuiButton((Rectangle){ 24, 24, 120, 30 }, "#191#Show Message")) showMessageBox = true;

            if (showMessageBox)
            {
                int result = GuiMessageBox((Rectangle){ 85, 70, 250, 100 },
                    "#191#Message Box", "Hi! This is a message!", "Nice;Cool");

                if (result >= 0) showMessageBox = false;
            }

        EndDrawing();


		// BeginDrawing();

		// ClearBackground(BLACK);

		// DrawText("Hello Raylib", 200,200,20,WHITE);

		// DrawTexture(wabbit, 400, 200, WHITE);
		
		// EndDrawing();
	}

	UnloadTexture(wabbit);

	CloseWindow();
	return 0;
}
