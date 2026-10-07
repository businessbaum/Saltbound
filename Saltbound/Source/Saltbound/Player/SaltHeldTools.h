// Saltbound - the tools in the player's hands (D55, UI_Design.md section 4; D80: in the hands, used with the left mouse button).

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SaltHeldTools.generated.h"

class UCameraComponent;
class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UPointLightComponent;
class USaltArmsComponent;
class USaltSwingComponent;
class UStaticMeshComponent;

/**
 * Shows what the player holds, in the player's hands (D80: USaltArmsComponent reaches for each tool's grips, which are set on the
 * tool, so the hands stay on it whatever it does):
 *  - the compass lying in the right palm, its dial tilted to the eye, the needle swinging to world north (+X) and settling;
 *    used (left mouse button held), it comes up close to read;
 *  - the spyglass closed in the right fist, low; used, it goes up to the eye while its three drawtubes slide out, and the view
 *    narrows to 20 degrees (the HUD draws the round vignette and brass rim; the mouse slows with the field of view);
 *  - the lantern hanging from the left fist by its ring, swinging as the player moves, a warm light (2100 K) inside; used, it
 *    is lifted high and lights further;
 *  - the spade in both hands (the right on the D-handle, the left on the whipping), its blade ahead and down. Digging is a
 *    stroke: aim, drive the blade in (it goes into the ground), lever back, lift a spadeful, toss it aside; three strokes over a
 *    dig's hold, one at a click with nothing to dig.
 * A tool rises into view in 0.25 s and sinks out of it when put away. Meshes load on first use (never in constructors). All of it
 * is drawn as first-person primitives (it never pokes into walls), except the spade while its blade is in the ground.
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
	/** Lowers the tool in hand (and widens the spyglass's view) while the eyes are elsewhere (D58: a close look at the
	 *  chart); it comes back up afterwards. */
	void SetSuppressed(bool bInSuppressed) { bSuppressed = bInSuppressed; }
	/** D80: the left mouse button on the tool in hand (not on something in the world it is used on). */
	void SetUsing(bool bInUsing);
	bool IsUsing() const { return bUsing; }
	/** How far the use has gone (0 .. 1: the spyglass at the eye, the compass close, the lantern lifted). */
	float GetUse() const { return Use; }
	/** 0 lowered .. 1 in view (the shown tool). */
	float GetRaise() const { return Raise; }
	/** The tool being shown (None while the hands are empty or the tool is lowered for a close look). */
	FName GetShownTool() const { return Shown; }
	/** How far the spyglass has narrowed the view (0..1). */
	float GetSpyglass() const { return Spyglass; }
	/** The field of view now (degrees), and the look speed's scale that goes with it. */
	float GetFieldOfView() const;
	float GetLookScale() const;
	/** Where the compass needle's red end points (world, horizontal). */
	FVector GetNeedleNorth() const;
	/** D80: where the spade is in its stroke (0 .. 1; -1 at rest). */
	float GetStrokePhase() const { return StrokePhase; }
	/** D80: how far the body bends with the stroke: the eyes lowered (cm) and pitched (degrees, down negative). */
	void GetViewLean(float& OutDown, float& OutPitch) const;

	static constexpr float SpyglassFOV = 20.f;

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	void EnsureParts();
	void UpdateCompass(float DeltaTime);
	/** D80: the spade's pose in the view (its crossbar, cm) at a stroke's phase (-1: ready); ViewXf is the view's place in the
	 *  world (the stroke aims the blade at DigSpot). */
	FTransform ShovelPose(float Phase, const FTransform& ViewXf) const;
	/** D80: where the stroke goes in: the sand looked at, within the spade's reach in front of the feet. */
	bool FindDigSpot();
	/** D80: tells the arms where each hand goes for the shown tool. */
	void PlaceHands(float Up);
	/** D80: first-person drawing on or off for a part (sb.Arms.World draws everything in the world, to look at it from outside). */
	void SetFirstPerson(UPrimitiveComponent* Part, bool bFirstPerson);

	UPROPERTY(Transient) TObjectPtr<UCameraComponent> Camera;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> View;
	UPROPERTY(Transient) TObjectPtr<USaltArmsComponent> Arms;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> CompassHand;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Compass;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Needle;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> LanternHand;
	UPROPERTY(Transient) TObjectPtr<USaltSwingComponent> LanternSwing;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Lantern;
	UPROPERTY(Transient) TObjectPtr<UPointLightComponent> LanternLight;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> LanternGlass;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> ShovelHand;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Shovel;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> ShovelLoad;
	/** D80: the spadeful thrown off the blade at the toss, as clods flying in the world. */
	UPROPERTY(Transient) TObjectPtr<UInstancedStaticMeshComponent> Clods;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> SpyglassHand;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> SpyglassMesh;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> SpyglassDraws;

	FName Tool;
	bool bSuppressed = false;
	bool bUsing = false;
	/** The tool being shown (it sinks out before the next one rises). */
	FName Shown;
	float Raise = 0.f;
	float Use = 0.f;
	float Spyglass = 0.f;
	float NeedleYaw = 0.f;
	float NeedleRate = 0.f;
	float Flicker = 0.f;
	float BaseFOV = 80.f;
	/** D80: the spade's stroke: the phase now (-1 at rest), and a click's single stroke (seconds into it, -1 none). */
	float StrokePhase = -1.f;
	float StrokeTime = -1.f;
	/** The spade drawn in the world (its blade in the ground) rather than as a first-person part. */
	bool bShovelInWorld = false;
	/** The spot the stroke drives the blade into (world), the level direction to it from the player, and whether there is one. */
	FVector DigSpot = FVector::ZeroVector;
	FVector DigFwd = FVector::ForwardVector;
	bool bDigSpot = false;
	float LastStrokePhase = -1.f;
	struct FClod
	{
		FVector Pos;
		FVector Vel;
		float Size;
		float Life;
	};
	TArray<FClod> Flying;
	void ThrowClods();
	void UpdateClods(float DeltaTime);
	/** Whether everything is drawn in the world (sb.Arms.World). */
	bool bWorldDebug = false;
	/** A slow breath and the walk's sway of whatever is held (cm, view frame). */
	float Breath = 0.f;
};
