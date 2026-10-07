// Saltbound - music by mood (D84; see SaltMusic.h).

#include "World/SaltMusic.h"

#include "Boat/SaltBoat.h"
#include "Boat/SaltHullDamage.h"
#include "Components/AudioComponent.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Saltbound.h"
#include "Sound/SoundBase.h"
#include "World/SaltLightingRig.h"

namespace SaltMusicLocal
{
	const FName Calm(TEXT("Calm"));
	const FName Night(TEXT("Night"));
	const FName Drama(TEXT("Drama"));
	const FName Off(TEXT("off"));

	constexpr float Volume = 0.7f;
	constexpr float FadeIn = 4.f;
	constexpr float CrossFade = 5.f;
	constexpr float FadeAway = 8.f;
	constexpr float RestMin = 70.f;
	constexpr float RestMax = 160.f;

	int32 MoodIndex(FName Mood)
	{
		return Mood == Night ? 1 : Mood == Drama ? 2 : 0;
	}

	FAutoConsoleCommandWithWorldAndArgs CmdMusic(
		TEXT("sb.Music"),
		TEXT("D84: music by mood. sb.Music Calm|Night|Drama (hold a mood) | off (silence) | auto (follow the world) | next (end the rest) ")
		TEXT("| info."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			USaltMusicSubsystem* Music = World ? World->GetSubsystem<USaltMusicSubsystem>() : nullptr;
			if (!Music)
			{
				return;
			}
			const FString A = Args.Num() > 0 ? Args[0] : TEXT("info");
			if (A == TEXT("auto"))
			{
				Music->ForceMood(NAME_None);
			}
			else if (A == TEXT("next"))
			{
				Music->PlayNext();
			}
			else if (A != TEXT("info"))
			{
				Music->ForceMood(FName(*A));
			}
			UE_LOG(LogSaltbound, Display, TEXT("SB_MUSIC mood=%s wanted=%s track=%s playing=%d rest=%.0f s"), *Music->GetMood().ToString(),
				*Music->WantedMood().ToString(), *Music->GetTrack().ToString(), Music->IsPlaying() ? 1 : 0, Music->GetRestLeft());
		}));
}

const TArray<FString>& USaltMusicSubsystem::Tracks(FName InMood)
{
	using namespace SaltMusicLocal;
	// Imported by Tools/ue_scripts/import_music.py from Audio/Music/Background (the owner's files).
	static const TArray<FString> CalmTracks = {
		TEXT("/Game/Saltbound/Audio/Music/Calm/SW_Music_Calm_Ambient.SW_Music_Calm_Ambient"),
		TEXT("/Game/Saltbound/Audio/Music/Calm/SW_Music_Calm_Electronics.SW_Music_Calm_Electronics"),
	};
	static const TArray<FString> NightTracks = { TEXT("/Game/Saltbound/Audio/Music/Night/SW_Music_Night_Cinematic.SW_Music_Night_Cinematic") };
	static const TArray<FString> DramaTracks = { TEXT("/Game/Saltbound/Audio/Music/Drama/SW_Music_Drama_Documentary.SW_Music_Drama_Documentary") };
	return InMood == Night ? NightTracks : InMood == Drama ? DramaTracks : CalmTracks;
}

bool USaltMusicSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void USaltMusicSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	bBegun = true;
	Rng.GenerateNewSeed();
	// The first track a few seconds in: the world is heard first.
	RestUntil = 6.f;
}

void USaltMusicSubsystem::Deinitialize()
{
	for (UAudioComponent* C : { Playing.Get(), Fading.Get() })
	{
		if (C)
		{
			C->Stop();
		}
	}
	Super::Deinitialize();
}

TStatId USaltMusicSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(USaltMusicSubsystem, STATGROUP_Tickables);
}

