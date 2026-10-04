// Saltbound - the tools in the player's hands (D55, UI_Design.md section 4): compass, spyglass, lantern.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SaltHeldTools.generated.h"

class UCameraComponent;
class UMaterialInstanceDynamic;
class UPointLightComponent;
class USaltSwingComponent;
class UStaticMeshComponent;

/**
 * Shows what the player holds, in front of the camera (there are no arms yet):
 *  - the compass low on the right, tilted towards the eye, its needle swinging to world north (+X) and settling;
 *  - the spyglass: the view narrows to 20 degrees in 0.35 s (the HUD draws the round vignette and brass rim; mouse
 *    sensitivity follows the field of view);
 *  - the lantern hanging from the left hand, swinging as the player moves, a warm light (2100 K) inside.
 * A tool rises into view in 0.25 s and sinks out of it when put away. Meshes load on first use (never in constructors).
 */
UCLASS(ClassGroup = (Saltbound))
class SALTBOUND_API USaltHeldToolsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USaltHeldToolsComponent();

	/** The tool now in hand (None: empty hands). */
	void SetTool(FName Tool);
	FName GetTool() const { return Tool; }
	/** 0 lowered .. 1 in view (the shown tool). */
	float GetRaise() const { return Raise; }
	/** How far the spyglass has narrowed the view (0..1). */
	float GetSpyglass() const { return Spyglass; }
	/** The field of view now (degrees), and the look speed's scale that goes with it. */
	float GetFieldOfView() const;
	float GetLookScale() const;
	/** Where the compass needle's red end points (world, horizontal). */
	FVector GetNeedleNorth() const;

	static constexpr float SpyglassFOV = 20.f;

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	void EnsureParts();
	void UpdateCompass(float DeltaTime);

	UPROPERTY(Transient) TObjectPtr<UCameraComponent> Camera;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> CompassHand;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Compass;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Needle;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> LanternHand;
	UPROPERTY(Transient) TObjectPtr<USaltSwingComponent> LanternSwing;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Lantern;
	UPROPERTY(Transient) TObjectPtr<UPointLightComponent> LanternLight;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> LanternGlass;

	FName Tool;
	/** The tool being shown (it sinks out before the next one rises). */
	FName Shown;
	float Raise = 0.f;
	float Spyglass = 0.f;
	float NeedleYaw = 0.f;
	float NeedleRate = 0.f;
	float Flicker = 0.f;
	float BaseFOV = 80.f;
};
