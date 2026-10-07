// Saltbound - people's lines (FH2, D81).

#include "World/SaltTalk.h"

#include "Boat/SaltBoat.h"
#include "Boat/SaltHullDamage.h"
#include "Core/SaltSaveSubsystem.h"
#include "Dom/JsonObject.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Hub/SaltHubPoint.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Player/SaltInputConfig.h"
#include "Player/SaltInventoryComponent.h"
#include "Player/SaltPlayerCharacter.h"
#include "Player/SaltPlayerController.h"
#include "Saltbound.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UI/SaltTalkWidget.h"
#include "World/SaltLightingRig.h"
#include "World/SaltPrologue.h"

namespace
{
	/** Walking this far (cm) from where a talk began ends it. */
	constexpr double TalkReach = 400.0;
	/** A press that may still be bouncing from opening the talk (s). */
	constexpr float AnswerGuard = 0.25f;
	/** How long a taken answer shows marked before the talk goes on (s). */
	constexpr float MarkTime = 0.2f;
	/** Above the input of play (the default context is 0), so E and 1-3 answer instead of using things and taking tools. */
	constexpr int32 TalkPriority = 100;
	/** The speaker's head above where they stand (cm): calls are pinned beside it. */
	constexpr double HeadUp = 170.0;

	/** Lines.json lives with the content and is staged with the game (DefaultGame.ini). */
	FString LinesPath()
	{
		return FPaths::ProjectContentDir() / TEXT("Saltbound/Story/Lines.json");
	}

	TArray<FString> Strings(const TSharedPtr<FJsonObject>& O, const TCHAR* Field)
	{
		TArray<FString> Out;
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		if (O.IsValid() && O->TryGetArrayField(Field, A))
		{
			for (const TSharedPtr<FJsonValue>& V : *A)
			{
				FString S;
				if (V.IsValid() && V->TryGetString(S) && !S.IsEmpty())
				{
					Out.Add(S);
				}
			}
		}
		return Out;
	}

	FName NameField(const TSharedPtr<FJsonObject>& O, const TCHAR* Field)
	{
		FString S;
		return O.IsValid() && O->TryGetStringField(Field, S) && !S.IsEmpty() ? FName(*S) : NAME_None;
	}

	FSaltTalkLine ReadLine(FName Id, const TSharedPtr<FJsonObject>& O)
	{
		FSaltTalkLine L;
		L.Id = Id;
		L.Speaker = NameField(O, TEXT("speaker"));
		L.Text = FText::FromString(O->GetStringField(TEXT("text")));
		L.Do = Strings(O, TEXT("do"));
		L.Next = NameField(O, TEXT("next"));
		O->TryGetStringField(TEXT("on"), L.On);
		double Num = 0.0;
		if (O->TryGetNumberField(TEXT("near"), Num))
		{
			L.Near = static_cast<float>(Num);
		}
		if (O->TryGetNumberField(TEXT("seconds"), Num))
		{
			L.Seconds = static_cast<float>(Num);
		}
		const TArray<TSharedPtr<FJsonValue>>* Choices = nullptr;
		if (O->TryGetArrayField(TEXT("choices"), Choices))
		{
			for (const TSharedPtr<FJsonValue>& V : *Choices)
			{
				const TSharedPtr<FJsonObject> C = V.IsValid() ? V->AsObject() : nullptr;
				if (!C.IsValid())
				{
					continue;
				}
				FSaltTalkChoice Choice;
				Choice.Text = FText::FromString(C->GetStringField(TEXT("text")));
				Choice.Next = NameField(C, TEXT("next"));
				Choice.If = Strings(C, TEXT("if"));
				C->TryGetBoolField(TEXT("plain"), Choice.bPlain);
				L.Choices.Add(MoveTemp(Choice));
			}
		}
		return L;
	}

	bool KnownCondition(FString C)
	{
		C.RemoveFromStart(TEXT("!"));
		return C.StartsWith(TEXT("beat:")) || C.StartsWith(TEXT("tool:")) || C.StartsWith(TEXT("item:")) || C.StartsWith(TEXT("day:"))
			|| C == TEXT("tear") || C == TEXT("night");
	}

	bool KnownAction(const FString& A)
	{
		return A.StartsWith(TEXT("beat:")) || A.StartsWith(TEXT("tool:")) || A.StartsWith(TEXT("item:"));
	}

