// Saltbound - runtime-built Enhanced Input actions and key mappings; the player's own keys (D66).

#include "Player/SaltInputConfig.h"

#include "Core/SaltGameUserSettings.h"
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

	const FName IdMoveForward(TEXT("MoveForward"));
	const FName IdMoveBack(TEXT("MoveBack"));
	const FName IdMoveLeft(TEXT("MoveLeft"));
	const FName IdMoveRight(TEXT("MoveRight"));
	const FName IdJump(TEXT("Jump"));
	const FName IdSprint(TEXT("Sprint"));
	const FName IdDive(TEXT("Dive"));
	const FName IdInteract(TEXT("Interact"));
	const FName IdSatchel(TEXT("Satchel"));
	const FName IdToolWheel(TEXT("ToolWheel"));
}

const TArray<FSaltRebindable>& USaltInputConfig::Rebindables()
{
	static const TArray<FSaltRebindable> List = {
		{ IdMoveForward, NSLOCTEXT("SaltKeys", "MoveForward", "Move forward"), EKeys::W, { EKeys::Up }, NSLOCTEXT("SaltKeys", "PadMove", "left stick") },
		{ IdMoveBack, NSLOCTEXT("SaltKeys", "MoveBack", "Move back"), EKeys::S, { EKeys::Down }, NSLOCTEXT("SaltKeys", "PadMove", "left stick") },
		{ IdMoveLeft, NSLOCTEXT("SaltKeys", "MoveLeft", "Move left"), EKeys::A, { EKeys::Left }, NSLOCTEXT("SaltKeys", "PadMove", "left stick") },
		{ IdMoveRight, NSLOCTEXT("SaltKeys", "MoveRight", "Move right"), EKeys::D, { EKeys::Right }, NSLOCTEXT("SaltKeys", "PadMove", "left stick") },
		{ IdJump, NSLOCTEXT("SaltKeys", "Jump", "Jump · swim up"), EKeys::SpaceBar, {}, NSLOCTEXT("SaltKeys", "PadA", "A") },
		{ IdSprint, NSLOCTEXT("SaltKeys", "Sprint", "Sprint"), EKeys::LeftShift, {}, NSLOCTEXT("SaltKeys", "PadL3", "left stick, pressed") },
		{ IdDive, NSLOCTEXT("SaltKeys", "Dive", "Dive"), EKeys::LeftControl, { EKeys::C }, NSLOCTEXT("SaltKeys", "PadB", "B") },
		{ IdInteract, NSLOCTEXT("SaltKeys", "Interact", "Use · interact"), EKeys::E, {}, NSLOCTEXT("SaltKeys", "PadX", "X") },
		{ IdSatchel, NSLOCTEXT("SaltKeys", "Satchel", "Satchel"), EKeys::Tab, { EKeys::I }, NSLOCTEXT("SaltKeys", "PadView", "View") },
		{ IdToolWheel, NSLOCTEXT("SaltKeys", "ToolWheel", "Tool wheel (hold)"), EKeys::Q, {}, NSLOCTEXT("SaltKeys", "PadLB", "LB") },
	};
	return List;
}

const FSaltRebindable* USaltInputConfig::FindRebindable(FName Id)
{
	return Rebindables().FindByPredicate([Id](const FSaltRebindable& R) { return R.Id == Id; });
}

FKey USaltInputConfig::KeyOf(FName Id)
{
	const FSaltRebindable* R = FindRebindable(Id);
	if (!R)
	{
		return EKeys::Invalid;
	}
	const USaltGameUserSettings* S = USaltGameUserSettings::Get();
	return S ? S->GetKey(Id, R->Default) : R->Default;
}

FName USaltInputConfig::UsedBy(const FKey& Key, FName Except)
{
	for (const FSaltRebindable& R : Rebindables())
	{
		if (R.Id != Except && (KeyOf(R.Id) == Key || R.Fixed.Contains(Key)))
		{
			return R.Id;
		}
	}
	return NAME_None;
}

bool USaltInputConfig::IsReserved(const FKey& Key)
{
	return Key == EKeys::Escape || Key == EKeys::F10 || Key == EKeys::One || Key == EKeys::Two || Key == EKeys::Three
		|| Key == EKeys::Four || Key == EKeys::Five || Key.IsMouseButton() || Key.IsGamepadKey() || Key.IsAxis1D() || Key.IsAxis2D();
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
	ToolWheel = MakeAction(this, TEXT("IA_ToolWheel"), EInputActionValueType::Boolean);
	Tools.Reset();
	for (int32 I = 1; I <= 5; ++I)
	{
		Tools.Add(MakeAction(this, *FString::Printf(TEXT("IA_Tool%d"), I), EInputActionValueType::Boolean));
	}
	BuildMappings();
}

void USaltInputConfig::BuildMappings()
{
	DefaultContext = NewObject<UInputMappingContext>(this, MakeUniqueObjectName(this, UInputMappingContext::StaticClass(), TEXT("IMC_Default")));
	UInputMappingContext* C = DefaultContext;

	// Keyboard movement: the player's keys (D66), the arrow keys always.
	MapAxisKey(C, Move, KeyOf(IdMoveForward), /*bToY*/ true, /*bNegate*/ false);
	MapAxisKey(C, Move, KeyOf(IdMoveBack), true, true);
	MapAxisKey(C, Move, KeyOf(IdMoveRight), false, false);
	MapAxisKey(C, Move, KeyOf(IdMoveLeft), false, true);
	MapAxisKey(C, Move, EKeys::Up, true, false);
	MapAxisKey(C, Move, EKeys::Down, true, true);
	MapAxisKey(C, Move, EKeys::Right, false, false);
	MapAxisKey(C, Move, EKeys::Left, false, true);
	// Gamepad movement
	C->MapKey(Move, EKeys::Gamepad_Left2D);

	C->MapKey(LookMouse, EKeys::Mouse2D);
	C->MapKey(LookGamepad, EKeys::Gamepad_Right2D);

	C->MapKey(Jump, KeyOf(IdJump));
	C->MapKey(Jump, EKeys::Gamepad_FaceButton_Bottom);
	C->MapKey(Sprint, KeyOf(IdSprint));
	C->MapKey(Sprint, EKeys::Gamepad_LeftThumbstick);
	C->MapKey(Dive, KeyOf(IdDive));
	C->MapKey(Dive, EKeys::C);
	C->MapKey(Dive, EKeys::Gamepad_FaceButton_Right);
	C->MapKey(Interact, KeyOf(IdInteract));
	C->MapKey(Interact, EKeys::Gamepad_FaceButton_Left);
	C->MapKey(Menu, EKeys::Escape);
	C->MapKey(Menu, EKeys::F10);
	C->MapKey(Menu, EKeys::Gamepad_Special_Right);
	C->MapKey(Inventory, KeyOf(IdSatchel));
	C->MapKey(Inventory, EKeys::I);
	C->MapKey(Inventory, EKeys::Gamepad_Special_Left);
	// D55: the tool wheel on Q (owner; Tab and I keep opening the satchel) and LB; keys 1 to 5 take a tool directly.
	C->MapKey(ToolWheel, KeyOf(IdToolWheel));
	C->MapKey(ToolWheel, EKeys::Gamepad_LeftShoulder);
	const FKey Digits[] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five };
	for (int32 I = 0; I < Tools.Num(); ++I)
	{
		C->MapKey(Tools[I], Digits[I]);
	}
}
