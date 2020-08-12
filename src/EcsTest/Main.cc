//------------------------------------------------------------------------------
//  Canoe.cc
//------------------------------------------------------------------------------
#include "Pre.h"
#include "Core/Main.h"
#include "Gfx/Gfx.h"
#include "Input/Input.h"
#include "Assets/Gfx/ShapeBuilder.h"
#include "glm/mat4x4.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "shaders.h"
#include "Movement.h"

using namespace Oryol;

// derived application class
class Canoe : public App {
public:
    AppState::Code OnRunning();
    AppState::Code OnInit();
    AppState::Code OnCleanup();    
private:
    MoveState GetInput();
};
OryolMain(Canoe);

//------------------------------------------------------------------------------
AppState::Code
Canoe::OnRunning() {
}

//------------------------------------------------------------------------------
AppState::Code
Canoe::OnInit() {
}

//------------------------------------------------------------------------------
AppState::Code
Canoe::OnCleanup() {

}


//----------------------------------------------------------------
MoveState
Canoe::GetInput() {
    MoveState state = MoveState::None;

    return state;
}