	template <typename T>
	T* FirstActor(UWorld* World)
	{
		TActorIterator<T> It(World);
		return It ? *It : nullptr;
	}

	FAutoConsoleCommandWithWorldAndArgs CmdTalk(
		TEXT("sb.Talk"),
		TEXT("FH2 (D81): talk to a speaker (sb.Talk Keeper), or 'answer', 'choose N' (1-3), 'end', 'call <CallId>'."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			USaltTalkSubsystem* Talk = World ? World->GetSubsystem<USaltTalkSubsystem>() : nullptr;
			ASaltPlayerCharacter* Player = World ? Cast<ASaltPlayerCharacter>(UGameplayStatics::GetPlayerPawn(World, 0)) : nullptr;
			if (!Talk || Args.Num() == 0)
			{
				return;
			}
			if (Args[0] == TEXT("answer"))
			{
				Talk->Answer();
			}
			else if (Args[0] == TEXT("choose") && Args.Num() > 1)
			{
				Talk->Choose(FCString::Atoi(*Args[1]) - 1);
			}
			else if (Args[0] == TEXT("end"))
			{
				Talk->End();
			}
			else if (Args[0] == TEXT("call") && Args.Num() > 1)
			{
				Talk->Call(FName(*Args[1]));
			}
			else
			{
				Talk->Begin(FName(*Args[0]), Player);
			}
		}));

	FAutoConsoleCommandWithWorld CmdTalkReload(
		TEXT("sb.Talk.Reload"),
		TEXT("FH2 (D81): read Content/Saltbound/Story/Lines.json again and print what is wrong in it."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (USaltTalkSubsystem* Talk = World ? World->GetSubsystem<USaltTalkSubsystem>() : nullptr)
			{
				Talk->Load();
			}
		}));
}

bool USaltTalkSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void USaltTalkSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	Load();
	if (USaltSaveSubsystem* Saves = USaltSaveSubsystem::Get(&InWorld))
	{
		BeatHandle = Saves->OnStoryBeat.AddUObject(this, &USaltTalkSubsystem::OnBeat);
	}
}

void USaltTalkSubsystem::Deinitialize()
{
	if (USaltSaveSubsystem* Saves = BeatHandle.IsValid() ? USaltSaveSubsystem::Get(GetWorld()) : nullptr)
	{
		Saves->OnStoryBeat.Remove(BeatHandle);
	}
	BeatHandle.Reset();
	if (Widget)
	{
		Widget->RemoveFromParent();
		Widget = nullptr;
	}
	Super::Deinitialize();
}

TStatId USaltTalkSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(USaltTalkSubsystem, STATGROUP_Tickables);
}

TArray<FString> USaltTalkSubsystem::Load()
{
	Speakers.Reset();
	Lines.Reset();
	Calls.Reset();
	CallsMade.Reset();
	TArray<FString> Problems;
	FString Json;
	TSharedPtr<FJsonObject> RootObject;
	if (!FFileHelper::LoadFileToString(Json, *LinesPath()))
	{
		Problems.Add(FString::Printf(TEXT("cannot read %s"), *LinesPath()));
	}
	else if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), RootObject) || !RootObject.IsValid())
	{
		Problems.Add(TEXT("Lines.json is not valid JSON"));
	}
	if (RootObject.IsValid())
	{
		const TSharedPtr<FJsonObject>* Section = nullptr;
		if (RootObject->TryGetObjectField(TEXT("speakers"), Section))
		{
			for (const TPair<FString, TSharedPtr<FJsonValue>>& It : (*Section)->Values)
			{
				const TSharedPtr<FJsonObject> O = It.Value.IsValid() ? It.Value->AsObject() : nullptr;
				if (!O.IsValid())
				{
					continue;
				}
				FSaltSpeaker S;
				S.Id = FName(*It.Key);
				S.Name = FText::FromString(O->GetStringField(TEXT("name")));
				S.Seal = NameField(O, TEXT("seal"));
				S.PointId = NameField(O, TEXT("point"));
				const TArray<TSharedPtr<FJsonValue>>* Entries = nullptr;
				if (O->TryGetArrayField(TEXT("entries"), Entries))
				{
					for (const TSharedPtr<FJsonValue>& V : *Entries)
					{
						const TSharedPtr<FJsonObject> E = V.IsValid() ? V->AsObject() : nullptr;
						if (E.IsValid())
						{
							S.Entries.Add({ Strings(E, TEXT("if")), NameField(E, TEXT("line")) });
						}
					}
				}
				Speakers.Add(S.Id, MoveTemp(S));
			}
		}
		for (const TCHAR* Kind : { TEXT("lines"), TEXT("calls") })
		{
			if (RootObject->TryGetObjectField(Kind, Section))
			{
				TMap<FName, FSaltTalkLine>& Into = FCString::Strcmp(Kind, TEXT("lines")) == 0 ? Lines : Calls;
				for (const TPair<FString, TSharedPtr<FJsonValue>>& It : (*Section)->Values)
				{
					const TSharedPtr<FJsonObject> O = It.Value.IsValid() ? It.Value->AsObject() : nullptr;
					if (O.IsValid())
					{
						Into.Add(FName(*It.Key), ReadLine(FName(*It.Key), O));
					}
				}
			}
		}
	}
	Problems.Append(Validate());
	UE_LOG(LogSaltbound, Display, TEXT("SB_TALK loaded speakers=%d lines=%d calls=%d problems=%d"), Speakers.Num(), Lines.Num(), Calls.Num(),
		Problems.Num());
	for (const FString& P : Problems)
	{
		UE_LOG(LogSaltbound, Warning, TEXT("SB_TALK problem: %s"), *P);
	}
	return Problems;
}

