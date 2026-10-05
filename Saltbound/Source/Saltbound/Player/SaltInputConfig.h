// Saltbound - runtime-built Enhanced Input actions and key mappings.

#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "UObject/Object.h"
#include "SaltInputConfig.generated.h"

class UInputAction;
class UInputMappingContext;

/** An action whose keyboard key the player can change (Keys and buttons, D66). The gamepad button is shown, not changed. */
struct FSaltRebindable
{
	FName Id;
	FText Label;
	FKey Default;
	/** Other keys that always work too (the arrow keys, C for diving, I for the satchel); shown, not changed. */
	TArray<FKey> Fixed;
	FText Pad;
};

/**
 * Owns all player input actions and their default key mappings.
 * Built in code so every binding is reviewable in one place; no input assets are required.
 * Free movement and control stations share the same actions; the character routes them by mode.
 */
UCLASS()
class SALTBOUND_API USaltInputConfig : public UObject
{
	GENERATED_BODY()

public:
	/** Creates actions and the mapping context (with the player's keys). */
	void Build();
	/** Makes a new mapping context with the player's current keys (D66); the actions stay, so bindings stay valid. */
	void BuildMappings();

	/** The actions the player can rebind, in the order of the Keys and buttons screen. */
	static const TArray<FSaltRebindable>& Rebindables();
	static const FSaltRebindable* FindRebindable(FName Id);
	/** The key the player uses for it now (their choice or the default). */
	static FKey KeyOf(FName Id);
	/** The rebindable action (other than Except) that already uses Key as its key or a fixed one; None when free. */
	static FName UsedBy(const FKey& Key, FName Except);
	/** Keys that cannot be taken (they open the pause menu, or are the tools' digits). */
	static bool IsReserved(const FKey& Key);

	/** WASD / left stick. Axis2D: X = right, Y = forward. */
	UPROPERTY(Transient) TObjectPtr<UInputAction> Move;
	/** Mouse delta. Axis2D in mouse counts (already scaled by the engine axis config). */
	UPROPERTY(Transient) TObjectPtr<UInputAction> LookMouse;
	/** Right stick. Axis2D in [-1, 1], applied as a turn rate. */
	UPROPERTY(Transient) TObjectPtr<UInputAction> LookGamepad;
	UPROPERTY(Transient) TObjectPtr<UInputAction> Jump;
	UPROPERTY(Transient) TObjectPtr<UInputAction> Sprint;
	/** Swim downwards (held). Jump swims upwards / climbs out of the water. */
	UPROPERTY(Transient) TObjectPtr<UInputAction> Dive;
	/** Use the focused object; leaves a control station when at one. */
	UPROPERTY(Transient) TObjectPtr<UInputAction> Interact;
	/** Opens the pause menu (Esc; F10 also works in the editor, where Esc ends the play session). */
	UPROPERTY(Transient) TObjectPtr<UInputAction> Menu;
	/** Opens the satchel (Tab, I, gamepad Back). D50; the map left it (D57: the chart is on the mast). */
	UPROPERTY(Transient) TObjectPtr<UInputAction> Inventory;
	/** The tool wheel (D55): held open with Q or LB; letting go takes the tool. */
	UPROPERTY(Transient) TObjectPtr<UInputAction> ToolWheel;
	/** Keys 1 to 5: take a tool directly (the wheel's order). */
	UPROPERTY(Transient) TArray<TObjectPtr<UInputAction>> Tools;

	UPROPERTY(Transient) TObjectPtr<UInputMappingContext> DefaultContext;
};
