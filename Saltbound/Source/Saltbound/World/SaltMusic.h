// Saltbound - music by mood (D84): the owner's tracks, chosen by what the world is doing.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "SaltMusic.generated.h"

class UAudioComponent;
class USoundBase;

/**
 * D84 (the owner added four tracks: Audio/Music/Background/chill, dark, drama): music that follows the mood of the moment
 * (GameConcept: "music by mood"):
 *  - Calm (by day, exploring, the menus): the chill tracks, one at a time;
 *  - Night (from dusk, the night amount over a half): the dark track; it starts at once when the night comes (the dusk that calls
 *    the player out to the Spire, FirstHour 0:23);
 *  - Drama (a storm at sea, or the boat a third full of water): the drama track, at once.
 * A track fades in (4 s) and plays to its end; then the music rests 70-160 s before the next (rare enough to stay special, as in
 * the sailing games the owner likes). A change to Night or Drama crossfades (5 s) at once; a change back to Calm waits for the
 * rest. Every track is in the SC_Music class, so the settings' Music volume applies. sb.Music <Calm|Night|Drama|off|auto|next|info>.
 */
UCLASS()
class SALTBOUND_API USaltMusicSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/** The mood the world asks for now (Calm, Night, Drama). */
	FName WantedMood() const;
	/** Holds a mood (sb.Music; tests), off (silence) or None (follow the world again). */
	void ForceMood(FName InMood) { Forced = InMood; }
	/** Ends the rest: the next track starts now. */
	void PlayNext();
	FName GetMood() const { return Mood; }
	/** The track playing (its asset name; None in a rest), and the seconds left of the rest. */
	FName GetTrack() const;
	float GetRestLeft() const { return FMath::Max(RestUntil - Clock, 0.f); }
	bool IsPlaying() const;
	/** The tracks of a mood (their asset paths). */
	static const TArray<FString>& Tracks(FName Mood);

private:
	void Start(FName InMood, float FadeSeconds);
	void Rest(float Seconds) { RestUntil = Clock + Seconds; }

	UPROPERTY(Transient) TObjectPtr<UAudioComponent> Playing;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> Fading;
	UPROPERTY(Transient) TObjectPtr<USoundBase> PlayingSound;

	FName Mood;
	FName Forced;
	float Clock = 0.f;
	float RestUntil = 6.f;
	float NextCheck = 0.f;
	int32 LastIndex[3] = { -1, -1, -1 };
	bool bBegun = false;
	FRandomStream Rng;
};