TArray<FString> USaltTalkSubsystem::Validate() const
{
	TArray<FString> Out;
	auto CheckIf = [&Out](const FString& Where, const TArray<FString>& Conditions)
	{
		for (const FString& C : Conditions)
		{
			if (!KnownCondition(C))
			{
				Out.Add(FString::Printf(TEXT("%s: unknown condition '%s'"), *Where, *C));
			}
		}
	};
	for (const TPair<FName, FSaltSpeaker>& S : Speakers)
	{
		if (S.Value.Entries.Num() == 0)
		{
			Out.Add(FString::Printf(TEXT("speaker %s has no entries"), *S.Key.ToString()));
		}
		for (const FSaltTalkEntry& E : S.Value.Entries)
		{
			CheckIf(S.Key.ToString(), E.If);
			if (!Lines.Contains(E.Line))
			{
				Out.Add(FString::Printf(TEXT("speaker %s: entry line '%s' does not exist"), *S.Key.ToString(), *E.Line.ToString()));
			}
		}
	}
	for (const TPair<FName, FSaltTalkLine>& L : Lines)
	{
		const FString Id = L.Key.ToString();
		if (!Speakers.Contains(L.Value.Speaker))
		{
			Out.Add(FString::Printf(TEXT("line %s: unknown speaker '%s'"), *Id, *L.Value.Speaker.ToString()));
		}
		if (L.Value.Text.IsEmpty())
		{
			Out.Add(FString::Printf(TEXT("line %s has no text"), *Id));
		}
		for (const FString& A : L.Value.Do)
		{
			if (!KnownAction(A))
			{
				Out.Add(FString::Printf(TEXT("line %s: unknown action '%s'"), *Id, *A));
			}
		}
		int32 Numbered = 0;
		int32 Plain = 0;
		for (const FSaltTalkChoice& C : L.Value.Choices)
		{
			(C.bPlain ? Plain : Numbered)++;
			CheckIf(Id, C.If);
			if (!C.Next.IsNone() && !Lines.Contains(C.Next))
			{
				Out.Add(FString::Printf(TEXT("line %s: answer leads to '%s', which does not exist"), *Id, *C.Next.ToString()));
			}
		}
		if (Numbered > 3 || Plain > 1)
		{
			Out.Add(FString::Printf(TEXT("line %s: %d numbered and %d plain answers (at most 3 and 1)"), *Id, Numbered, Plain));
		}
		if (!L.Value.Next.IsNone() && !Lines.Contains(L.Value.Next))
		{
			Out.Add(FString::Printf(TEXT("line %s: next '%s' does not exist"), *Id, *L.Value.Next.ToString()));
		}
	}
	for (const TPair<FName, FSaltTalkLine>& C : Calls)
	{
		if (!Speakers.Contains(C.Value.Speaker))
		{
			Out.Add(FString::Printf(TEXT("call %s: unknown speaker '%s'"), *C.Key.ToString(), *C.Value.Speaker.ToString()));
		}
		if (!C.Value.On.IsEmpty() && !C.Value.On.StartsWith(TEXT("beat:")))
		{
			Out.Add(FString::Printf(TEXT("call %s: 'on' must be a beat (beat:X)"), *C.Key.ToString()));
		}
	}
	return Out;
}

