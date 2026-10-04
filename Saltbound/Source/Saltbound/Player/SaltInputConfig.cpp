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
	Inventory = MakeAction(this, TEXT("IA_Inventory"), EInputActionValueType::Boolean);
	Map = MakeAction(this, TEXT("IA_Map"), EInputActionValueType::Boolean);
	ToolWheel = MakeAction(this, TEXT("IA_ToolWheel"), EInputActionValueType::Boolean);
	Tools.Reset();
	for (int32 I = 1; I <= 6; ++I)
	{
		Tools.Add(MakeAction(this, *FString::Printf(TEXT("IA_Tool%d"), I), EInputActionValueType::Boolean));
	}

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
	C->MapKey(Inventory, EKeys::Tab);
	C->MapKey(Inventory, EKeys::I);
	C->MapKey(Inventory, EKeys::Gamepad_Special_Left);
	C->MapKey(Map, EKeys::M);
	// D55: the tool wheel on Q (owner; Tab and I keep opening the satchel) and LB; keys 1 to 6 take a tool directly.
	C->MapKey(ToolWheel, EKeys::Q);
	C->MapKey(ToolWheel, EKeys::Gamepad_LeftShoulder);
	const FKey Digits[] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six };
	for (int32 I = 0; I < Tools.Num(); ++I)
	{
		C->MapKey(Tools[I], Digits[I]);
	}
}
