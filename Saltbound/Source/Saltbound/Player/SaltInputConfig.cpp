// Saltbound - runtime-built Enhanced Input actions and key mappings.

#include "Player/SaltInputConfig.h"

#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"

namespace
{
	UInputAction* MakeAction(UObject* Outer, const TCHAR* Name, EInputActionValueType Type)
	{
		UInputAction* Action = NewObject<UInputAction>(Outer, Name);
		Action->ValueType = Type;
		return Action;
	}

	/** Maps a key onto a 2D axis: optionally swizzled (key drives Y) and/or negated. */
	void MapAxisKey(UInputMappingContext* Context, UInputAction* Action, const FKey& Key, bool bToY, bool bNegate)
	{
		FEnhancedActionKeyMapping& Mapping = Context->MapKey(Action, Key);
		if (bToY)
		{
			UInputModifierSwizzleAxis* Swizzle = NewObject<UInputModifierSwizzleAxis>(Context);
			Swizzle->Order = EInputAxisSwizzle::YXZ;
			Mapping.Modifiers.Add(Swizzle);
		}
		if (bNegate)
		{
			Mapping.Modifiers.Add(NewObject<UInputModifierNegate>(Context));
		}
	}
}

void USaltInputConfig::Build()
{
	Move = MakeAction(this, TEXT("IA_Move"), EInputActionValueType::Axis2D);
	LookMouse = MakeAction(this, TEXT("IA_LookMouse"), EInputActionValueType::Axis2D);
	LookGamepad = MakeAction(this, TEXT("IA_LookGamepad"), EInputActionValueType::Axis2D);
	Jump = MakeAction(this, TEXT("IA_Jump"), EInputActionValueType::Boolean);
	Sprint = MakeAction(this, TEXT("IA_Sprint"), EInputActionValueType::Boolean);
	Dive = MakeAction(this, TEXT("IA_Dive"), EInputActionValueType::Boolean);
	Interact = MakeAction(this, TEXT("IA_Interact"), EInputActionValueType::Boolean);
	Menu = MakeAction(this, TEXT("IA_Menu"), EInputActionValueType::Boolean);
	Menu->bTriggerWhenPaused = true;

	DefaultContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Default"));
	UInputMappingContext* C = DefaultContext;

	// Keyboard movement
	MapAxisKey(C, Move, EKeys::W, /*bToY*/ true, /*bNegate*/ false);
	MapAxisKey(C, Move, EKeys::S, true, true);
	MapAxisKey(C, Move, EKeys::D, false, false);
	MapAxisKey(C, Move, EKeys::A, false, true);
	MapAxisKey(C, Move, EKeys::Up, true, false);
	MapAxisKey(C, Move, EKeys::Down, true, true);
	MapAxisKey(C, Move, EKeys::Right, false, false);
	MapAxisKey(C, Move, EKeys::Left, false, true);
	// Gamepad movement
	C->MapKey(Move, EKeys::Gamepad_Left2D);

	C->MapKey(LookMouse, EKeys::Mouse2D);
	C->MapKey(LookGamepad, EKeys::Gamepad_Right2D);

	C->MapKey(Jump, EKeys::SpaceBar);
	C->MapKey(Jump, EKeys::Gamepad_FaceButton_Bottom);
	C->MapKey(Sprint, EKeys::LeftShift);
	C->MapKey(Sprint, EKeys::Gamepad_LeftThumbstick);
	C->MapKey(Dive, EKeys::LeftControl);
	C->MapKey(Dive, EKeys::C);
	C->MapKey(Dive, EKeys::Gamepad_FaceButton_Right);
	C->MapKey(Interact, EKeys::E);
	C->MapKey(Interact, EKeys::Gamepad_FaceButton_Left);
	C->MapKey(Menu, EKeys::Escape);
	C->MapKey(Menu, EKeys::F10);
	C->MapKey(Menu, EKeys::Gamepad_Special_Right);
}