bool USaltTalkSubsystem::Holds(const FString& Condition) const
{
	FString C = Condition;
	const bool bNot = C.RemoveFromStart(TEXT("!"));
	bool bHolds = false;
	UWorld* World = GetWorld();
	const ASaltPlayerCharacter* Player = Talker.IsValid() ? Talker.Get()
		: Cast<ASaltPlayerCharacter>(UGameplayStatics::GetPlayerPawn(World, 0));
	FString Arg;
	if (C.Split(TEXT(":"), &C, &Arg))
	{
		if (C == TEXT("beat"))
		{
			const USaltSaveSubsystem* Saves = USaltSaveSubsystem::Get(World);
			bHolds = Saves && Saves->HasBeat(FName(*Arg));
		}
		else if (C == TEXT("tool"))
		{
			bHolds = Player && Player->IsToolOwned(FName(*Arg));
		}
		else if (C == TEXT("item"))
		{
			const USaltInventoryComponent* Satchel = Player ? Player->GetInventory() : nullptr;
			bHolds = Satchel && Satchel->CountOf(FName(*Arg)) > 0;
		}
		else if (C == TEXT("day"))
		{
			const ASaltLightingRig* Rig = FirstActor<ASaltLightingRig>(World);
			bHolds = Rig && Rig->GetDay() >= FCString::Atoi(*Arg);
		}
	}
	else if (C == TEXT("tear"))
	{
		const ASaltBoat* Boat = FirstActor<ASaltBoat>(World);
		const USaltHullDamageComponent* Damage = Boat ? Boat->GetHullDamage() : nullptr;
		bHolds = Damage && Damage->GetSailTear() > 0;
	}
	else if (C == TEXT("night"))
	{
		const ASaltLightingRig* Rig = FirstActor<ASaltLightingRig>(World);
		bHolds = Rig && Rig->GetNightAmount() > 0.5f;
	}
	return bHolds != bNot;
}

bool USaltTalkSubsystem::HoldsAll(const TArray<FString>& Conditions) const
{
	for (const FString& C : Conditions)
	{
		if (!Holds(C))
		{
			return false;
		}
	}
	return true;
}

bool USaltTalkSubsystem::Begin(FName InSpeaker, ASaltPlayerCharacter* Player, const AActor* Speaker)
{
	const FSaltSpeaker* S = Speakers.Find(InSpeaker);
	if (!Player || !S)
	{
		UE_LOG(LogSaltbound, Warning, TEXT("SB_TALK no lines for speaker=%s"), *InSpeaker.ToString());
		return false;
	}
	if (IsTalking() && SpeakerId == InSpeaker)
	{
		return true;
	}
	Talker = Player;
	FName Entry;
	for (const FSaltTalkEntry& E : S->Entries)
	{
		if (HoldsAll(E.If) && Lines.Contains(E.Line))
		{
			Entry = E.Line;
			break;
		}
	}
	if (Entry.IsNone())
	{
		UE_LOG(LogSaltbound, Warning, TEXT("SB_TALK speaker=%s has nothing to say now"), *InSpeaker.ToString());
		return false;
	}
	if (Speaker)
	{
		SpeakerActors.Add(InSpeaker, const_cast<AActor*>(Speaker));
	}
	SpeakerId = InSpeaker;
	BeganAt = Player->GetActorLocation();
	SetInputActive(true);
	UE_LOG(LogSaltbound, Display, TEXT("SB_TALK begin speaker=%s"), *InSpeaker.ToString());
	Show(Entry);
	return true;
}

