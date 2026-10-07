// Saltbound - people's lines (FH2, D81): who says what, and what the player answers.
//
// The lines are data (Content/Saltbound/Story/Lines.json, staged with the game): each speaker has a name, a wax seal and entry
// lines chosen by the story's state; each line has its words, what it does (gives tools, records beats) and up to three numbered
// answers plus the plain one (E). A person speaks on a torn half sheet low in the middle of the screen while the world keeps
// running (idea A of D75, USaltTalkWidget); a call in passing shows on a small scrap beside the speaker (idea B).

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "SaltTalk.generated.h"

class AActor;
class ASaltPlayerCharacter;
class ASaltPlayerController;
class UInputAction;
class UInputMappingContext;
class USaltTalkWidget;

/** An answer to a line: numbered (keys 1-3) or the plain one (E). */
struct FSaltTalkChoice
{
	FText Text;
	/** The line it leads to; None ends the talk. */
	FName Next;
	TArray<FString> If;
	bool bPlain = false;
};

struct FSaltTalkLine
{
	FName Id;
	FName Speaker;
	FText Text;
	/** Actions when the line is shown: beat:X, tool:X, item:X:N. */
	TArray<FString> Do;
	TArray<FSaltTalkChoice> Choices;
	/** For a line without choices: where E goes (None ends the talk). */
	FName Next;
	/** A call in passing (idea B): the beat or condition that starts it, how near the player must be (cm), how long it shows. */
	FString On;
	float Near = 0.f;
	float Seconds = 0.f;
};

struct FSaltTalkEntry
{
	TArray<FString> If;
	FName Line;
};

struct FSaltSpeaker
{
	FName Id;
	FText Name;
	/** The wax mark's glyph (T_UI_TalkSeal_<Seal>: Anchor, Wave, Island). */
	FName Seal;
	/** The hub point that stands for them until they have an actor of their own (ASaltHubPoint::PointId). */
	FName PointId;
	TArray<FSaltTalkEntry> Entries;
};

/** An answer as shown now: its key ("1".."3", "E") and where it is in the line's choices. */
struct FSaltShownChoice
{
	FString Key;
	FText Text;
	int32 Index = INDEX_NONE;
};

UCLASS()
class SALTBOUND_API USaltTalkSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/** Opens a talk with a speaker (by the id in Lines.json, e.g. "Keeper") on the line their entries choose for the story's state
	 *  now. Speaker is where they stand (calls are pinned beside it); none: their hub point. False when nobody by that id has a
	 *  line to say. Walking 4 m away from where it began ends it. */
	bool Begin(FName SpeakerId, ASaltPlayerCharacter* Player, const AActor* Speaker = nullptr);
	/** The plain answer (E): finishes the writing first, then takes the plain choice (or goes on, or ends). */
	void Answer();
	/** A numbered answer (0 for key 1). */
	void Choose(int32 Number);
	void End();
	bool IsTalking() const { return !LineId.IsNone(); }
	FName GetLineId() const { return LineId; }
	FName GetSpeakerId() const { return SpeakerId; }
	const FSaltTalkLine* GetLine() const { return Lines.Find(LineId); }
	/** The answers shown with the line now (conditions applied): the numbered ones, then the plain one. */
	const TArray<FSaltShownChoice>& GetShownChoices() const { return Shown; }
	USaltTalkWidget* GetWidget() const { return Widget; }
	/** Tests: the words are written at once and answers are taken without the moment they show marked. */
	void SetInstant(bool bOn) { bInstant = bOn; }
	/** Whether the talk's keys (E, 1-3) are mapped above play's now. */
	bool IsInputActive() const { return bInputActive; }

	/** Shows a call in passing (idea B) from Lines.json's calls, beside its speaker. */
	bool Call(FName CallId);
	/** Where a speaker stands (the Keeper's figure, D82); none: their hub point. */
	void SetSpeakerActor(FName Speaker, AActor* Actor);
	const AActor* FindSpeakerActor(FName Speaker) const;

	/** Reads Lines.json (again); returns what is wrong in it (lines that lead nowhere, unknown actions or conditions). */
	TArray<FString> Load();
	TArray<FString> Validate() const;
	const FSaltSpeaker* FindSpeaker(FName Id) const { return Speakers.Find(Id); }
	const TMap<FName, FSaltTalkLine>& GetLines() const { return Lines; }
	const TMap<FName, FSaltTalkLine>& GetCalls() const { return Calls; }
	/** Whether a condition (beat:X, tool:X, item:X, tear, night, day:N; ! negates) holds now. */
	bool Holds(const FString& Condition) const;
	bool HoldsAll(const TArray<FString>& Conditions) const;

private:
	void Show(FName Line);
	void Do(const FString& Action);
	void Mark(int32 ShownIndex);
	void Take(int32 ChoiceIndex);
	void OnBeat(FName Beat);
	void EnsureInput();
	void SetInputActive(bool bActive);
	void HandleAnswer();
	void HandleChoose(int32 Number);
	ASaltPlayerController* Controller() const;
	USaltTalkWidget* EnsureWidget();

	TMap<FName, FSaltSpeaker> Speakers;
	TMap<FName, FSaltTalkLine> Lines;
	TMap<FName, FSaltTalkLine> Calls;
	TMap<FName, TWeakObjectPtr<AActor>> SpeakerActors;
	TSet<FName> CallsMade;

	FName LineId;
	FName SpeakerId;
	TArray<FSaltShownChoice> Shown;
	TWeakObjectPtr<ASaltPlayerCharacter> Talker;
	FVector BeganAt = FVector::ZeroVector;
	float LineAge = 0.f;
	/** A chosen answer is marked on the sheet for a moment before the talk goes on (its index in the line's choices; none: go on). */
	bool bPending = false;
	int32 PendingChoice = INDEX_NONE;
	float PendingIn = 0.f;
	bool bInstant = false;

	UPROPERTY(Transient) TObjectPtr<USaltTalkWidget> Widget;
	UPROPERTY(Transient) TObjectPtr<UInputMappingContext> TalkContext;
	UPROPERTY(Transient) TObjectPtr<UInputAction> AnswerAction;
	UPROPERTY(Transient) TArray<TObjectPtr<UInputAction>> ChooseActions;
	TWeakObjectPtr<UObject> BoundInput;
	bool bInputActive = false;
	FDelegateHandle BeatHandle;
};