FName USaltMusicSubsystem::WantedMood() const
{
	using namespace SaltMusicLocal;
	UWorld* World = GetWorld();
	const ASaltLightingRig* Rig = nullptr;
	for (TActorIterator<ASaltLightingRig> It(World); It; ++It)
	{
		Rig = *It;
		break;
	}
	// A storm, or the boat filling: drama.
	if (Rig && (Rig->CurrentSeaState == TEXT("Storm") || Rig->CurrentWeather == TEXT("Storm")))
	{
		return Drama;
	}
	for (TActorIterator<ASaltBoat> It(World); It; ++It)
	{
		if (It->GetHullDamage() && It->GetHullDamage()->GetFill() > 0.33f)
		{
			return Drama;
		}
	}
	if (Rig && Rig->GetNightAmount() > 0.5f)
	{
		return Night;
	}
	return Calm;
}

FName USaltMusicSubsystem::GetTrack() const
{
	return Playing && Playing->IsPlaying() && PlayingSound ? PlayingSound->GetFName() : NAME_None;
}

bool USaltMusicSubsystem::IsPlaying() const
{
	return Playing && Playing->IsPlaying();
}

void USaltMusicSubsystem::PlayNext()
{
	RestUntil = Clock;
	NextCheck = Clock;
}

void USaltMusicSubsystem::Start(FName InMood, float FadeSeconds)
{
	using namespace SaltMusicLocal;
	const TArray<FString>& List = Tracks(InMood);
	if (List.Num() == 0)
	{
		return;
	}
	// Another track of the mood than last time.
	int32& Last = LastIndex[MoodIndex(InMood)];
	int32 Index = Rng.RandRange(0, List.Num() - 1);
	if (List.Num() > 1 && Index == Last)
	{
		Index = (Index + 1) % List.Num();
	}
	Last = Index;
	USoundBase* Sound = LoadObject<USoundBase>(nullptr, *List[Index]);
	if (!Sound)
	{
		UE_LOG(LogSaltbound, Warning, TEXT("SB_MUSIC missing %s"), *List[Index]);
		Rest(30.f);
		return;
	}
	if (Fading)
	{
		Fading->Stop();
	}
	Fading = Playing;
	if (Fading)
	{
		Fading->FadeOut(FadeSeconds, 0.f);
	}
	Playing = UGameplayStatics::CreateSound2D(GetWorld(), Sound, Volume, 1.f, 0.f, nullptr, false, false);
	PlayingSound = Sound;
	Mood = InMood;
	if (Playing)
	{
		// It plays on in the pause menu.
		Playing->bIsUISound = true;
		Playing->FadeIn(FadeSeconds, Volume);
	}
	UE_LOG(LogSaltbound, Display, TEXT("SB_MUSIC start %s (%s), fade %.0f s"), *Sound->GetName(), *InMood.ToString(), FadeSeconds);
}

void USaltMusicSubsystem::Tick(float DeltaTime)
{
	using namespace SaltMusicLocal;
	Super::Tick(DeltaTime);
	if (!bBegun)
	{
		return;
	}
	Clock += DeltaTime;
	if (Clock < NextCheck)
	{
		return;
	}
	NextCheck = Clock + 1.f;
	const FName Want = !Forced.IsNone() ? Forced : WantedMood();
	if (Want == Off)
	{
		if (Playing && Playing->IsPlaying() && Mood != Off)
		{
			Playing->FadeOut(2.f, 0.f);
		}
		Mood = Off;
		return;
	}
	// A track that ended: a rest before the next.
	if (Playing && !Playing->IsPlaying())
	{
		Playing = nullptr;
		PlayingSound = nullptr;
		Rest(Rng.FRandRange(RestMin, RestMax));
	}
	if (Want != Mood)
	{
		if (Want == Night || Want == Drama)
		{
			// The night has come, or the storm: at once.
			Start(Want, IsPlaying() ? CrossFade : FadeIn);
			return;
		}
		// Back to calm: what plays fades away, a shorter rest, then the calm tracks.
		if (IsPlaying())
		{
			Playing->FadeOut(FadeAway, 0.f);
			Fading = Playing;
			Playing = nullptr;
			PlayingSound = nullptr;
			Rest(Rng.FRandRange(30.f, 50.f));
		}
		Mood = Want;
	}
	if (!IsPlaying() && Clock >= RestUntil)
	{
		Start(Want, FadeIn);
	}
}