void USaltTalkSubsystem::Show(FName Line)
{
	const FSaltTalkLine* L = Lines.Find(Line);
	const FSaltSpeaker* S = L ? Speakers.Find(L->Speaker) : nullptr;
	if (!L)
	{
		End();
		return;
	}
	LineId = Line;
	LineAge = 0.f;
	bPending = false;
	for (const FString& A : L->Do)
	{
		Do(A);
	}
	// The numbered answers whose conditions hold, then the plain one; a line with none goes on with E.
	Shown.Reset();
	const FSaltTalkChoice* Plain = nullptr;
	int32 PlainIndex = INDEX_NONE;
	for (int32 I = 0; I < L->Choices.Num(); ++I)
	{
		const FSaltTalkChoice& C = L->Choices[I];
		if (!HoldsAll(C.If))
		{
			continue;
		}
		if (C.bPlain)
		{
			Plain = &C;
			PlainIndex = I;
		}
		else if (Shown.Num() < 3)
		{
			Shown.Add({ FString::FromInt(Shown.Num() + 1), C.Text, I });
		}
	}
	if (Plain)
	{
		Shown.Add({ TEXT("E"), Plain->Text, PlainIndex });
	}
	else
	{
		Shown.Add({ TEXT("E"), L->Next.IsNone() ? NSLOCTEXT("SaltTalk", "Farewell", "Farewell.") : NSLOCTEXT("SaltTalk", "GoOn", "Go on."), INDEX_NONE });
	}
	if (USaltTalkWidget* W = EnsureWidget())
	{
		W->ShowLine(S ? S->Name : FText::GetEmpty(), S ? S->Seal : NAME_None, L->Text, Shown);
		if (bInstant)
		{
			W->FinishWriting();
		}
	}
	UE_LOG(LogSaltbound, Display, TEXT("SB_TALK line=%s answers=%d"), *Line.ToString(), Shown.Num());
}

void USaltTalkSubsystem::Do(const FString& Action)
{
	FString Kind;
	FString Arg;
	if (!Action.Split(TEXT(":"), &Kind, &Arg))
	{
		return;
	}
	ASaltPlayerCharacter* Player = Talker.Get();
	if (Kind == TEXT("beat"))
	{
		if (USaltPrologueSubsystem* Prologue = GetWorld()->GetSubsystem<USaltPrologueSubsystem>())
		{
			Prologue->Reach(FName(*Arg));
		}
		else if (USaltSaveSubsystem* Saves = USaltSaveSubsystem::Get(GetWorld()))
		{
			Saves->SetBeat(FName(*Arg), 0.f);
		}
	}
	else if (Kind == TEXT("tool") && Player)
	{
		Player->SetToolOwned(FName(*Arg), true);
	}
	else if (Kind == TEXT("item") && Player && Player->GetInventory())
	{
		FString Item;
		FString Count = TEXT("1");
		if (!Arg.Split(TEXT(":"), &Item, &Count))
		{
			Item = Arg;
		}
		Player->GetInventory()->Add(FName(*Item), FMath::Max(1, FCString::Atoi(*Count)));
	}
	UE_LOG(LogSaltbound, Display, TEXT("SB_TALK do=%s"), *Action);
}

void USaltTalkSubsystem::Answer()
{
	if (!IsTalking() || bPending || (!bInstant && LineAge < AnswerGuard))
	{
		return;
	}
	if (Widget && Widget->IsWriting())
	{
		Widget->FinishWriting();
		return;
	}
	const int32 At = Shown.IndexOfByPredicate([](const FSaltShownChoice& C) { return C.Key == TEXT("E"); });
	if (At != INDEX_NONE)
	{
		Mark(At);
	}
}

void USaltTalkSubsystem::Choose(int32 Number)
{
	if (!IsTalking() || bPending || (!bInstant && LineAge < AnswerGuard))
	{
		return;
	}
	if (Widget && Widget->IsWriting())
	{
		Widget->FinishWriting();
		return;
	}
	const FString Key = FString::FromInt(Number + 1);
	const int32 At = Shown.IndexOfByPredicate([&Key](const FSaltShownChoice& C) { return C.Key == Key; });
	if (At != INDEX_NONE)
	{
		Mark(At);
	}
}

void USaltTalkSubsystem::Mark(int32 ShownIndex)
{
	if (Widget)
	{
		Widget->MarkChoice(ShownIndex);
	}
	PendingChoice = Shown[ShownIndex].Index;
	bPending = true;
	PendingIn = MarkTime;
	if (bInstant)
	{
		bPending = false;
		Take(PendingChoice);
	}
}

