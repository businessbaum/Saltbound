// Saltbound - runtime-built Enhanced Input actions and key mappings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "SaltInputConfig.generated.h"

class UInputAction;
class UInputMappingContext;

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
	/** Creates actions and the default mapping context. */
	void Build();

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

	UPROPERTY(Transient) TObjectPtr<UInputMappingContext> DefaultContext;
};