void USaltTalkSubsystem::Take(int32 ChoiceIndex)
{
	const FSaltTalkLine* L = GetLine();
	if (!L)
	{
		End();
		return;
	}
	const FName Next = L->Choices.IsValidIndex(ChoiceIndex) ? L->Choices[ChoiceIndex].Next : L->Next;
	UE_LOG(LogSaltbound, Display, TEXT("SB_TALK answer line=%s choice=%d next=%s"), *LineId.ToString(), ChoiceIndex, *Next.ToString());
	if (Next.IsNone())
	{
		End();
	}
	else
	{
		Show(Next);
	}
}

void USaltTalkSubsystem::End()
{
	if (!IsTalking())
	{
		return;
	}
	UE_LOG(LogSaltbound, Display, TEXT("SB_TALK end speaker=%s line=%s"), *SpeakerId.ToString(), *LineId.ToString());
	LineId = NAME_None;
	SpeakerId = NAME_None;
	Shown.Reset();
	bPending = false;
	if (Widget)
	{
		Widget->HideLine();
	}
	SetInputActive(false);
}

void USaltTalkSubsystem::Tick(float DeltaTime)
{
	if (!IsTalking())
	{
		return;
	}
	LineAge += DeltaTime;
	ASaltPlayerCharacter* Player = Talker.Get();
	ASaltPlayerController* PC = Controller();
	// Walking away, or a screen over the world (the satchel, a store, the title), ends the talk.
	if (!Player || FVector::Dist2D(Player->GetActorLocation(), BeganAt) > TalkReach
		|| (PC && (PC->IsSatchelOpen() || PC->IsStoreOpen() || PC->IsTitleOpen())))
	{
		End();
		return;
	}
	if (bPending)
	{
		PendingIn -= DeltaTime;
		if (PendingIn <= 0.f)
		{
			bPending = false;
			Take(PendingChoice);
		}
	}
}

ASaltPlayerController* USaltTalkSubsystem::Controller() const
{
	const ASaltPlayerCharacter* Player = Talker.Get();
	if (ASaltPlayerController* PC = Player ? Cast<ASaltPlayerController>(Player->GetController()) : nullptr)
	{
		return PC;
	}
	return Cast<ASaltPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0));
}

USaltTalkWidget* USaltTalkSubsystem::EnsureWidget()
{
	if (!Widget)
	{
		if (ASaltPlayerController* PC = Controller())
		{
			Widget = CreateWidget<USaltTalkWidget>(PC, USaltTalkWidget::StaticClass(), TEXT("SaltTalk"));
			if (Widget)
			{
				// Over the HUD (0), under the screens (10 and up).
				Widget->AddToViewport(3);
			}
		}
	}
	return Widget;
}

void USaltTalkSubsystem::EnsureInput()
{
	if (!TalkContext)
	{
		TalkContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Talk"));
		AnswerAction = NewObject<UInputAction>(this, TEXT("IA_TalkAnswer"));
		AnswerAction->ValueType = EInputActionValueType::Boolean;
		for (int32 I = 0; I < 3; ++I)
		{
			UInputAction* A = NewObject<UInputAction>(this, *FString::Printf(TEXT("IA_TalkChoose%d"), I + 1));
			A->ValueType = EInputActionValueType::Boolean;
			ChooseActions.Add(A);
		}
	}
	ASaltPlayerController* PC = Controller();
	UEnhancedInputComponent* Input = PC ? Cast<UEnhancedInputComponent>(PC->InputComponent) : nullptr;
	if (Input && BoundInput.Get() != Input)
	{
		Input->BindAction(AnswerAction, ETriggerEvent::Started, this, &USaltTalkSubsystem::HandleAnswer);
		for (int32 I = 0; I < ChooseActions.Num(); ++I)
		{
			Input->BindAction(ChooseActions[I], ETriggerEvent::Started, this, &USaltTalkSubsystem::HandleChoose, I);
		}
		BoundInput = Input;
	}
}

void USaltTalkSubsystem::SetInputActive(bool bActive)
{
	EnsureInput();
	ASaltPlayerController* PC = Controller();
	ULocalPlayer* LP = PC ? PC->GetLocalPlayer() : nullptr;
	UEnhancedInputLocalPlayerSubsystem* EI = LP ? LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	if (!EI || !TalkContext || bActive == bInputActive)
	{
		return;
	}
	if (bActive)
	{
		// The player's Use key (it may have been changed, D66) and the pad's X answer; 1-3 (and the pad's d-pad) choose.
		TalkContext->UnmapAll();
		TalkContext->MapKey(AnswerAction, USaltInputConfig::KeyOf(TEXT("Interact")));
		TalkContext->MapKey(AnswerAction, EKeys::Gamepad_FaceButton_Left);
		const FKey Keys[3][3] = { { EKeys::One, EKeys::NumPadOne, EKeys::Gamepad_DPad_Up }, { EKeys::Two, EKeys::NumPadTwo, EKeys::Gamepad_DPad_Right },
			{ EKeys::Three, EKeys::NumPadThree, EKeys::Gamepad_DPad_Down } };
		for (int32 I = 0; I < 3; ++I)
		{
			for (const FKey& K : Keys[I])
			{
				TalkContext->MapKey(ChooseActions[I], K);
			}
		}
		EI->AddMappingContext(TalkContext, TalkPriority);
	}
	else
	{
		EI->RemoveMappingContext(TalkContext);
	}
	bInputActive = bActive;
}

void USaltTalkSubsystem::HandleAnswer()
{
	Answer();
}

void USaltTalkSubsystem::HandleChoose(int32 Number)
{
	Choose(Number);
}

void USaltTalkSubsystem::SetSpeakerActor(FName Speaker, AActor* Actor)
{
	SpeakerActors.Add(Speaker, Actor);
}

const AActor* USaltTalkSubsystem::FindSpeakerActor(FName Speaker) const
{
	if (const TWeakObjectPtr<AActor>* A = SpeakerActors.Find(Speaker); A && A->IsValid())
	{
		return A->Get();
	}
	const FSaltSpeaker* S = Speakers.Find(Speaker);
	if (!S || S->PointId.IsNone())
	{
		return nullptr;
	}
	for (TActorIterator<ASaltHubPoint> It(GetWorld()); It; ++It)
	{
		if (It->PointId == S->PointId)
		{
			return *It;
		}
	}
	return nullptr;
}

bool USaltTalkSubsystem::Call(FName CallId)
{
	const FSaltTalkLine* C = Calls.Find(CallId);
	const FSaltSpeaker* S = C ? Speakers.Find(C->Speaker) : nullptr;
	const AActor* Where = C ? FindSpeakerActor(C->Speaker) : nullptr;
	const APawn* Player = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!C || !S || !Where || !Player)
	{
		UE_LOG(LogSaltbound, Warning, TEXT("SB_TALK call=%s has no speaker in the world"), *CallId.ToString());
		return false;
	}
	const double Distance = FVector::Dist(Player->GetActorLocation(), Where->GetActorLocation());
	if (C->Near > 0.f && Distance > C->Near)
	{
		UE_LOG(LogSaltbound, Display, TEXT("SB_TALK call=%s too far (%.0f m)"), *CallId.ToString(), Distance / 100.0);
		return false;
	}
	// Long enough to read: a moment, and a third of a second a word.
	TArray<FString> Words;
	const int32 WordCount = C->Text.ToString().ParseIntoArrayWS(Words);
	const float Seconds = C->Seconds > 0.f ? C->Seconds : 2.5f + 0.35f * WordCount;
	if (USaltTalkWidget* W = EnsureWidget())
	{
		W->ShowCall(S->Name, S->Seal, C->Text, Where->GetActorLocation() + FVector(0.0, 0.0, HeadUp), Seconds);
	}
	CallsMade.Add(CallId);
	UE_LOG(LogSaltbound, Display, TEXT("SB_TALK call=%s speaker=%s distance=%.0f m seconds=%.1f"), *CallId.ToString(), *C->Speaker.ToString(),
		Distance / 100.0, Seconds);
	return true;
}

void USaltTalkSubsystem::OnBeat(FName Beat)
{
	const FString On = TEXT("beat:") + Beat.ToString();
	for (const TPair<FName, FSaltTalkLine>& C : Calls)
	{
		if (C.Value.On == On && !CallsMade.Contains(C.Key))
		{
			Call(C.Key);
		}
	}
}
